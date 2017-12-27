/*
 * Copyright (c) 2017 Alien Robot Army
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266SSDP.h>
#include <WiFiManager.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <SimpleDHT.h>
#include <FS.h>

#define DHTPIN D2
#define ACTPIN D5
#define PIRPIN D7
#define DHTTYPE DHT11

SimpleDHT11 dht;
ESP8266WebServer HTTP(80);
WiFiClient espClient;
PubSubClient client(espClient);

bool shouldSaveConfig = false;
char mqtt_server[40] = "";
char mqtt_port[6] = "1883";
char label[255];


byte h, f;
int lightValue;
int pirState;
String StationName;


//callback notifying us of the need to save config
void saveConfigCallback()
{
    Serial.println("Should save config");
    shouldSaveConfig = true;
}

byte CtoF(byte i) {
  return i * 9/5 + 32;
}

void handleReboot() {
  HTTP.send(200, "text/plain",  "Rebooting.\r\n");
  ESP.reset();
}
void handleReset() {
  HTTP.send(200, "text/plain",  "Resetting. Please connect to Access Point\r\n");
  if (SPIFFS.begin()) {
    SPIFFS.remove("/config.txt");
  } else {
    Serial.println("SPIFFS error removing /config.txt");
  }
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  ESP.reset();
}
void handleBlink() {
  /* Helps you identify what device this is */
  HTTP.send(200, "text/plain",  "Blinking for 30 seconds\r\n");
  for (int i = 0; i < 30; i++) {
    digitalWrite(ACTPIN, HIGH);
    delay(500);
    digitalWrite(ACTPIN, LOW);
    delay(500);
  }
}
void handleTemperature() {
  digitalWrite(ACTPIN, HIGH);
  IPAddress ip = WiFi.localIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      String s = "{ ";
      s += "\"Label\": \"";
      s += label;
      s += "\", ";
      s += "\"Address\": \"";
      s += ipStr;
      s += "\", ";
      s += "\"Temperature\": ";
      s += CtoF(f);
      s += ", \"Humidity\": ";
      s += h;
      s += " }\r\n";
  HTTP.send(200, "application/json", s);
  digitalWrite(ACTPIN, LOW);
}
void handleLight() {
  digitalWrite(ACTPIN, HIGH);
  IPAddress ip = WiFi.localIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      String s = "{ ";
      s += "\"Label\": \"";
      s += label;
      s += "\", ";
      s += "\"Address\": \"";
      s += ipStr;
      s += "\", ";
      s += "\"Lumens\": ";
      s += lightValue;
      s += " }\r\n";
  HTTP.send(200, "application/json", s);
  digitalWrite(ACTPIN, LOW);
}
void handleMotion() {
  digitalWrite(ACTPIN, HIGH);
  IPAddress ip = WiFi.localIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      String s = "{ ";
      s += "\"Label\": \"";
      s += label;
      s += "\", ";
      s += "\"Address\": \"";
      s += ipStr;
      s += "\", ";
      s += "\"Motion\": ";
      s += (pirState == HIGH) ? "ON" : "OFF";
      s += " }\r\n";
  HTTP.send(200, "application/json", s);
  digitalWrite(ACTPIN, LOW);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.print("Waiting on DHT11 ");
  delay(1000);
  
  Serial.println("done");
  Serial.println("Starting WiFi...");

  pinMode(ACTPIN, OUTPUT);
  digitalWrite(ACTPIN, LOW);


    //read configuration from FS json
    Serial.println("mounting FS...");

    if (SPIFFS.begin()) {
      //SPIFFS.format();
      Serial.println("mounted file system");
      File f = SPIFFS.open("/config.txt", "r");

      if (f) {
        // we could open the file
        while (f.available()) {
          //Lets read line by line from the file
          String line = f.readStringUntil('\n');
          String Key = line.substring(0,3);
          String Value = line.substring(4);
          Value.trim();
          if (Key == "mqs") {
            strcpy(mqtt_server, Value.c_str());
          }
          if (Key == "mqp") {
            strcpy(mqtt_port, Value.c_str());
          }
          if (Key == "loc") {
            strcpy(label, Value.c_str());
          }
        }

      }
      f.close();
    } else {
      Serial.println("failed to mount FS");
    }

    WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
    WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);
    WiFiManagerParameter custom_label("label", "label", label, 255);

    //WiFiManager
    //Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;

    //set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);

    //add all your parameters here
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_label);

    //reset settings - for testing
    //wifiManager.resetSettings();
 
 if(wifiManager.autoConnect()) {
    Serial.printf("Starting HTTP...\n");
    HTTP.on("/index.html", HTTP_GET, [](){
      HTTP.send(200, "text/plain", "Hello World!");
    });
    HTTP.on("/description.xml", HTTP_GET, [](){
      SSDP.schema(HTTP.client());
    });
    HTTP.begin();

    Serial.printf("Starting SSDP...\n");
    SSDP.setSchemaURL("description.xml");
    SSDP.setHTTPPort(80);
    SSDP.setName("Alien Robot Army Sensor");
    SSDP.setSerialNumber("001788102201");
    SSDP.setURL("index.html");
    SSDP.setModelName("Alien Robot Army Multi-Sensor");
    SSDP.setModelNumber("929000226503");
    SSDP.setModelURL("http://www.alienrobotarmy.com/sensor");
    SSDP.setManufacturer("Alien Robot Army.com");
    SSDP.setManufacturerURL("http://www.alienrobotarmy.com");
    SSDP.setDeviceType("urn:schemas-upnp-org:device:TemperatureSensor:1");
    SSDP.begin();
    HTTP.on("/temperature", handleTemperature); 
    HTTP.on("/lumens", handleLight);
    HTTP.on("/motion", handleMotion);
    HTTP.on("/blink", handleBlink);
    HTTP.on("/reboot", handleReboot);
    HTTP.on("/reset", handleReset);

    StationName = wifi_station_get_hostname();
    //read updated parameters
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_port, custom_mqtt_port.getValue());
    strcpy(label, custom_label.getValue());

    //save the custom parameters to FS
    if (shouldSaveConfig) {
      Serial.println("saving config");
      File f = SPIFFS.open("/config.txt", "w");
      if (!f) {
        Serial.println("file creation failed");
      }
      // now write two lines in key/value style with  end-of-line characters
      f.print("mqs=");
      f.println(mqtt_server);
      f.print("mqp=");
      f.println(mqtt_port);
      f.print("loc=");
      f.println(label);
      f.close();
      Serial.println("save complete.");
    }
    Serial.printf("Saved Settings:\n\tMQTT Server [%s]\n\tMTTT Port [%d]\n\tLabel [%s]\n", mqtt_server, (int)atol(mqtt_port), label);

    Serial.printf("Starting MQTT...\n");
    client.setServer(mqtt_server, (int)atol(mqtt_port));
    
    Serial.printf("Ready!\n");
  } else {
    Serial.printf("WiFi Failed\n");
    ESP.reset();
    delay(1000);
  }
}

