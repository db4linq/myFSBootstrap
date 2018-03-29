#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h> 
#include <FS.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
// #include <Ticker.h> 
#define RELAY_PIN  D1
#define FIRMWARE_VERSION  "2008.0.1.5"
#define DBG_OUTPUT_PORT Serial

const String deviceId = "123456789";
const char* ssid = "sujin_seedum";
const char* password = "0891185329";
const char* host = "esp8266fs";
#define DHTTYPE   DHT22           // DHT type (DHT11, DHT22)
#define DHTPIN    D4  

unsigned long previousMillis = 0;     // will store last time LED was updated
// constants won't change :
const long interval = 1000 * 10; 
float t = 0;
float h = 0;

//Ticker flipper;
DHT_Unified dht(DHTPIN, DHTTYPE); 
ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(8001);

void toggle_status(){
  String jsonResponse = "{ \"id\": \""+ deviceId +"\", \"type\": 1, \"value\": " + String(digitalRead(RELAY_PIN)) + " }";
  webSocket.broadcastTXT(jsonResponse); 
}

void toggle() {
  digitalWrite(RELAY_PIN, !digitalRead(RELAY_PIN));
  toggle_status();
}
//format bytes
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path){
  DBG_OUTPUT_PORT.println("handleFileRead: " + path);
  if(path.endsWith("/")) path += "index.html";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    
    //if ((contentType != "text/html") || (contentType != "text/htm")) {
    //  bool isPublic = true;
    //  String cache = String(isPublic ? "public" : "private") +", max-age=" + String(86400) + ", must-revalidate";
    //  server.sendHeader("Cache-Control", cache);
    //}
    
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void read_tem() {
  
  sensors_event_t event;  
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println("Error reading temperature!"); 
  }
  else {
    t = event.temperature;
    Serial.print("Temperature: ");
    Serial.print(t);
    Serial.println(" *C");
  }
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println("Error reading humidity!");
  }
  else {
    h = event.relative_humidity;
    Serial.print("Humidity: ");
    Serial.print(h);
    Serial.println("%");
  }
  if ((isnan(event.relative_humidity)) && (isnan(event.temperature))) {
    return;
  }
  String json = "{";
    json += "\"type\": 2";
    json += ", \"temperature\": "+String(t);
    json += ", \"humidity\": "+String(h);
    json += "}"; 
  DBG_OUTPUT_PORT.println(json);
  webSocket.broadcastTXT(json.c_str(), json.length());
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:
            DBG_OUTPUT_PORT.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                DBG_OUTPUT_PORT.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
                //webSocket.sendTXT(num, json);
                String json = "{";
                  json += "\"type\": 2";
                  json += ", \"temperature\": "+String(t);
                  json += ", \"humidity\": "+String(h);
                  json += "}"; 
                DBG_OUTPUT_PORT.println(json);
                webSocket.broadcastTXT(json.c_str(), json.length());
                toggle_status();
            }
            break;
        case WStype_TEXT: {
            DBG_OUTPUT_PORT.printf("[%u] get Text: %s\n", num, payload); 
            // send message to client
            // webSocket.sendTXT(num, "message here"); 
            // send data to all connected clients
            // webSocket.broadcastTXT("message here");
            DynamicJsonBuffer jsonBuffer;
            JsonObject& root = jsonBuffer.parseObject(payload);
            if (root.success()) { 
              if (root["type"].as<int>() == 1) {
                toggle();
              }
            } else {
              String jsonResponse = "{ \"type\": 0, \"value\": \"invalid json command\" }";
              webSocket.broadcastTXT(jsonResponse);
            }
            break;
        }
        case WStype_BIN:
            DBG_OUTPUT_PORT.printf("[%u] get binary length: %u\n", num, length);
            hexdump(payload, length);

            // send message to client
            // webSocket.sendBIN(num, payload, length);
            break;
    } 
}

String indexProcessor(const String& key) {
  Serial.println(String("KEY IS ") + key);
  if (key == "VERSION") return FIRMWARE_VERSION;
  else if (key == "DEVICE_ID") return String(ESP.getChipId());

  return "oops";
}

void setup(void){
  DBG_OUTPUT_PORT.begin(115200);
  DBG_OUTPUT_PORT.print("\n");
  
  DBG_OUTPUT_PORT.setDebugOutput(true);
  SPIFFS.begin();
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {    
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      DBG_OUTPUT_PORT.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    DBG_OUTPUT_PORT.printf("\n");
  }
  pinMode(RELAY_PIN, OUTPUT);
  //WIFI INIT
  DBG_OUTPUT_PORT.printf("Connecting to %s\n", ssid);
  if (String(WiFi.SSID()) != String(ssid)) {
    WiFi.begin(ssid, password);
  }
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DBG_OUTPUT_PORT.print(".");
  } 
  DBG_OUTPUT_PORT.println("");
  DBG_OUTPUT_PORT.print("Connected! IP address: ");
  DBG_OUTPUT_PORT.println(WiFi.localIP());
  
  //SERVER INIT
  server.on("/", HTTP_GET, [](){
    if(!handleFileRead("/index.html")) server.send(404, "text/plain", "FileNotFound");
  });

  //get heap status, analog input value and all GPIO statuses in one json call
  server.on("/all", HTTP_GET, [](){
    String json = "{";
    json += "\"heap\":"+String(ESP.getFreeHeap());
    json += ", \"analog\":"+String(analogRead(A0));
    json += ", \"id\":\""+String(2)+"\"";
    json += "}";
    server.send(200, "application/json", json);
    json = String();
  });
  
  server.serveStatic("/", SPIFFS, "/", "max-age=86400");
   
  //called when the url is not defined here
  //use it to load content from SPIFFS
  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });

  server.begin();
  DBG_OUTPUT_PORT.println("HTTP server started");
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  DBG_OUTPUT_PORT.println("WebSocket server started");

  dht.begin();
  
  // flipper.attach(10, read_tem);
  read_tem();
}

void loop() {
  server.handleClient();
  webSocket.loop();

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;

    read_tem(); 
  }
}
