/***************************************************************************************** 
*
* Waterflow sensor (Pulse counter) V3.0
*
* Written in Arduino IDE.
* This counter uses interrupts on ESP-01's GPIO3.
* It is "talk-only" client. 
* It sends out data while pulses detected.
* NTP sync.
* It keeps adding pulses until poweroff/reset.
*
* MQTT Message consists of 
*       pulses - number of pulses counted since last device boot.
*       liters - pulses converted to liters.
*       edges - number of rising and falling edges detected. Useful to check if disc is moving comparing old and actual value.
*       moved_last_half_hour - '1' if a transition in input pin has been detected in the last half hour.
*       inactive_for_48hrs - '1' if no new data in last 48 hours. Good to alarm if sensor is not registering data for a long period.
*       last_edge - Last edge detected datetime.
*       since - Last booting datetime.
*
* Copyright (C) 2021 Rubén López <rubenlogon@yahoo.es>
* 
*
* This library is free software. You may use/redistribute it under The MIT License terms. 
*
*****************************************************************************************/
#include <Arduino.h>
#include <ArduinoJson.h>
#include <NTPClient.h> //version from https://github.com/taranais/NTPClient/
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include "waterflow_sensor.h"

bool send_data = true;
bool moved_in_last_half_hour = false;
bool moved_in_last_48hrs = false;
const long utcOffsetInSeconds = 3600; //UTC+1
String BootDatetime = "-";
String LastEdgeDatetime = "-";

volatile bool edge_detected = false;
volatile bool pulse_reference; //indicates which kind of edge is going to be the reference for 1 pulse rev. (true = rising edge, false = falling edge)
volatile unsigned int Pulses = 0;
volatile unsigned int Edges = 0;
volatile unsigned long LastEdgeMillis;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  //Not used
}

WiFiClient WifiClient;
PubSubClient MqttClient(mqtt_broker, mqtt_port, mqttCallback, WifiClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);
const size_t bufferSize = JSON_OBJECT_SIZE(7);
DynamicJsonBuffer jsonBuffer(bufferSize);
JsonObject& payload = jsonBuffer.createObject();

void pulseHandler() {
  if(Edges == 0) {
    pulse_reference = !digitalRead(PULSE_PIN);
  }
  if((unsigned long)(millis() - LastEdgeMillis) >= DEBOUNCE_MS) {
    edge_detected = true;
    Edges = Edges + 1;
    LastEdgeMillis = millis();
    if(pulse_reference == digitalRead(PULSE_PIN)) { //We got a pulse (1 rev)
      Pulses = Pulses + 1;
    }
  }
}

bool pubdata(void) {
  payload["pulses"]               = Pulses; // = disc revs
  payload["liters"]               = Edges*1000*WATERMETER_RESOLUTION_M3*(10/2); //my watermeter has a resolution of 0.0001m³/rev and half circle disc. 1 rev m³ = 10 x 0.0001. Half metal circle m³ res = (10 x 0.0001)/2. Half metal circle liters res = 1000 x (10 x 0.0001)/2 = 0.5l 
  payload["edges"]                = Edges;
  payload["moved_last_half_hour"] = int(moved_in_last_half_hour);
  payload["inactive_for_48hrs"]   = int(!moved_in_last_48hrs);
  payload["last_edge"]            = LastEdgeDatetime;
  payload["since"]                = BootDatetime;

  char buffer[512];
  payload.printTo(buffer, sizeof(buffer));
    
  if(!MqttClient.publish(dest_topic, buffer, true)) {
    //Fail to publish
    if(!MqttClient.connected()) {
      //Not connected to broker
      if(!MqttClient.connect(client_id, mqtt_username, mqtt_password)) {
        //Can't connect to broker - Maybe wifi down? restart loop
      }
    }
    return false;
  }
  else {
    return true;
  }
}

void setup() {
  //Serial.begin(115200, SERIAL8N1, SERIALTX_ONLY);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(PULSE_PIN, INPUT_PULLUP);
  attachInterrupt(PULSE_PIN, pulseHandler, CHANGE);

  WiFi.hostname(client_id);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED);
  MqttClient.connect(client_id, mqtt_username, mqtt_password);
  timeClient.begin();
  timeClient.update();
  BootDatetime = timeClient.getFormattedDate();
}

void loop() {
  if(WiFi.status() == WL_CONNECTED) {
    if((unsigned long)(millis() - LastEdgeMillis) >= LED_FLICKER_MS){
      digitalWrite(LED_BUILTIN, LOW); //There's wifi, then led ON
    }
    if(moved_in_last_half_hour && (unsigned long)(millis() - LastEdgeMillis) >= 1800 * 1000) {
      moved_in_last_half_hour = false;
      send_data = true;
    }
    if(moved_in_last_48hrs && (unsigned long)(millis() - LastEdgeMillis) >= 3600 * 24 * 2 * 1000) {
      moved_in_last_48hrs = false;
      send_data = true;
    }
    if(edge_detected) {
      moved_in_last_half_hour = true;
      moved_in_last_48hrs = true;
      timeClient.update();
      LastEdgeDatetime = timeClient.getFormattedDate();
      digitalWrite(LED_BUILTIN, HIGH); //turnoff led
      send_data = true;
      edge_detected = false;
    }
    if(MqttClient.connected()) {
      if(send_data && pubdata()) {
        send_data = false;
      }
      MqttClient.loop();
    }
    else {
      if(MqttClient.connect(client_id, mqtt_username, mqtt_password)) {
        MqttClient.loop();
      }
    }
  } 
  else {
    digitalWrite(LED_BUILTIN, HIGH); //No wifi, no led light
    WiFi.begin(ssid, password);
    while(WiFi.waitForConnectResult() != WL_CONNECTED);
  }
}
