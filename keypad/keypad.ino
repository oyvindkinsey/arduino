#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

const char *SSID = "diskworld";
const char *PASSWORD = SECRETS_WIFI_PASSWORD;
const char *MQTT_SERVER = "192.168.0.10";
const char *MQTT_USER = "keypad";
const char *MQTT_PASSWORD = SECRETS_MQTT_PASSWORD;
const char *MQTT_CLIENT_ID = "keypad";
const char *OTA_HOST = "keypad";
const char *OTA_PASSWORD = SECRETS_OTA_PASSWORD;
const char *AVAILABILITY_TOPIC = "home/keypads/outer_door/available";
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
  Serial.begin(9600);
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
  }
}

/* **************************************************************
 * Custom
 * *************************************************************/

#include <Wire.h>
#include <Keypad.h>
#include "Keypad_I2C.h"
#define I2CADDR 0x20

const byte KEYPAD_ROWS = 4;
const byte KEYPAD_COLS = 4;
const uint8_t MAX_LENGTH = 20;
const int TIMEOUT = 10000;
const char *UPDATE_TOPIC = "home/keypads/outer_door/code";

int key_ts;

const char keys[KEYPAD_ROWS][KEYPAD_COLS] = {{'1', '2', '3', 'A'},
                                             {'4', '5', '6', 'B'},
                                             {'7', '8', '9', 'C'},
                                             {'*', '0', '#', 'D'}};

byte rowPins[KEYPAD_ROWS] = {4, 5, 6, 7};
byte colPins[KEYPAD_COLS] = {0, 1, 2, 3};

Keypad_I2C kpd(makeKeymap(keys), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS,
               I2CADDR, PCF8574);

uint8_t length = 0;

struct Entry {
  char key;
  Entry *next;
  ~Entry() {}
};

Entry *start = nullptr;
Entry *end = nullptr;

void reset() {
  while (start != nullptr) {
    Entry *t = start;
    start = t->next;
    delete t;
  }
  start = nullptr;
  end = nullptr;
  key_ts = 0;
}

void sendCode(char code[]) {
  Serial.println("Sending " + String(code));
  client.publish(UPDATE_TOPIC, code);
}

void handleKey(char key) {
  key_ts = millis();
  if (key == '#') {
    char code[length];
    uint8_t i = 0;

    // build the char array and free the nodes
    while (start != nullptr) {
      code[i++] = start->key;
      Entry *t = start;
      start = t->next;
      delete t;
    }
    code[length] = 0;

    length = 0;
    start = nullptr;
    end = nullptr;
    sendCode(code);
  } else {
    Entry *t = new Entry;
    t->key = key;
    t->next = nullptr;
    if (start == nullptr) {
      start = t;
    }
    if (end != nullptr) {
      end->next = t;
    }
    end = t;

    if (length < MAX_LENGTH) {
      length++;
    } else {
      Entry *t = start;
      start = start->next;
      delete t;
    }
  }
}

/* handlers */

void mqtt_callback(char *topic, byte *payload, unsigned int length) {
}

void custom_setup() {
  Wire.begin(0, 2);
  kpd.begin(makeKeymap(keys));
  Serial.println("Keypad ready...");
}

void custom_loop() {
  if (key_ts != 0 && millis() - key_ts > TIMEOUT) {
    reset();
  }
  char key = kpd.getKey();
  if (key) {
    Serial.println(key);
    handleKey(key);
  }
}