void sendMessage(String Type, String Value) {
  if (client.connect("ESP8266Client")) {
      char charBuf[1024];
      char tBuf[255];

      IPAddress ip = WiFi.localIP();
      String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
      String s = "{ "; 
      s += "\"Label\": \"";
      s += label;
      s += "\", ";
      s += "\"Address\": \"";
      s += ipStr;
      s += "\", ";
      s += Value;
      s += " }";

      String t = Type + "/"; 
      t += StationName;

      digitalWrite(ACTPIN, HIGH);
      s.toCharArray(charBuf, 1024);
      t.toCharArray(tBuf, 255);
      client.publish(tBuf, charBuf);
      digitalWrite(ACTPIN, LOW);
      Serial.printf("Sent [%s] [%s]\n", tBuf, charBuf);
    } else {
      Serial.printf("Couldn't connect to %s:%d\n", mqtt_server, (int)atol(mqtt_port));
    }
}

void loop() {
  static int i = 0, x = 0;
  static int now = 0, Dthen = 0, Lthen = 0;
  static int MotionEvent = 0;

  now = millis();
  
   // Sample the LDR once a second
   if (now > (Lthen+1000)) {
    //Serial.println("analogRead()");
    lightValue = analogRead(A0);
    Lthen = now;
   }

   // Sample DHT11 every 10 seconds
   if (now > (Dthen+10000)) {
    int err = 0;
    if ((err = dht.read(D2, &f, &h, NULL)) != SimpleDHTErrSuccess) {
      Serial.print("Read DHT11 failed, err="); Serial.println(err);
      delay(500);
    } else {
      i=0;
      Serial.print("DHT Read ");
      Serial.print(CtoF(f));
      Serial.print(" / ");
      Serial.println(h);
      delay(1000);
      //Serial.println("analogRead()");
      lightValue = analogRead(A0);
  
      String tempMsg = "\"Temperature\": ";
      tempMsg += CtoF(f);
      tempMsg += ", \"Humidity\": ";
      tempMsg += h;

      String lightMsg = "\"Lumens\": ";
      lightMsg += lightValue;

      sendMessage("temperature", tempMsg);
      sendMessage("lumens", lightMsg);
    
      Dthen = now;    
    }
  }

  pirState = digitalRead(PIRPIN);
  if (pirState == HIGH && MotionEvent == 0) {
    Serial.println("Motion detected");
    sendMessage("motion", "\"Event\": \"ON\"");
    MotionEvent = 1;
  }
  if (pirState == LOW && MotionEvent == 1) {
    Serial.println("Motion ended");
    sendMessage("motion", "\"Event\": \"OFF\"");
    MotionEvent = 0;
  }
  
  HTTP.handleClient();
  delay(100);
}

