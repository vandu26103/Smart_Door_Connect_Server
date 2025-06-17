/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "rc522.h"
#include "string.h"
#include "stdio.h"
#include "stdarg.h"

#define openDoor 	__HAL_TIM_SET_COMPARE(&htim1,TIM_CHANNEL_1, 10);
#define closeDoor 	__HAL_TIM_SET_COMPARE(&htim1,TIM_CHANNEL_1, 20);
#define delayMs(x) HAL_Delay(x);


#define Dia_chi_LCD 0x4E
#define MAX_CARDS 10

#define MODE_LOCK 0x00
#define MODE_CHECK 0x01 
#define MODE_ADD 0x02
#define MODE_DEL 0x03
#define PW_CHANGE 0x04
#define MODE_ADD_ID 0x05
int MODE = 1;
uint32_t doorOpenTime = 0;
uint32_t lastFailTime  = 0;
uint8_t failedAttempts = 0; 
uint8_t lockoutActive = 0;
uint8_t lockTime = 1;
//check
char passValid[10];
char keyValid[10];
char numRfid;
//string
char HexString[20];
char M[50];
uint8_t tmp[15];
int idx = 0;
//rfid
uint8_t defaultKeyA[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t validKeyA[6] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5}; 
uint8_t cardSerial[5];
//keypad
uint32_t previousMillis = 0;
uint32_t currentMillis = 0;
uint8_t keyPressed = 1;
//btn
uint32_t btnPressTime = 0;
uint8_t btnPress = 0;
//servo
uint8_t doorIsOpen = 0;
uint8_t changeKeyPass = 0;
/* USER CODE END Includes */

 
/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c2;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C2_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void doorOpen();
void doorClose();
void doorOpenSlow(){
	for(int i=25;i>=10;i--){
		__HAL_TIM_SET_COMPARE(&htim1,TIM_CHANNEL_1, i);
		delayMs(40);
	}
}
void doorCloseSlow(){
	
	for(int i=10;i<=25;i++){
		__HAL_TIM_SET_COMPARE(&htim1,TIM_CHANNEL_1, i);
		delayMs(40);
	}
	
}
char h2c(char c){  
  return "0123456789ABCDEF"[0x0F & (unsigned char)c];
}
uint8_t updateHexString(uint8_t value, uint8_t index){
  HexString[index++]=h2c(value>>4);
  HexString[index++]=h2c(value&0x0F);
  return(index);
}
void Lcd_Ghi_Lenh (char lenh){
  char data_u, data_l;
	uint8_t data_t[4];
	data_u = (lenh&0xf0);
	data_l = ((lenh<<4)&0xf0);
	data_t[0] = data_u|0x0C;  //en=1, rs=0
	data_t[1] = data_u|0x08;  //en=0, rs=0
	data_t[2] = data_l|0x0C;  //en=1, rs=0
	data_t[3] = data_l|0x08;  //en=0, rs=0
	
	HAL_I2C_Master_Transmit (&hi2c2, Dia_chi_LCD,
                          (uint8_t *) data_t, 4, 1000);
	delayMs(10);
}
void Lcd_Ghi_Dulieu (char data){
	char data_u, data_l;
	uint8_t data_t[4];
	data_u = (data&0xf0);
	data_l = ((data<<4)&0xf0);
	data_t[0] = data_u|0x0D;  //en=1, rs=1
	data_t[1] = data_u|0x09;  //en=0, rs=1
	data_t[2] = data_l|0x0D;  //en=1, rs=1
	data_t[3] = data_l|0x09;  //en=0, rs=1
	HAL_I2C_Master_Transmit (&hi2c2, Dia_chi_LCD,
                         (uint8_t *) data_t, 4, 1000);
	delayMs(10);
}
void Lcd_init (void){
	Lcd_Ghi_Lenh (0x03); 
	delayMs(50);
	Lcd_Ghi_Lenh (0x02); 
	delayMs(50);
	Lcd_Ghi_Lenh (0x06); 
	delayMs(50);
	Lcd_Ghi_Lenh (0x0c); 
	delayMs(50);
	Lcd_Ghi_Lenh (0x28); 
	delayMs(50);
	Lcd_Ghi_Lenh (0x80);
	delayMs(50);
}
void Lcd_Ghi_Chuoi (char *chuoi){
	while(*chuoi)
	{
		Lcd_Ghi_Dulieu(*chuoi++);
	}
	
}
void LCD_SetCursor(uint8_t row, uint8_t col) {
    uint8_t address = (row == 0) ? 0x80 : 0xC0;
    address += col;
    Lcd_Ghi_Lenh(address);
}

void LCD_Print_1Line(char *chuoi){
	Lcd_Ghi_Lenh(0x80);
	Lcd_Ghi_Chuoi(chuoi);
	delayMs(5);
	Lcd_Ghi_Lenh(0xC0);
	Lcd_Ghi_Chuoi("                 ");
	delayMs(5);
}

