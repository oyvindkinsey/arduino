#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

const char *SSID = "diskworld";
const char *PASSWORD = SECRETS_WIFI_PASSWORD;
const char *MQTT_SERVER = "192.168.0.10";
const char *MQTT_USER = "buzzer";
const char *MQTT_PASSWORD = SECRETS_MQTT_PASSWORD;
const char *MQTT_CLIENT_ID = "outer_door";
const char *OTA_HOST = "buzzer";
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
      false,
      "offline");
    mqtt_subscribe();
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
  wifi_setup();
  mqtt_setup();
  ota_setup();
  custom_setup();
}

void loop() {
  mqtt_loop();
  ota_loop();
  custom_loop();
  if (ts == 0 || millis() - ts > AVAILABILITY_INTERVAL) {
    ts = millis();
    client.publish(AVAILABILITY_TOPIC, "online");
    custom_status();
  }
}

/* **************************************************************
 * Custom
 * *************************************************************/

 // channel 1
const byte relON_1[] = {0xA0, 0x01, 0x01, 0xA2};
const byte relOFF_1[] = {0xA0, 0x01, 0x00, 0xA1};
const int TIMEOUT_1 = 30000;
int buzzer_ts_1;
char *status_1 = "LOCK";

void unlock_1() {
  status_1 = "UNLOCK";
  Serial.write(relON_1, sizeof(relON_1));
  buzzer_ts_1 = millis();
}

void lock_1() {
  status_1 = "LOCK";
  Serial.write(relOFF_1, sizeof(relOFF_1));
  buzzer_ts_1 = 0;
}
// channel 2
const byte relON_2[] = {0xA0, 0x02, 0x01, 0xA3};
const byte relOFF_2[] = {0xA0, 0x02, 0x00, 0xA2};
const int TIMEOUT_2 = 30000;
int buzzer_ts_2;
char *status_2 = "LOCK";

void unlock_2() {
  status_2 = "UNLOCK";
  Serial.write(relON_2, sizeof(relON_2));
  buzzer_ts_2 = millis();
}

void lock_2() {
  status_2 = "LOCK";
  Serial.write(relOFF_2, sizeof(relOFF_2));
  buzzer_ts_2 = 0;
}

/* handlers */

void mqtt_callback(char *topic, byte *payload, unsigned int length) {
  std::string t((char *)topic, length);
  std::string p((char *)payload, length);

  if (strcmp(topic, "home/locks/outer_door/set/1") == 0) {
    if (p == "UNLOCK") {
      unlock_1();
    } else {
      lock_1();
    }
  } else {
    if (p == "UNLOCK") {
      unlock_2();
    } else {
      lock_2();
    }
  }

  reconnect();
  custom_status();
}

void mqtt_subscribe() {
  client.subscribe("home/locks/outer_door/set/#", 1);
}

void custom_setup() {
  lock_1()  ;
  lock_2()  ;
  reconnect();
  custom_status();
  Serial.println("Lock ready...");
}

void custom_loop() {
  if (buzzer_ts_1 != 0 && millis() - buzzer_ts_1 > TIMEOUT_1) {
    lock_1();
  }
  if (buzzer_ts_2 != 0 && millis() - buzzer_ts_2 > TIMEOUT_2) {
    lock_2();
  }
}

void custom_status() {
  client.publish("home/locks/outer_door/1", status_1);
  client.publish("home/locks/outer_door/2", status_2);
}
