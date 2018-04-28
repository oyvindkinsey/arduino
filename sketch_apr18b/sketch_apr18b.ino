#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <Wire.h>
#include <i2ckeypad.h>

#define ROWS 4
#define COLS 4

// With A0, A1 and A2 of PCF8574 to ground I2C address is 0x20
#define PCF8574_ADDR 0x20


i2ckeypad kpd = i2ckeypad(PCF8574_ADDR, ROWS, COLS);



// Update these with values suitable for your network.
const char* ssid = "sfsdfsbbg";
const char* password = "ppasswordd";
const char* mqtt_server = "10.55.44.108";

byte relON[] = {0xA0, 0x01, 0x01, 0xA2};  //Hex command to send to serial for open relay
byte relOFF[] = {0xA0, 0x01, 0x00, 0xA1}; //Hex command to send to serial for close relay

WiFiClient espClient;
PubSubClient client(espClient);
String switch1;
String strTopic;
String strPayload;

void setup_wifi() {

  delay(10);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  strTopic = String((char*)topic);
  if(strTopic == "home/locks/outer_door/set")
    {
    switch1 = String((char*)payload);

    if(switch1 == "UNLOCK")
      {
        Serial.write(relON, sizeof(relON));
        client.publish("home/locks/outer_door", "UNLOCK");
        
      }
    else
      {
        Serial.write(relOFF, sizeof(relOFF));   // turns the relay OFF
        client.publish("home/locks/outer_door", "LOCK");
      }
    }
}
 
 
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    // Attempt to connect
    if (client.connect("outer_door")) {
      // Once connected, publish an announcement...
      client.subscribe("home/locks/outer_door/#");
      client.publish("home/locks/outer_door", "LOCK");
    } else {
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
 
void setup()
{
  Serial.begin(9600);
  setup_wifi(); 
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

Serial.write(relOFF, sizeof(relOFF));   // turns the relay OFF
}
 
void loop()
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