void blinkBuzz(int solan, int thoigian){
	for(int i=0;i<solan;i++){
		HAL_GPIO_WritePin(GPIOB,GPIO_PIN_15,1);
		delayMs(thoigian);
		HAL_GPIO_WritePin(GPIOB,GPIO_PIN_15,0);
		delayMs(thoigian);
	}
}
void showError(){
	blinkBuzz(2,50);
	Lcd_Ghi_Lenh(0x80);
	Lcd_Ghi_Chuoi("Error!!!         ");
	delayMs(1000);
}
void openTheDoor(){
	if(doorIsOpen==0){
	doorIsOpen=1;
	failedAttempts = 0;
	blinkBuzz(2,40);
	doorOpenSlow();
//	openDoor;
	doorOpenTime = HAL_GetTick();
	}
}

void blinkLockMode(){
	blinkBuzz(1,70);
	sprintf(M,"Lock %ds!!!         ",5*lockTime);
	LCD_Print_1Line(M);
}


void printf0(char *buf){
	
	sprintf(M,"%s\r\n",buf);
	HAL_UART_Transmit(&huart1,(uint8_t*)M,strlen(M),100); 
}
void sendAT(char *cmd) {
  HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 1000);
}
int sendAT_Wait(char *cmd, char *expected, uint32_t timeout) {
  char buffer[200];
  memset(buffer, 0, sizeof(buffer));
  sendAT(cmd);
  uint32_t t0 = HAL_GetTick();
  uint16_t idx = 0;
  while (HAL_GetTick() - t0 < timeout) {
    uint8_t c;
    if (HAL_UART_Receive(&huart1, &c, 1, 100) == HAL_OK) {
      buffer[idx++] = c;
      if (strstr(buffer, expected) != NULL) return 0;
		}
	}
	return 1;
}

int send4G(char *data){
	if(sendAT_Wait("AT+CGATT=1\r\n", "OK", 500)==1) return 1;
	delayMs(3);
	if(sendAT_Wait("AT+CGDCONT=1,\"IP\",\"v-internet\"\r\n", "OK", 500)==1) return 1;
	delayMs(3);
	if(sendAT_Wait("AT+CGACT=1,1\r\n", "OK", 500) ==1) return 1;
	delayMs(3);
	if(sendAT_Wait("AT+HTTPINIT\r\n", "OK", 500) ==1) return 1;
	delayMs(3);
	if(sendAT_Wait("AT+HTTPPARA=\"URL\",\"http://ldciot.id.vn/projects/dbiot/receive.php\"\r\n", "OK", 500) ==1) return 2;
	delayMs(3);
	if(sendAT_Wait("AT+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\"\r\n", "OK", 500) ==1) return 2;
	delayMs(3);
	if(sendAT_Wait("AT+HTTPDATA=29,10000\r\n", "DOWN", 500) ==1) return 2;
	delayMs(3);
	sprintf(M,"sensor1=%s&sensor2=88\r\n",data);
	if(sendAT_Wait(M, "OK", 1000) ==1) return 2;
	delayMs(3);
	if(sendAT_Wait("AT+HTTPACTION=1\r\n", "OK", 1000) ==1) return 2;
	delayMs(100);
	if(sendAT_Wait("AT+HTTPTERM\r\n", "OK", 500) ==1) return 2;
	return 0;
}
int get4G(){
//	char buffer[100];
//  memset(buffer, 0, sizeof(buffer));
	int status =0;
	if(sendAT_Wait("AT+CGATT=1\r\n", "OK", 500)==1) return 1;
	delayMs(3);
	if(sendAT_Wait("AT+CGDCONT=1,\"IP\",\"v-internet\"\r\n", "OK", 500)==1) return 1;
	delayMs(3);
	if(sendAT_Wait("AT+CGACT=1,1\r\n", "OK", 500) ==1) return 1;
	delayMs(3);
	if(sendAT_Wait("AT+HTTPINIT\r\n", "OK", 500) ==1) return 1;
	delayMs(3);
	if(sendAT_Wait("AT+HTTPPARA=\"URL\",\"http://ldciot.id.vn/projects/dbiot/status.txt\"\r\n", "OK", 500) ==1) return 2;
	delayMs(3);
	if(sendAT_Wait("AT+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\"\r\n", "OK", 500) ==1) return 2;
	delayMs(3);
	if(sendAT_Wait("AT+HTTPACTION=0\r\n", "OK", 1000) ==1) return 2;
	delayMs(500);
//	sendAT("AT+HTTPREAD=0,3\r\n");
//	uint32_t t0 = HAL_GetTick();
//  uint16_t idx = 0;
	if(sendAT_Wait("AT+HTTPREAD=0,3\r\n", "on", 1000) ==0) {
		status = 10;
	} 
	delayMs(10);
	if(sendAT_Wait("AT+HTTPTERM\r\n", "OK", 500) ==1) return 2;
	return status;
}
//void send2Web(char *){
//	int sendStatus = send4G();
//	if(sendStatus ==2){
//		sendAT_Wait("AT+HTTPTERM\r\n", "OK", 1000)
//	} else
//}
char matkhau;
void simpleMap(uint8_t *input, int len, uint8_t *output) {
    for (int i = 0; i < len; i++) {
        output[i] = (input[i] ^ (i * 0x5A)) + 0x33;
    }
}
void changePassword(uint8_t *newPassword){
	if(changeKeyPass == 0){
			HAL_I2C_Mem_Write(&hi2c2,0xA1,0x08,2,newPassword,strlen((char *)newPassword),1000);
			delayMs(50);
		//	printf0("Doi mat khau thanh cong");
			memcpy(passValid, newPassword, sizeof((uint8_t*)newPassword));
	} else {
			HAL_I2C_Mem_Write(&hi2c2,0xA1,0x00,2,newPassword,strlen((char *)newPassword),1000);
			delayMs(50);
		//	printf0("Doi mat khau thanh cong");
			memcpy(keyValid, newPassword, sizeof((uint8_t*)newPassword));
			
	}
	idx = 0;
	memset(newPassword,'\0',15);
	MODE = MODE_CHECK; 
}

