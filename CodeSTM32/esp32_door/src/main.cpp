#include <Arduino.h>
#include <wifiConfig.h>
#include <HttpClient.h>
#define RXD2 16   // GPIO16 as RX2
#define TXD2 17   // GPIO17 as TX2
//#define led_wifi 2
#define wifiStatusPin 14
#define doorStatusPin 13
const char* serverName = "http://ldciot.id.vn/projects/dbiot/receive.php";  
void setup() {
  pinMode(wifiStatusPin,OUTPUT);
  pinMode(doorStatusPin,OUTPUT);
  digitalWrite(doorStatusPin,0);
  Serial.begin(115200);          // Serial monitor
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);  // UART2 init
  Serial.println("UART2 Reader started. Waiting for data...");
  wifiConfig.begin();
}
String data;
unsigned long timeSendHttp = 0; 
unsigned long timeGetHttp = 0; 
void loop() {
  wifiConfig.run();
  /// receive from stm32
  if(WiFi.status() == WL_CONNECTED){
    Serial.println("Co WIFI");
    if(millis() - timeGetHttp > 1000){
      timeGetHttp = millis();
      HttpClient http;
      http.begin(serverName);
      int httpCode = http.GET();

      if (httpCode == 200) {
        String response = http.getString();
        Serial.println("Server response: " + response);

        StaticJsonDocument<200> doc;
        DeserializationError err = deserializeJson(doc, response);
        if (!err) {
          String state = doc["led"];
          if (state == "on") {
            digitalWrite(doorStatusPin, HIGH);  // Mở cửa
            Serial.println("→ MỞ CỬA");
          } else {
            digitalWrite(doorStatusPin, LOW);   // Đóng cửa
            Serial.println("→ ĐÓNG CỬA");
          }
        }
      } else {
        Serial.printf("Lỗi kết nối HTTP: %d\n", httpCode);
      }
      http.end();
    }
    
    ///////////////////
    digitalWrite(wifiStatusPin,1);
    String received = "";
    unsigned long startTime = millis();
    while (millis() - startTime < 1500) { // 3-second timeout
      while (Serial2.available()) {
        char c = Serial2.read();
        received += c;
        if (c == '\n') {
          break;
        }
      }
      if (received.endsWith("\n")) {
        break; // complete line received
      }
    }
    if (received.length() > 0) {
      Serial.print("Received: ");
      Serial.print(received);  // includes newline
      if(received.indexOf(F("Data:")) != -1){		
        int startIndex = received.indexOf(F("Data:")) + 5;
        String data_temp = received.substring(startIndex);
        int sum=0;
        uint8_t checksum = (uint8_t)data_temp[12]; 
        Serial.print("du lieu check: ");
        Serial.println(checksum);
        for(int i=0;i<12;i++){
          sum+= (uint8_t)data_temp[i];
        }
        Serial.print("du lieu sum: ");
        Serial.println((uint8_t)sum);
        if((uint8_t)sum == checksum){
          data = data_temp.substring(0,12);
          Serial.print("du lieu post: ");
          Serial.println(data);
        }
      }
    } else {
      Serial.println("Timeout!");
    }
    /// http post
    if(data.length()>0){
        
      HttpClient http;

      http.begin(serverName);
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");

      // Dữ liệu cần gửi (ví dụ: sensor1=25.6)
      String postData = "sensor1=" + data +"&sensor2=99";

      int httpResponseCode = http.POST(postData);

      if (httpResponseCode > 0) {
        String response = http.getString(); // Phản hồi từ server
        Serial.println("Phản hồi từ server:");
        Serial.println(response);
        data  = "";
      } else {
        Serial.print("Lỗi gửi POST, mã lỗi: ");
        Serial.println(httpResponseCode);
      } 

      http.end();
    }
  } else{
    digitalWrite(wifiStatusPin,0);
    Serial.println("Lost WIFI");
    delay(10);
  }
  delay(50);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("→ MẤT WIFI. Đang thử kết nối lại...");
    // wifiConfig.begin(); // Gọi lại nếu cần, tùy theo thư viện bạn dùng
  } else {
    Serial.println("→ ĐANG KẾT NỐI WIFI");
  }
  Serial.print("WiFi Status: ");
  Serial.println(WiFi.status());
  Serial.print("WiFi Mode: ");
  Serial.println(wifiMode);
}
