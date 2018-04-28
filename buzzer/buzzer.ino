#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

// Update these with values suitable for your network.
const char *SSID = "diskworld";
const char *PASSWORD = SECRETS_WIFI_PASSWORD;
const char *MQTT_SERVER = "192.168.0.10";
const char *MQTT_USER = "buzzer";
const char *MQTT_PASSWORD = SECRETS_MQTT_PASSWORD;

byte relON[] = {0xA0, 0x01, 0x01,
                0xA2}; // Hex command to send to serial for open relay
byte relOFF[] = {0xA0, 0x01, 0x00,
                 0xA1}; // Hex command to send to serial for close relay

const int AVAILABILITY_INTERVAL = 60000;

WiFiClient espClient;
PubSubClient client(espClient);
String switch1;
String strTopic;
String strPayload;

void setup_wifi() {

  delay(10);
  WiFi.begin(SSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

int ts;

void unlock() {
  Serial.println("Unlocking");
  Serial.write(relON, sizeof(relON));
  client.publish("home/locks/outer_door", "UNLOCK");
}

void lock() {
  Serial.println("Locking");
  Serial.write(relOFF, sizeof(relOFF)); // turns the relay OFF
  client.publish("home/locks/outer_door", "LOCK");
}

void callback(char *topic, byte *payload, unsigned int length) {
  payload[length] = '\0';
  strTopic = String((char *)topic);
  if (strTopic == "home/locks/outer_door/set") {
    switch1 = String((char *)payload);
    if (switch1 == "UNLOCK") {
      unlock();
    } else {
      lock();
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    // Attempt to connect
    if (client.connect("outer_door", MQTT_USER, MQTT_PASSWORD)) {
      // Once connected, publish an announcement...
      client.subscribe("home/locks/outer_door/#");
    } else {
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void updateAvailability() {
  client.publish("home/locks/outer_door/available", "online");
  client.publish("home/locks/outer_door", "LOCK");
}

void setup() {
  Serial.begin(9600);
  setup_wifi();
  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(callback);

  client.publish("home/locks/outer_door", "LOCK");
  Serial.println("Lock ready...");
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (millis() - ts > AVAILABILITY_INTERVAL) {
    ts = millis();
    updateAvailability();
  }
}