int isEnteringCode = 0;   // Cờ báo hiệu đang nhập mã định danh
int cardPendingSave = 0;  
char tempCode[5] = {0};   // Lưu mã định danh tạm
uint8_t codeIdx = 0;      // Vị trí nhập ký tự

int EEPROM_SaveNewCard(uchar *cardnum, char *code){
//	uint8_t numRFID;
//	HAL_I2C_Mem_Read(&hi2c2,0xA1,0x06,2,&numRFID,1,200);
//	HAL_Delay(10);
//	int baseAddr = 7 + (numRFID * 5) ;
//	numRFID++;
//	HAL_I2C_Mem_Write(&hi2c2,0xA0,0x06,2,&numRFID,1,200);
//	HAL_Delay(10);
//	HAL_I2C_Mem_Write(&hi2c2,0xA0,baseAddr,2,(uint8_t*)cardSerial,5,200);
//	HAL_Delay(10);
	
		uint8_t numRFID;
    if (HAL_I2C_Mem_Read(&hi2c2, 0xA1, 0x0F, 2, &numRFID, 1, 200) != HAL_OK) {
        return 0;
    }
    delayMs(20);

    int baseAddr = 16 + (numRFID * (4+4));
    numRFID++;
		numRfid = numRFID;
    if (HAL_I2C_Mem_Write(&hi2c2, 0xA0, 0x0F, 2, &numRFID, 1, 200) != HAL_OK) {
        return 0;
    }
    delayMs(20);
		///UID
    if (HAL_I2C_Mem_Write(&hi2c2, 0xA0, baseAddr, 2, (uint8_t*)cardSerial,4, 200) != HAL_OK) {
        return 0;
    }
    delayMs(20);
		///ID
		
		if (HAL_I2C_Mem_Write(&hi2c2, 0xA0, baseAddr + 4, 2, (uint8_t*)code, 4, 200) != HAL_OK) {
        return 0;
    }
		LCD_Print_1Line("Add card success! ");
		blinkBuzz(4,50);
		delayMs(1000);
    return 1;
		
}
////////////////////////////////////////////////////////////
typedef struct {
    uint8_t uid[4];
    char code[4];
} UserRecord;
uint8_t cardViewIndex = 0;
uint8_t totalCards = 0;
int isViewingCards = 0; // Cờ đang xem danh sách thẻ
int EEPROM_ReadCardByIndex(uint8_t index, UserRecord *card) {
    int baseAddr = 16 + (index * 8);
    if (HAL_I2C_Mem_Read(&hi2c2, 0xA1, baseAddr, 2, (uint8_t*)card, 8, 200) != HAL_OK)
        return 0;
    return 1;
}
UserRecord currentCard;
void showCard(UserRecord *card, uint8_t index) {
//    sprintf(M, "ID:%d UID:%02X %02X %02X %02X CODE:%c%c%c%c\r\n", index + 1,
//            card->uid[0], card->uid[1], card->uid[2], card->uid[3],
//            card->code[0], card->code[1], card->code[2], card->code[3]);
//    HAL_UART_Transmit(&huart1, (uint8_t*)M, strlen(M), 100);
		sprintf(M, "ID:%c%c%c%c.  %d/%d    ",card->code[0], card->code[1], card->code[2], card->code[3],index+1,numRfid);
		Lcd_Ghi_Lenh(0xC0);
		Lcd_Ghi_Chuoi(M);
}
void EEPROM_DeleteCardByIndex(uint8_t index) {
		uint8_t numRFID;
		HAL_I2C_Mem_Read(&hi2c2,0xA1,0x0F,2,&numRFID,1,200);
		int baseAddr = ((index+1) * 8) + 16 ; 
		int buffer_size = (16+((numRFID+1)*8)) - baseAddr;
		char buffer[buffer_size+1];
		uint8_t blank[8];
		memset(blank, 0xFF, 8);
		HAL_I2C_Mem_Read(&hi2c2,0xA1,baseAddr,2,(uint8_t*)buffer,buffer_size,2000);
		delayMs(50);
		HAL_I2C_Mem_Write(&hi2c2,0xA0,(baseAddr-8),2,(uint8_t*)buffer,buffer_size,2000);
		delayMs(50);
		HAL_I2C_Mem_Write(&hi2c2,0xA0,baseAddr,2,(uint8_t*)blank,8,2000);
		delayMs(50);
		numRFID--;
		numRfid = numRFID;
		HAL_I2C_Mem_Write(&hi2c2,0xA0,0x0F,2,&numRFID,1,200);
		delayMs(20);
}
/////////////////////////////////////////////////////////////////
void mode_changePass(){
	sprintf(M, "New PW:%s            \r\n", tmp);
	Lcd_Ghi_Lenh(0xC0);
	Lcd_Ghi_Chuoi(M);
}
void checkkey(uint8_t *bf) {
	

	
	
	if (keyPressed != 1) {
		if(MODE == MODE_LOCK){
			delayMs(1);
		} 
		else if (MODE == MODE_DEL && !isEnteringCode) {
				if (!isViewingCards) {
						HAL_I2C_Mem_Read(&hi2c2, 0xA1, 0x0F, 2, &totalCards, 1, 100);
						if (totalCards == 0) {
		//            printf0("Khong co the nao trong EEPROM\r\n");
							LCD_Print_1Line("No card!!      ");
							blinkBuzz(2,50);
							delayMs(1000);
							keyPressed = 1;
							MODE = MODE_CHECK;
							return;
						}
						cardViewIndex = 0;
						EEPROM_ReadCardByIndex(cardViewIndex, &currentCard);
						showCard(&currentCard, cardViewIndex);
						isViewingCards = 1;
						return;
				}

				switch (keyPressed) {
						case 'A':
								if (cardViewIndex > 0) {
										cardViewIndex--;
										EEPROM_ReadCardByIndex(cardViewIndex, &currentCard);
										showCard(&currentCard, cardViewIndex);
								}
								break;
						case 'B':
								if (cardViewIndex < totalCards - 1) {
										cardViewIndex++;
										EEPROM_ReadCardByIndex(cardViewIndex, &currentCard);
										showCard(&currentCard, cardViewIndex);
								} 
								break;
						case 'C':
								EEPROM_DeleteCardByIndex(cardViewIndex);
								blinkBuzz(4,50);
								Lcd_Ghi_Lenh(0x80);
								Lcd_Ghi_Chuoi("Delete success!!  **    ");
								delayMs(1000);
		//            printf0("Da xoa the\r\n");
								HAL_I2C_Mem_Read(&hi2c2, 0xA1, 0x0F, 2, &totalCards, 1, 100);
								if (totalCards == 0) {
		//                printf0("Danh sach rong\r\n");
										MODE = MODE_CHECK;
										isViewingCards = 0;
										return;
								}
								if (cardViewIndex >= totalCards) cardViewIndex = totalCards - 1;
								EEPROM_ReadCardByIndex(cardViewIndex, &currentCard);
								showCard(&currentCard, cardViewIndex);
								break;
						case '#':
		//            printf0("Thoat che do xoa the\r\n");
								MODE = MODE_CHECK;
								isViewingCards = 0;
								break;
				}
				keyPressed = 1;
				//return;
			}
		else if (MODE == MODE_ADD_ID && isEnteringCode) {
			if(keyPressed == 'C'){
				if (codeIdx > 0) {
					codeIdx--;
					tempCode[codeIdx] = '\0'; 
					
				} 
				sprintf(M, "ID: %s             \r\n", tempCode);
				Lcd_Ghi_Lenh(0xC0);
				Lcd_Ghi_Chuoi(M);
				keyPressed = 1;
				return;
			} 
			else if (keyPressed!='#' && codeIdx < 4) {
				tempCode[codeIdx++] = keyPressed;
				sprintf(M, "ID: %s             \r\n", tempCode);
				Lcd_Ghi_Lenh(0xC0);
				Lcd_Ghi_Chuoi(M);
				keyPressed = 1;
				return;
			}
			else if (keyPressed == '#') {
					tempCode[4] = '\0';
					if (codeIdx == 4 && cardPendingSave) {
						if (EEPROM_SaveNewCard(cardSerial, tempCode)) {
	//						printf0("Da luu UID + ma dinh danh vao EEPROM\r\n");
							isEnteringCode = 0;
							cardPendingSave = 0;
							codeIdx = 0;
							memset(tempCode, 0, 5);
							MODE = MODE_ADD;
						} else {
	//							printf0("Luu that bai!\r\n");
								codeIdx = 0;
								memset(tempCode, 0, 5);
						}
					} else {
	//            printf0("Ma khong hop le\r\n");
							codeIdx = 0;
							memset(tempCode, 0, 5);
					}

					
					keyPressed = 1;
					return;
			} else {
	//        printf0("Chi duoc nhap so\r\n");
					keyPressed = 1;
					return;
			}
		} 
		else if(MODE == PW_CHANGE){
			if(keyPressed == 'C'){
				if (idx > 0) {
					idx--;
					bf[idx] = '\0'; 
				} 
			}
			else if((keyPressed >'0' && keyPressed <='9') || keyPressed =='#'){
				if (changeKeyPass ==0 ) {
					if(keyPressed!='#' && idx<6){
						bf[idx++] = keyPressed;
						keyPressed = 1;
					} else if(keyPressed =='#' && idx>=6){
						bf[idx] = '\0';
						keyPressed = 1;
						changePassword(bf);
					}
				} else {
					if(keyPressed!='#' && idx<8){
						bf[idx++] = keyPressed;
						keyPressed = 1;
					} else if(keyPressed =='#' &&idx>=8){
						bf[idx] = '\0';
						keyPressed = 1;
						changePassword(bf);
					}
				}
			}
		}
		else {
			char M0[13];
			char M1[13];
			char M2[13];
			char M3[13];
			if (keyPressed == '#') {
					if (idx > 0 && idx<14) {
							bf[idx] = '\0';
							keyPressed = 1;
//							if(MODE == PW_CHANGE){
//								changePassword(bf);
//							} else {
								sprintf(M0,"%s*100",keyValid);
								sprintf(M1,"%s*101",keyValid);
								sprintf(M2,"%s*102",keyValid);
								sprintf(M3,"%s*103",keyValid);
								
								if (strcmp((char*)bf, passValid) == 0 || strcmp((char*)bf, keyValid) == 0) {
									
										LCD_Print_1Line("Welcome...         ");
										openTheDoor();
										lockTime = 1;
										if(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_1)==1){///esp wifi
//											sprintf(M, "Data:FFFFFFFFFFFFFF\n");
											sprintf(M, "Data:FFFFFFFFFFFF%c\n", 0x48);
											HAL_UART_Transmit(&huart2, (uint8_t*)M, strlen(M), 100);
										} else {///4G
											sendAT_Wait("AT+HTTPTERM\r\n", "OK", 1000);
											char buffer4G[13] = "FFFFFFFFFFFF";
											buffer4G[12] = '\0';
											int send = send4G(buffer4G);
											if(send==2){
												sendAT_Wait("AT+HTTPTERM\r\n", "OK", 1000);
											}
										}		
										delayMs(1000);
								} 
								else if (strcmp((char*)bf, M2) == 0) {
//									printf0("Vao che do them the\r\n");
										lockTime = 1;
										MODE = MODE_ADD;
								}  else if (strcmp((char*)bf, M3) == 0) {
//										printf0("Vao che do xoa the\r\n");
										lockTime = 1;
										MODE = MODE_DEL;  
										keyPressed = 'B';
//										NVIC_SetPendingIRQ(EXTI9_5_IRQn);
										checkkey(tmp);
										
								} else if (strcmp((char*)bf,M1) == 0) {
//										printf0("Vao che do doi mat khau\r\n");
										lockTime = 1;
										LCD_Print_1Line("Enter new PW!   ");
										memset(bf,'\0',15);
										changeKeyPass = 0;
										MODE = PW_CHANGE; 
								}
								else if (strcmp((char*)bf,M0) == 0) {
//										printf0("Vao che do doi mat khau\r\n");
										lockTime = 1;
										LCD_Print_1Line("Enter new key PW!   ");
										memset(bf,'\0',15);
										changeKeyPass = 1;
										MODE = PW_CHANGE; 
								}
								else {
//										printf0("Mat khau khong hop le\r\n");
									blinkBuzz(2,50);
									LCD_Print_1Line("Enter again!!     ");
									delayMs(1000);
									failedAttempts++;
									if (failedAttempts >= 5) {
										lockoutActive = 1;
										if(lockTime < 5) lockTime++;
										lastFailTime = HAL_GetTick();
//										printf0("Nhap sai 5 lan. Khoa 10s!\r\n");
										MODE = MODE_LOCK;
									}
								}
//							}
							
					}
					else if(idx==0){
						MODE = MODE_CHECK; 
					}
					idx = 0;
					memset(bf, '\0', 15);
			}else if (keyPressed == 'C') {
					if (idx > 0) {
							idx--;
							bf[idx] = '\0'; 
					} 
					keyPressed = 1;
			} else {
					bf[idx++] = keyPressed;
					keyPressed = 1;
			}
		}
	}
}


