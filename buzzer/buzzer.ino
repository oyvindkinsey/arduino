#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

const char *SSID = "diskworld";
const char *PASSWORD = SECRETS_WIFI_PASSWORD;
const char *MQTT_SERVER = "192.168.0.10";
const char *MQTT_USER = "relay";
const char *MQTT_PASSWORD = SECRETS_MQTT_PASSWORD;
const char *MQTT_CLIENT_ID = "outer_door";
const char *OTA_HOST = "relay";
const char *OTA_PASSWORD = SECRETS_OTA_PASSWORD;
const char *AVAILABILITY_TOPIC = "home/locks/outer_door/available";
const int AVAILABILITY_INTERVAL = 60000;

/* wifi */
WiFiClient espClient;

void wifi_setup() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    WiFi.begin(SSID, PASSWORD);
    Serial.println("Retrying connection...");
  }
}

/* ota */

void ota_setup() {
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(OTA_HOST);

  ArduinoOTA.onStart([]() {
    String type =
        (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

void ota_loop() { ArduinoOTA.handle(); }

/* mqtt */
PubSubClient client(espClient);

void reconnect() {

  while (!client.connected()) {
    bool connected = client.connect(
      MQTT_CLIENT_ID,
      MQTT_USER,
      MQTT_PASSWORD,
      AVAILABILITY_TOPIC,
      1,
      0,
      "offline");
    if (!connected) {
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void mqtt_setup() {
  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(mqtt_callback);
  client.publish("home/locks/outer_door", "LOCK");
}

void mqtt_loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

/* **************************************************************
 * Main
 * *************************************************************/

int ts = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Setup");
  Serial.println("wifi");
  wifi_setup();
  Serial.println("ota");
  ota_setup();
  Serial.println("custom");
  custom_setup();
  client.publish("home/locks/outer_door/hello", "hello");
}

void loop() {
  mqtt_loop();
  ota_loop();
  custom_loop();
  if (ts == 0 || millis() - ts > AVAILABILITY_INTERVAL) {
    ts = millis();
    client.publish(AVAILABILITY_TOPIC, "online_");
  }
}

/* **************************************************************
 * Custom
 * *************************************************************/

 // channel 1
const byte relON[] = {0xA0, 0x01, 0x01, 0xA2};
const byte relOFF[] = {0xA0, 0x01, 0x00, 0xA1};

// channel 2
// const byte relON[] = {0xA0, 0x02, 0x01, 0xA3};
// const byte relOFF[] = {0xA0, 0x02, 0x00, 0xA2};
const int TIMEOUT = 30000;

int buzzer_ts;

void unlock() {
  Serial.write(relON, sizeof(relON));
  buzzer_ts = millis();
  reconnect();
  client.publish("home/locks/outer_door", "UNLOCK");
}

void lock() {
  Serial.write(relOFF, sizeof(relOFF));
  buzzer_ts = 0;
  reconnect();
  client.publish("home/locks/outer_door", "LOCK");
}

/* handlers */

void mqtt_callback(char *topic, byte *payload, unsigned int length) {
  Serial.println("MQTT Callback");
  client.publish("home/locks/outer_door/callback", (char *)payload);
  payload[length] = '\0';
  if (String((char *)payload) == "UNLOCK") {
    unlock();
  } else {
    lock();
  }
}

void custom_setup() {
  Serial.println("Custom setup");
  lock()  ;
  reconnect();
  client.subscribe("home/locks/outer_door/set", 1);
  Serial.println("Lock ready...");
}

void custom_loop() {
  if (buzzer_ts != 0 && millis() - buzzer_ts > TIMEOUT) {
    lock();
  }
}
