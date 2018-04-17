#include "DHT.h"
#include <WiFiLink.h>
#include <PubSubClient.h>

#define ADAFRUIT_USERNAME ""
#define AIO_KEY ""
#define HOME_FEEDS ADAFRUIT_USERNAME "/feeds/+"
#define HOME_TEMP_CHART_FEED ADAFRUIT_USERNAME "/feeds/home-temperature-chart"
#define HOME_TEMP_MEASURE_TOGGLE ADAFRUIT_USERNAME "/feeds/home-temperature-capture-toggle"
#define HOME_TEMP_CURRENT_MEASURE ADAFRUIT_USERNAME "/feeds/home-temperature-gauge"
#define HOME_HUMIDITY_CHART_FEED ADAFRUIT_USERNAME "/feeds/home-humidity-chart"
#define HOME_HUMIDITY_MEASURE_TOGGLE ADAFRUIT_USERNAME "/feeds/home-humidity-capture-toggle"
#define HOME_HUMIDITY_CURRENT_MEASURE ADAFRUIT_USERNAME "/feeds/home-humidity-gauge"
#define HOME_ALARM_START_TOGGLE ADAFRUIT_USERNAME "/feeds/home-alarm-toggle"
#define HOME_ALARM_STATUS_FEED ADAFRUIT_USERNAME "/feeds/home-alarm-status"
#define HOME_ALARM_FEED ADAFRUIT_USERNAME "/feeds/home-alarm"
#define HOME_ALARM_RESET ADAFRUIT_USERNAME "/feeds/home-alarm-reset"
#define DHTPIN 2
#define DHTTYPE DHT22

const int motionSensorPin = 7;
const int keyIndex = 0;
const long measureTempInterval = 10000;
const long measureHumidityInterval = 10000;
const long mqttBrokerPingInterval = 5000;
char onValue[] = "ON";
char offValue[] = "OFF";
char alarmEnabledMessage[] = "Alarm enabled";
char alarmDisabledMessage[] = "ALARM DISABLED!";
char detectingMessage[] = "Detecting...";
char motionDetectedMessage[] = "MOTION DETECTED";

unsigned long lastTempMeasureTime = 0;
unsigned long lastHumidityMeasureTime = 0;
unsigned long lastMqttBorkerPingTime = 0;
int status = WL_IDLE_STATUS;
char ssid[] = "";
char pass[] = "";
bool shouldMeasureTemp;
bool shouldMeasureHumidity;
bool alarmStarted;
bool motionDetected;

void callback(char* topic, byte* payload, unsigned int length) {
  if (strcmp(topic, HOME_TEMP_MEASURE_TOGGLE) == 0) {
    setShouldMeasureTemp(payload, length);
  } else if (strcmp(topic, HOME_HUMIDITY_MEASURE_TOGGLE) == 0) {
    setShouldMeasureHumidity(payload, length);
  } else if (strcmp(topic, HOME_ALARM_START_TOGGLE) == 0) {
    updateAlarmData(payload, length);
    publishAlarmData();
  } else if (strcmp(topic, HOME_ALARM_RESET) == 0) {
    resetAlarm();
    publishAlarmData();
  }
}

WiFiClient wifiClient;
PubSubClient mqttClient("io.adafruit.com", 1883, callback, wifiClient);
DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(9600);
  pinMode(motionSensorPin, INPUT);
  
  connectToWiFi();

  connectToMqttBroker();

  shouldMeasureTemp = false;
  shouldMeasureHumidity = false;
  alarmStarted = true;
  publishMeasureTempToggleState(offValue);
  publishMeasureHumidityToggleState(offValue);
  publishStartAlarmToggleState(onValue);
  publishAlarmData();
  
  subscribeForFeed(HOME_TEMP_MEASURE_TOGGLE);
  subscribeForFeed(HOME_HUMIDITY_MEASURE_TOGGLE);
  subscribeForFeed(HOME_ALARM_START_TOGGLE);
  subscribeForFeed(HOME_ALARM_RESET);
  
  dht.begin();
}

void loop() {
  if (alarmStarted){
    int motionSensorValue = digitalRead(motionSensorPin);

    if (motionSensorValue == LOW && !motionDetected){
      motionDetected = true;
      publishAlarmData();
      tone(BUZZER, 1000);     
    }
  }
  
  if (shouldMeasureTemp) {
    bool isTimeToMeasure = checkIsTimeToMeasureTemp();

    if (isTimeToMeasure) {
      publishTemp();
    }
  }

  if (shouldMeasureHumidity) {
      bool isTimeToMeasure = checkIsTimeToMeasureHumidity();
  
      if (isTimeToMeasure) {
        publishHumidity();
      }
  }
  
  pingMqttBrokerIfNeeded();
}

bool connectToWiFi() {
  if (WiFi.status() == WL_NO_WIFI_MODULE_COMM) {
    Serial.print("Communication with WiFi module not established.");
    endExecution();
  }

  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID:");
    Serial.println(ssid);

    status = WiFi.begin(ssid, pass);
    delay(10000);
  }

  Serial.println("Connected to WiFi!");
}

void endExecution() {
  while (true);
}

void connectToMqttBroker()
{
  if (mqttClient.connect("ardprimo", ADAFRUIT_USERNAME, AIO_KEY)) {
    Serial.println("MQTT Connected!");
  } else {
    Serial.println("MQTT connection failed!");
    endExecution();
  }
}