int hasCharacter(uint8_t *str) {
    if (str == NULL) return 0; // n?u chu?i null
    return str[0] != '\0';          // true n?u c� �t nh?t 1 k� t?
}

void RFID_addCard(void)
{		
		
    uchar status;
    uchar type[2];
		uchar newKeyBlock[16] = {
			0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5,   // New Key A
			0xFF, 0x07, 0x80, 0x69,               // Access bits + check byte
			0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5    // New Key B
		};
		Lcd_Ghi_Lenh(0xC0);
		Lcd_Ghi_Chuoi("                         ");
		delayMs(100);
		Lcd_Ghi_Lenh(0x80);
		Lcd_Ghi_Chuoi("Add card...     ");
		
    status = MFRC522_Request(PICC_REQALL, type);
    if (status != MI_OK) return;

    status = MFRC522_Anticoll(cardSerial);
    if (status != MI_OK) return;
		
    MFRC522_SelectTag(cardSerial);
    
		status = MFRC522_Auth(PICC_AUTHENT1A, 7, defaultKeyA, cardSerial);
    if (status != MI_OK) {
			showError();
			return;
		}
//    block 7
    status = MFRC522_Write(7, newKeyBlock);
    if (status != MI_OK){
//        printf0("Ghi key that bai\n");
        return;
    }
//    printf0("Ghi key moi thanh cong vao sector 1\n");
		isEnteringCode = 1;
    codeIdx = 0;
    memset(tempCode, 0, 5);
    cardPendingSave = 1;
		Lcd_Ghi_Lenh(0x80);
		Lcd_Ghi_Chuoi("Nhap ID...          ");
		MODE = MODE_ADD_ID;
		blinkBuzz(1,50);
		//Add id to eeprom
//		if(EEPROM_SaveNewCard(cardSerial)==0){
//			return;
//		}
		//print lcd
		
		
		ClearBitMask(Status2Reg, 0x08);
    MFRC522_Halt();
//		MFRC522_Reset();
//		HAL_Delay(100);
//		MFRC522_Init();
}
int adc;
int get;
void RFID_checkDoor(void)
{		
    uchar status;
    uchar type[2];
//    printf0("Kiem tra the\n");
		
		Lcd_Ghi_Lenh(0x80);
		Lcd_Ghi_Chuoi("Password | Rfid     ");
		sprintf(M, "PW:%s             ", tmp); 
		Lcd_Ghi_Lenh(0xC0);
		Lcd_Ghi_Chuoi(M);
		
    status = MFRC522_Request(PICC_REQALL, type);
    if (status != MI_OK) 
		{
//			printf0("Khong requesst duoc ");
			return;
		}
    status = MFRC522_Anticoll(cardSerial);
    if (status != MI_OK) {
//			printf0("Khong Anticoll duoc ");
			return;
		}
	
    MFRC522_SelectTag(cardSerial);
    // X�c th?c b?ng key m?i
    status = MFRC522_Auth(PICC_AUTHENT1A, 7, validKeyA, cardSerial);
    if (status == MI_OK){
				for(int i=0;i<numRfid;i++){
					int baseAddr = (i * 8) + 16  ; 
					uint8_t cardCheck[10];
					HAL_I2C_Mem_Read(&hi2c2,0xA1,baseAddr,2,(uint8_t*)cardCheck,8,200);
					delayMs(20);
					if(memcmp(cardSerial, cardCheck, 4) == 0){
//						printf0("The co trong eeprom\n");
						//open door
						sprintf(M, "Welcome: %c%c%c%c    ",cardCheck[4], cardCheck[5], cardCheck[6], cardCheck[7]);
						Lcd_Ghi_Lenh(0x80);
						Lcd_Ghi_Chuoi(M);
						openTheDoor();
						lockTime = 1;
						/// esp
						uint8_t sum=0;
						uint8_t index = 0;
						for(int i=0;i<4;i++){
							index = updateHexString(cardCheck[i],index);
						}
						for(int i=4;i<8;i++){
							HexString[index++] = cardCheck[i];
						}
//						
						for(int i=0;i<12;i++){
							sum+=HexString[i];
						}
						HexString[index++] = (uint8_t)sum;
						HexString[index++] = '\n';
						HexString[index++] = '\0';
						if(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_1)==1){///esp wifi
							sprintf(M, "Data:%s\n", HexString);
							HAL_UART_Transmit(&huart2, (uint8_t*)M, strlen(M), 100);
						} else {///4G
							sendAT_Wait("AT+HTTPTERM\r\n", "OK", 1000);
							HexString[12] = '\0';
							int send = send4G(HexString);
							if(send==2){
								sendAT_Wait("AT+HTTPTERM\r\n", "OK", 1000);
							} 
						}							
////						else {
////							sprintf(M,"Ma loi: %d             ",send);
////							LCD_Print_1Line(M);
////						}
						delayMs(2000);
					}
				}
//				printf0("The dung ma xac thuc\n");
        
    }
    else{
				LCD_Print_1Line("Card not valid!!     ");
				blinkBuzz(2,80);
				
//        printf0("The khong hop le hoac sai key\n");
				failedAttempts++;
				if (failedAttempts >= 5) {
						lockoutActive = 1;
						if(lockTime < 5) lockTime++;
						MODE = MODE_LOCK;
						lastFailTime = HAL_GetTick();
//						printf0("Sai the 5 lan. Tam khoa 10s\r\n");
				} else {
					delayMs(2000);
				}
				
    }

    ClearBitMask(Status2Reg, 0x08);
    MFRC522_Halt();
