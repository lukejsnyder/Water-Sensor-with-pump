
#include <ESP8266WiFi.h>
#include <Losant.h>
#include <PubSubClient.h> 

// WiFi credentials.
const char* WIFI_SSID = "1206";
const char* WIFI_PASS = "912456801";

// Losant credentials.
const char* LOSANT_DEVICE_ID = "5ae8ca1fd27ab70006315236";
const char* LOSANT_ACCESS_KEY = "d8b8e8b7-fdbd-4718-82d1-71e03963aa0d";
const char* LOSANT_ACCESS_SECRET = "ac7f25ac625a0ec3e35f6cf99e1eb7d479a3dfa5edb90b82082faebd3b2138ec";


const int MOISTURE_PIN = 4; // D2
int relayInput = 2;

// Time between moisture sensor readings.
// The longer this delay, the longer the sensor
// will last without corroding.
const int REPORT_INTERVAL = 60000; // ms

// Delay between loops.
const int LOOP_INTERVAL = 10; // ms

WiFiClientSecure wifiClient;

LosantDevice device(LOSANT_DEVICE_ID);

/**
 * Invoked whenever a command is sent from Losant
 * to this device.
 */
void handleCommand(LosantCommand *command) {
  Serial.print("Command received: ");
  Serial.println(command->name);
}

/**
 * Connects to WiFi and then to the Losant platform.
 */
void connect() {

  // Connect to Wifi.
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println();
  Serial.print("Connecting to Losant...");

  device.connectSecure(wifiClient, LOSANT_ACCESS_KEY, LOSANT_ACCESS_SECRET);

  unsigned long connectionStart = millis();
  while(!device.connected()) {
    delay(500);
    Serial.print(".");

    // If we can't connect after 5 seconds, restart the board.
    if(millis() - connectionStart > 5000) {
      Serial.println("Failed to connect to Losant, restarting board.");
      ESP.restart();
    }
  }

  Serial.println("Connected!");
  Serial.println("This device is now ready for use!");
}

void setup() {
  Serial.begin(115200);

  // Wait until the serial in fully initialized.
  while(!Serial) {}
  
  Serial.println();
  Serial.println("Running Moisture Sensor Firmware.");
  

  pinMode(MOISTURE_PIN, OUTPUT);
  pinMode(relayInput, OUTPUT);

  // Attached the command handler.
  device.onCommand(&handleCommand);
  
  connect();
}

/**
 * Reads the value of the moisture sensor
 * and sends the value to Losant.
 */
void reportMoisture() {
  // Turn on the moisture sensor.
  // The sensor will corrode very quickly with current
  // running through it all the time. We just need to send
  // current through it long enough to read the value.
  digitalWrite(MOISTURE_PIN, HIGH);


  // Little time to stablize.
  delay(100);

  double raw = analogRead(A0);
  
  Serial.println();
  Serial.print("Moisture level: ");
  Serial.println(raw);

  // Turn the sensor back off.
  digitalWrite(MOISTURE_PIN, LOW);

  if(raw>700){
     digitalWrite(relayInput, LOW); // turn relay on
     delay(2000);
  }
   digitalWrite(relayInput, HIGH); // turn relay off
  
  // Build the JSON payload and send to Losant.
  // { "moisture" : <raw> }
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["moisture"] = raw;
  device.sendState(root);
}



int timeSinceLastRead = 0;

void loop() {

  bool toReconnect = false;

  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("Disconnected from WiFi");
    toReconnect = true;
  }

  if(!device.connected()) {
    Serial.println("Disconnected from MQTT");
    Serial.println(device.mqttClient.state());
    toReconnect = true;
  }

  if(toReconnect) {
    connect();
  }

  device.loop();

  // If enough time has elapsed, report the moisture.
  if(millis() - timeSinceLastRead > REPORT_INTERVAL) {
    timeSinceLastRead = millis();
    reportMoisture();
  }

  delay(LOOP_INTERVAL);
}