void subscribeForFeed(char feed[]){
  mqttClient.subscribe(feed);
}

void publishMeasureTempToggleState(char state[]) {
  publishData(HOME_TEMP_MEASURE_TOGGLE, state, "Measure temperature toggle state published", "Publishing measure temperature toggle state failed!");
}

void publishMeasureHumidityToggleState(char state[]) {
  publishData(HOME_HUMIDITY_MEASURE_TOGGLE, state, "Measure humidity toggle state published", "Publishing measure humidity toggle state failed!");
}

void publishStartAlarmToggleState(char state[]) {
  publishData(HOME_ALARM_START_TOGGLE, state, "Start alarm toggle state published", "Publishing start alarm toggle state failed!");
}

void setShouldMeasureTemp(byte* payload, unsigned int length) {
  char contentArray[length];
  getContent(payload, length, contentArray);
  String content(contentArray);
  String shouldMeasureValueString(onValue);

  if (content.equals(shouldMeasureValueString)) {
    shouldMeasureTemp = true;
    Serial.println("ShouldMeasureTemp set to true!");
  } else {
    shouldMeasureTemp = false;
    Serial.println("ShouldMeasureTemp set to false!");
  }
}

void setShouldMeasureHumidity(byte* payload, unsigned int length) {
  char contentArray[length];
  getContent(payload, length, contentArray);
  String content(contentArray);
  String shouldMeasureValueString(onValue);
  
  if (content.equals(shouldMeasureValueString)) {
    shouldMeasureHumidity = true;
    Serial.println("ShouldMeasureHumidity set to true!");
  } else {
    shouldMeasureHumidity = false;
    Serial.println("ShouldMeasureHumidity set to false!");
  }
}

void updateAlarmData(byte* payload, unsigned int length) {
  char contentArray[length];
  getContent(payload, length, contentArray);
  String content(contentArray);
  String shouldStartAlarmString(onValue);
  
  if (content.equals(shouldStartAlarmString)) {
    alarmStarted = true;
    Serial.println("Updating alarm data (alarm enabled)!");
  } else {
    alarmStarted = false;
    motionDetected = false;
    Serial.println("Updating alarm data (alarm disabled)!");
  }
}

void resetAlarm(){
  motionDetected = false;
  noTone(BUZZER);
}

void getContent(byte* payload, unsigned int length, char content[]) {
  int i = 0;

  for (int i = 0; i < length; i++) {
    content[i] = *(payload + i);
  }
}

bool checkIsTimeToMeasureTemp() {
  unsigned long currentTime = millis();
  bool isTimeToMeasure = false;

  if (currentTime - lastTempMeasureTime >= measureTempInterval) {
    lastTempMeasureTime = currentTime;
    isTimeToMeasure = true;
  }

  return isTimeToMeasure;
}

bool checkIsTimeToMeasureHumidity() {
  unsigned long currentTime = millis();
  bool isTimeToMeasure = false;

  if (currentTime - lastHumidityMeasureTime >= measureHumidityInterval) {
    lastHumidityMeasureTime = currentTime;
    isTimeToMeasure = true;
  }

  return isTimeToMeasure;
}

void publishTemp(){
  float temp = dht.readTemperature();
  char convertedTemp[8];
  sprintf(convertedTemp, "%2.2f", temp);

  publishData(HOME_TEMP_CHART_FEED, convertedTemp, "Temperature history published", "Publishing temperature history failed!");
  publishData(HOME_TEMP_CURRENT_MEASURE, convertedTemp, "Current temperature published", "Publishing current temperature failed!");
}

void publishHumidity(){
  float humidity = dht.readHumidity();
  char convertedHumidity[8];
  sprintf(convertedHumidity, "%2.2f", humidity);

  publishData(HOME_HUMIDITY_CHART_FEED, convertedHumidity, "Humidity history published", "Publishing humidity history failed!");
  publishData(HOME_HUMIDITY_CURRENT_MEASURE, convertedHumidity, "Current humidity published", "Publishing current humidity failed!");
}

void publishAlarmData(){
  if (alarmStarted){
    publishData(HOME_ALARM_STATUS_FEED, alarmEnabledMessage, "Alarm status published", "Publishing alarm status failed!");

    if (motionDetected){
      publishData(HOME_ALARM_FEED, motionDetectedMessage, "Motion detection status published", "Publishing motion detection status failed!");
    } else {
      publishData(HOME_ALARM_FEED, detectingMessage, "Motion detection status published", "Publishing motion detection status failed!");
    }
  } else {
    publishData(HOME_ALARM_STATUS_FEED, alarmDisabledMessage, "Alarm status published", "Publishing alarm status failed!");
    publishData(HOME_ALARM_FEED, alarmDisabledMessage, "Alarm status published", "Publishing alarm status failed!");
  }
}

void publishData(char feed[], char dataToPublish[], char successMessage[], char errorMessage[]){
  int result = mqttClient.publish(feed, dataToPublish);
  
  if (result != 0) {
    Serial.print(successMessage);
    Serial.print(": ");
    Serial.println(dataToPublish);
  } else {
    Serial.println(errorMessage);
    Serial.print("Error: ");
    Serial.println(result);
  }
}

void pingMqttBrokerIfNeeded() {
  unsigned long currentTime = millis();

  if (currentTime - lastMqttBorkerPingTime >= mqttBrokerPingInterval) {
    mqttClient.loop();
  }
}
