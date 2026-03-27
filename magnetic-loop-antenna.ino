#include "config.h"

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#define ENABLE_PIN D0
#define STEP_PIN D1
#define DIR_PIN D2
#define SENSOR_PIN D5
#define BTN_UP_PIN D6
#define BTN_DOWN_PIN D7

#define DIR_UP HIGH
#define DIR_DOWN LOW

#define SENSOR_NORMAL LOW
#define SENSOR_ALERT HIGH

#define POSITION_MARGIN 150

int position = 0;
int minPosition;
int maxPosition;

bool stepperEnabled;
bool stepperAutoDisable;
uint32_t stepperLastUse;

String webString;
ESP8266WebServer server(80);

void wifi_action_index() {
  String response = "<!DOCTYPE HTML><html>"
  "<head><meta charset=\"utf-8\" /><title>Magnetic Loop Antenna</title>"
  "<style>.button {margin: 10px; padding: 10px; border: 1px solid}</style></head>"
  "<body><h1>Magnetic loop antenna</h1>"
  "<p>Position: " + String(position) + "</p>"
  "<a class=\"button\" href=\"/stepDown?step=100\">-100</a>"
  "<a class=\"button\" href=\"/stepDown?step=10\">-10</a>"
  "<a class=\"button\" href=\"/stepDown?step=1\">-1</a>"
  "<a class=\"button\" href=\"/stepUp?step=1\">+1</a>"
  "<a class=\"button\" href=\"/stepUp?step=10\">+10</a>"
  "<a class=\"button\" href=\"/stepUp?step=100\">+100</a>"
  "</body></html>";
  server.send(200, "text/html", response);
}

void wifi_action_stepUp() {
  int stepValue = 1;
  if (server.hasArg("step")) {
    stepValue = server.arg("step").toInt();
  }
  for (int i = 0; i < stepValue; i++) {
    stepUp(1);
  }
  wifi_action_index();
}

void wifi_action_stepDown() {
  int stepValue = 1;
  if (server.hasArg("step")) {
    stepValue = server.arg("step").toInt();
  }
  for (int i = 0; i < stepValue; i++) {
    stepDown(1);
  }
  wifi_action_index();
}

void initWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

  Serial.println("Connecting to WiFi network");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  server.begin();
  server.on("/", wifi_action_index);
  server.on("/stepUp", wifi_action_stepUp);
  server.on("/stepDown", wifi_action_stepDown);
  Serial.println("Server started");

  Serial.println(WiFi.localIP());
}

void doWifi() {
  server.handleClient();
}

bool isSensorAlert() {
  return digitalRead(SENSOR_PIN) == SENSOR_ALERT;
}

void doCalibration() {
  minPosition = 0;
  maxPosition = 1600; // Max steps for 180 degrees

  Serial.println("Calibrating...");

  // Handle unknown MIN or MAX position when sensor is alerting
  // Try to tune up a bit if we below minimal position
  if (isSensorAlert()) {
    Serial.println("Sensor is HIGH, checking if below MIN position");
    position = minPosition;
    for (int i = 0; i < POSITION_MARGIN; i++) {
      stepUp(10);
      if (!isSensorAlert()) {
        position = minPosition;
        break;
      }
    }
  }
  
  // If sensor still alerting then we above maximum position
  if (isSensorAlert()) {
    Serial.println("Sensor is HIGH, checking if above MAX position");
    position = maxPosition;
    for (int i = 0; i < POSITION_MARGIN; i++) {
      stepDown(10);
      if (!isSensorAlert()) {
        position = maxPosition;
        break;
      }
    }
  }

  if (!isSensorAlert()) {
    Serial.println("Sensor is NORMAL, searching safe MIN and MAX positions");
    while (!isSensorAlert()) {
      stepDown(1);
    }
    position = 0;
  
    // Rewind a bit back before looking MAX position
    for (int i = 0; i < POSITION_MARGIN; i++)
      stepUp(1);
    
    while (!isSensorAlert()) {
      stepUp(1);
    }
    maxPosition = position;
    
    for (int i = 0; i < maxPosition; i++) {
      stepDown(1);
    }
  
    getInfo();
  } else {
    Serial.println("Sensor still ALERT, unable to calibrate");
  }
}

void doStep(int delay_ms) {
  enableStepper();
  digitalWrite(STEP_PIN, HIGH);
  delay(delay_ms);
  digitalWrite(STEP_PIN, LOW);
  delay(delay_ms);
}

void stepUp(int delay_ms) {
  digitalWrite(DIR_PIN, DIR_UP);
  doStep(delay_ms);
  position++;
}

void stepDown(int delay_ms) {
  digitalWrite(DIR_PIN, DIR_DOWN);
  doStep(delay_ms);
  position--;
}

void enableStepper() {
  digitalWrite(ENABLE_PIN, LOW);
  stepperLastUse = millis();
  stepperEnabled = true;
}

void disableStepper() {
  digitalWrite(ENABLE_PIN, HIGH);
  stepperEnabled = false;
}

void getInfo() {
  Serial.print("\nSensor is ");
  if (isSensorAlert()) {
    Serial.println("ALERT");
  } else {
    Serial.println("NORMAL");
  }
  
  Serial.print("Position is ");
  Serial.print(position);
  Serial.print(" (min: ");
  Serial.print(minPosition);
  Serial.print(", max: ");
  Serial.print(maxPosition);
  Serial.println(")");

  Serial.print("Stepper is ");
  if (stepperEnabled) {
    Serial.println("ENABLED");
  } else {
    Serial.println("DISABLED");
  }
}

void initSerial() {
  Serial.begin(115200);
}

void doSerial() {
    if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.length() > 0) {
      char command = input.charAt(0);
      int spaceIndex = input.indexOf(' ');
      int command_value = 1;
      if (spaceIndex > 0) {
        command_value = input.substring(spaceIndex + 1).toInt();
      }
      if (command == 'c') {
        doCalibration();
      } else if (command == 'u') {
        for (int i = 0; i < command_value; i++)
          stepUp(10);
      } else if (command == 'd') {
        for (int i = 0; i < command_value; i++)
          stepDown(10);
      } else if (command == 'U') {
        for (int i = 0; i < command_value; i++)
          stepUp(1);
      } else if (command == 'D') {
        for (int i = 0; i < command_value; i++)
          stepDown(1);
      } else if (command == 'i') {
        getInfo();
      }
    }
  }
}

void initButtons() {
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
}

void doButtons() {
  // Button UP is pressed
  if (digitalRead(BTN_UP_PIN) == LOW && digitalRead(BTN_DOWN_PIN) == HIGH) {
    stepUp(10);
  }

  // Button DOWN is pressed
  if (digitalRead(BTN_DOWN_PIN) == LOW && digitalRead(BTN_UP_PIN) == HIGH) {
    stepDown(10);
  } 
}

void initStepper() {
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(SENSOR_PIN, INPUT);

  enableStepper();
  stepperAutoDisable = true;
}

void doStepper() {
  // Disable stepper driver after 1 second of inactivity
  if (stepperAutoDisable && stepperEnabled) {
    if (millis() > stepperLastUse + 1000) {
      disableStepper();
    }
  }  
}

void setup() {
  initSerial();
  Serial.println("\n\n\nMagnetic Loop Antenna\n\n\n");

  initStepper();
  initButtons();  
  
  doCalibration();

  initWifi();
}

void loop() {
  doSerial();
  doButtons();
  doStepper();
  doWifi();
}
