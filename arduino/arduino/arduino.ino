#include <ESP8266WiFi.h>  
#include <FirebaseArduino.h>
#include <ArduinoJson.h>

#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>


#define WIFI_SSID "nombre wifi"
#define WIFI_PASSWORD "clave wifi"

#define FIREBASE_HOST "<nombre host>.firebaseio.com"
#define FIREBASE_AUTH "<clave auth>"


#define sensorTemp A0
#define ventilador 5 


int tiempoMedicion = 60; // segundos
unsigned long tiempoAntMedicion = 0;

int tiempoLect = 5; // segundos
unsigned long tiempoAntLect = 0;

int umbral = 35;
int temperatura = 0;
bool manualControl = false;

bool changed = false;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  // connect to wifi.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); 
  Serial.print("conectando......");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("conectado: ");
  Serial.println(WiFi.localIP());
  // ya esta conectado al wifi
  
  // conexión con firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  // pinMode(sensor, INPUT);
  pinMode(ventilador, OUTPUT);  
  
  Firebase.stream("version"); // pendiente a los cambios de version 


}

void loop() {
  // put your main code here, to run repeatedly:
 getTemperatura();  // 1020
 uploadStateVentilador();
 SubscriptionChanges();
 uploadTemperatura(temperatura);

}

int getTemperatura() {
       if (millis() - tiempoAntLect >= tiempoLect * 1000)  {
             tiempoAntLect = millis();  
             temperatura = analogRead(sensorTemp);    // 0 - 1023
             temperatura  = map(temperatura, 0, 1023, 0, 50);
             if (manualControl == false) {
                 if (temperatura >= umbral) { 
                   setVentilador(true); 
                 } else { 
                   setVentilador(false);
                 }              
             }
           
       }
       return temperatura;
}

String getTime() {
    WiFiClient client;
    HTTPClient http;  
    String timeS = "";
    
    http.begin(client, "http://worldtimeapi.org/api/timezone/America/Guayaquil");
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = http.getString();
          
          int beginS = payload.indexOf("datetime");
          int endS = payload.indexOf("day_of_week");
          Serial.println(payload);
          timeS = payload.substring(beginS + 11, endS - 3);    
    }
    return timeS;
}


void uploadTemperatura(int value) {
    if (millis() - tiempoAntMedicion >= tiempoMedicion * 1000)  { 
        // subimos temperatura al servidor
        tiempoAntMedicion = millis();
        Serial.print("uploadTemperatura -> ");
        Serial.println(value);
        String Time = getTime();     
        String path = "mediciones/"; 
        StaticJsonBuffer<200> jsonBuffer;
        JsonObject& data = jsonBuffer.createObject();
        data["time"] = Time;
        data["sensor"] = value;
        Firebase.push(path, data); 
    }
}


void setVentilador(bool estado) {
      bool estadoAct = digitalRead(ventilador);
      if (estadoAct != estado) {
          digitalWrite(ventilador, estado);
          Serial.print("set ventilador: ");
          Serial.println(estado);   
          changed = true;
      }
}

void uploadStateVentilador() {
    if (changed) {
        bool estado = digitalRead(ventilador);
        Firebase.setBool("ventilador_state", estado);
        if (!Firebase.failed()) {  
            Serial.println("write success");
            changed = false;
        } else {
            Serial.println("write failded");
        }
        delay(1000);  
    }
}

void SubscriptionChanges() {
     if (Firebase.available()) {  
         FirebaseObject event = Firebase.readEvent();
         String eventType = event.getString("type");
         if (eventType == "put") {  
               Serial.println("|....Changes() DataBase....|");
               int value = Firebase.getInt("ventilador");
               if (!Firebase.failed()) {
                  Serial.print("ventilador configurado: ");
                  Serial.println(value);
                  if (value == 0) { manualControl = false;}
                  if (value > 0) { 
                        manualControl = true;
                        if (value == 1) { setVentilador(false); }
                        if (value == 2) { setVentilador(true); }
                  }     
               }
               umbral = Firebase.getInt("umbral");
               if (!Firebase.failed()) { 
                  Serial.print("umbral configurado: ");
                  Serial.println(umbral);
               }
         }      
     }
}