//		MFRC522_Reset();
//		HAL_Delay(100);
//		MFRC522_Init();
}

void RFID_deleteCard(void)
{
    uchar status;
    uchar type[2];
    uchar defaultKeyBlock[16] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,   
        0xFF, 0x07, 0x80, 0x69,              
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF    
    };
		Lcd_Ghi_Lenh(0x80);
		Lcd_Ghi_Chuoi("Delete card....    ");
    status = MFRC522_Request(PICC_REQALL, type);
    if (status != MI_OK) return;

    status = MFRC522_Anticoll(cardSerial);
    if (status != MI_OK) return;
		
    MFRC522_SelectTag(cardSerial);

    status = MFRC522_Auth(PICC_AUTHENT1A, 7, validKeyA, cardSerial);
    if (status != MI_OK) 
		{
			showError();
			return;
		}
   
    status = MFRC522_Write(7, defaultKeyBlock);
    if (status == MI_OK) {
//        printf0("Xoa the thanh cong (key ve mac dinh)\n");
				for(int i=0;i<MAX_CARDS;i++){
					int baseAddr = (i * 8) + 16  ; 
					uint8_t cardCheck[7];
					HAL_I2C_Mem_Read(&hi2c2,0xA1,baseAddr,2,(uint8_t*)cardCheck,4,200);
					delayMs(20);
					if(memcmp(cardSerial, cardCheck, 4) == 0){
						EEPROM_DeleteCardByIndex(i);
						LCD_Print_1Line("Delete success!!      ");
					}
					
				}
				blinkBuzz(4,50);
    } else {
				showError();
//        printf0("Xoa the that bai\n");
    }
		ClearBitMask(Status2Reg, 0x08);
    MFRC522_Halt();
//		MFRC522_Reset();
//		HAL_Delay(100);
//		MFRC522_Init();
}

char keyPassword[10]= "12345678";
char openPassword[10]= "123456";
char test[20];
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
uint32_t timeSIMGet = 0;
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C2_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
	
	///RFID
	MFRC522_Init();
	delayMs(50);
	
	/// en wifi
	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_12,0);
	delayMs(500);
	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_12,1);
	
	
	///SERVO
	HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_1);
	delayMs(100);
	if(25 - __HAL_TIM_GET_COMPARE(&htim1,TIM_CHANNEL_1) > 2) doorCloseSlow();
	
//	closeDoor;
	///LCD
	

	////EEPROM
	//GHI mat khau..
//	HAL_I2C_Mem_Write(&hi2c2,0xA0,0x00,2,&keyPassword,8,500);
//	delayMs(10);
//	HAL_I2C_Mem_Write(&hi2c2,0xA0,0x08,2,&openPassword,6,500);
//	delayMs(10);
//	HAL_I2C_Mem_Write(&hi2c2,0xA0,0x0F,2,&num,1,200);
//	delayMs(10);
	
	//ktra mat khau
	HAL_I2C_Mem_Read(&hi2c2,0xA1,0x08,2,(uint8_t*)passValid,6,1000);
	delayMs(50);
	HAL_I2C_Mem_Read(&hi2c2,0xA1,0x00,2,(uint8_t*)keyValid,8,1000);
	delayMs(50);
//	sprintf(M, "MKhau:%s\r\n", passValid);
//	HAL_UART_Transmit(&huart1, (uint8_t*)M, strlen(M), 100);
	//ktra so the tu
	HAL_I2C_Mem_Read(&hi2c2,0xA1,0x0F,2,&numRfid,1,1000);
	delayMs(20);
//	sprintf(M, "So the tu:%d\r\n",numRfid );
//	HAL_UART_Transmit(&huart1, (uint8_t*)M, strlen(M), 100);
	
	//KEYPAD
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, 1);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, 1);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, 1);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, 1);
	//SIM
	delayMs(100);
	
	sendAT_Wait("ATE0\r\n", "OK", 1000);
	//LCD
	Lcd_init();
	Lcd_Ghi_Lenh(0x01);
	delayMs(100);
	Lcd_Ghi_Lenh(0x80);
	Lcd_Ghi_Chuoi("Hello");
	idx=0;
	tmp[0] = ' ';
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		//% pin
//		HAL_ADC_Start(&hadc1); 
//		HAL_ADC_PollForConversion(&hadc1,10);
//		adc=HAL_ADC_GetValue(&hadc1);
		
		
		
		delayMs(50);
		HAL_GPIO_TogglePin(GPIOC,GPIO_PIN_13);
		
		if(MODE == MODE_ADD) {
			RFID_addCard();
		} 
		else if (MODE == MODE_DEL){
			RFID_deleteCard();
		} 
		else if(MODE == MODE_CHECK){
			RFID_checkDoor();
			//get http dkhien cua
			if(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_1)==1){// wifi
				if(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_14)==1){
					LCD_Print_1Line("Welcome...         ");
					openTheDoor();
					sprintf(M, "Data:000000000000%c\n", 0x40);
					HAL_UART_Transmit(&huart2, (uint8_t*)M, strlen(M), 100);
					delayMs(5000);
				}
			} else if(HAL_GetTick() - timeSIMGet >=2500){
				//ktra get sim
				timeSIMGet = HAL_GetTick();
				get = get4G();
				if(get == 10){
					LCD_Print_1Line("Welcome...         ");
					openTheDoor();
					int send = send4G("000000000000");
					if(send==2){
						sendAT_Wait("AT+HTTPTERM\r\n", "OK", 500);
					} 
					//delayMs(2000);
				} else if(get == 2){
					sendAT_Wait("AT+HTTPTERM\r\n", "OK", 500);
				}
			}
		} 
		else if(MODE== PW_CHANGE){
			mode_changePass();
		} 
		else if(MODE == MODE_LOCK){
			blinkLockMode();
		}
		
		///btn reset password
		if(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_13) ==0){
			if(!btnPress){
				btnPressTime= HAL_GetTick();
				btnPress = 1;
			}else {
				if(HAL_GetTick() - btnPressTime >=5000){
						btnPress = 0;
						HAL_I2C_Mem_Write(&hi2c2,0xA0,0x00,2,(uint8_t*)"12345678",8,500);
						delayMs(50);
						HAL_I2C_Mem_Write(&hi2c2,0xA0,0x08,2,(uint8_t*)"123456",6,500);
						delayMs(50);
						blinkBuzz(4,70);
				}
			}
		} else btnPress = 0;
		
		
		
		if ( (HAL_GetTick() - doorOpenTime >= 5000) && doorIsOpen==1) {///time lock door
//			closeDoor;
			doorCloseSlow();
			doorIsOpen =0;
		}
		
		if (lockoutActive) {
			if (HAL_GetTick() - lastFailTime >= 5*lockTime*1000) { ///time lock active key
				lockoutActive = 0;
				failedAttempts = 0;
				HAL_GPIO_WritePin(GPIOB,GPIO_PIN_15,0);
				MODE = MODE_CHECK;
//				printf0("Da mo khoa nhap lai\r\n");
			} 
		}
		
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV8;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 7199;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 199;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0 ;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 25;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_15|GPIO_PIN_3
                          |GPIO_PIN_4|GPIO_PIN_5, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_12,GPIO_PIN_SET);
  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA4 PA15 */
  GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB14 PB15 PB3
                           PB4 PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_15|GPIO_PIN_3
                          |GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB1 PB12 PB13 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB6 PB7 PB8 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);



	///
	GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	
	
  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

	HAL_NVIC_SetPriority(EXTI15_10_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void EXTI15_10_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI15_10_IRQn 0 */
	openTheDoor();
  /* USER CODE END EXTI15_10_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13);
  /* USER CODE BEGIN EXTI15_10_IRQn 1 */

  /* USER CODE END EXTI15_10_IRQn 1 */
}
void EXTI9_5_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI9_5_IRQn 0 */
	//HAL_GPIO_WritePin(GPIOA,GPIO_PIN_11,1);
  /* USER CODE END EXTI9_5_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_6);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_7);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_8);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_9);
  /* USER CODE BEGIN EXTI9_5_IRQn 1 */

	
	//HAL_GPIO_WritePin(GPIOA,GPIO_PIN_11,0);
  /* USER CODE END EXTI9_5_IRQn 1 */
}
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	GPIO_InitTypeDef GPIO_InitStructPrivate  = {0};
  currentMillis = HAL_GetTick();
  if (currentMillis - previousMillis > 250) {
    /*Configure GPIO pins : PB6 PB7 PB8 PB9 to GPIO_INPUT*/
    GPIO_InitStructPrivate.Pin = GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9;
    GPIO_InitStructPrivate.Mode = GPIO_MODE_INPUT;
    GPIO_InitStructPrivate.Pull = GPIO_PULLDOWN; ////pulldown --nopull
    GPIO_InitStructPrivate.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStructPrivate);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, 1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, 0);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, 0);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, 0);
    if(GPIO_Pin == GPIO_PIN_6 && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6))
    {
      keyPressed = 68; //ASCII value of D
    }
    else if(GPIO_Pin == GPIO_PIN_7 && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7))
    {
      keyPressed = 67; //ASCII value of C
    }
    else if(GPIO_Pin == GPIO_PIN_8 && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8))
    {
      keyPressed = 66; //ASCII value of B
    }
    else if(GPIO_Pin == GPIO_PIN_9 && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9))
    {
      keyPressed = 65; //ASCII value of A
    }

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, 0);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, 1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, 0);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, 0);
    if(GPIO_Pin == GPIO_PIN_6 && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6))
    {
      keyPressed = 35; //ASCII value of #
    }
    else if(GPIO_Pin == GPIO_PIN_7 && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7))
    {
      keyPressed = 57; //ASCII value of 9
    }
    else if(GPIO_Pin == GPIO_PIN_8 && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8))
    {
      keyPressed = 54; //ASCII value of 6
    }
    else if(GPIO_Pin == GPIO_PIN_9 && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9))
    {
      keyPressed = 51; //ASCII value of 3
    }

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, 0);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, 0);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, 1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, 0);
    if(GPIO_Pin == GPIO_PIN_6 && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6))
    {
      keyPressed = 48; //ASCII value of 0
    }
    else if(GPIO_Pin == GPIO_PIN_7 && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7))
    {
      keyPressed = 56; //ASCII value of 8
    }
    else if(GPIO_Pin == GPIO_PIN_8 && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8))
    {
      keyPressed = 53; //ASCII value of 5
    }
    else if(GPIO_Pin == GPIO_PIN_9 && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9))
    {
      keyPressed = 50; //ASCII value of 2
    }

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, 0);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, 0);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, 0);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, 1);
    if(GPIO_Pin == GPIO_PIN_6 && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_6))
    {
      keyPressed = 42; //ASCII value of *
    }
    else if(GPIO_Pin == GPIO_PIN_7 && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7))
    {
      keyPressed = 55; //ASCII value of 7
    }
    else if(GPIO_Pin == GPIO_PIN_8 && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8))
    {
      keyPressed = 52; //ASCII value of 4
    }
    else if(GPIO_Pin == GPIO_PIN_9 && HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9))
    {
      keyPressed = 49; //ASCII value of 1
    }
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, 1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, 1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, 1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, 1);
    /*Configure GPIO pins : PB6 PB7 PB8 PB9 back to EXTI*/
    GPIO_InitStructPrivate.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStructPrivate.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStructPrivate);
    previousMillis = currentMillis;
		
		if(keyPressed>30 &&keyPressed<70){
			blinkBuzz(1,40);
			checkkey(tmp);
		}
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
