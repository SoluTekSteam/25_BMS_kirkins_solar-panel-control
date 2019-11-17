#include <OneWire.h>
#include <DallasTemperature.h>
#include <Countimer.h>

//INPUTS
#define readVoltsGreen A0 // if above 1V true (read as 204)
#define readVoltsYellow A1 // if above 1V true (read as 204)
#define batteryLevel A2 // check voltage directly from battery
#define inverterButton A5 // if above 1v being pushed
#define bmsActiveSignal 3  // will read TRUE when BMS active
#define tempSensors 2 // serial digital inputs

//OUTPUTS
#define voltOut1 A3  //sends 5v continuously
#define voltOut2 A4  //sends 5v continuously
#define greenDrainGround 13  // external LED
#define yellowDrainGround 12 // external LED
#define fanControl 11
#define voltLoadDump 10 // 24 volts to water heating element
#define lightingLoad 9 // 12 volt loads
#define greenLED 7
#define blueLED 6
#define redLED 5
#define inverterSwitch 4

Countimer timer;

int batteryState = 3;
float fanOnTemp       = 28.00;
float waterOffTemp       = 80.00;
double safeBatteryLow = 0.00;
double safeBatteryHigh = 40.00;
double safeCaseHigh = 45.00;
double safeWaterLow = 2.00;
double currentTemp, targetTemp;

bool errorState = false;
bool inverterTimerLock = false;
bool inverterTimerBlock = false;
bool inverterChanging = false;

bool inverterFaultTimerLock = false;
bool inverterFaultTimerBlock = false;

OneWire oneWire(tempSensors);
DallasTemperature sensors(&oneWire);

// 0 = water
// 1 = case
// 2 = battery

void setup() {
  Serial.begin(9600);
  sensors.begin();
  pinMode(fanControl, OUTPUT);
  pinMode(voltLoadDump, OUTPUT);
  pinMode(greenDrainGround, OUTPUT);
  pinMode(yellowDrainGround, OUTPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(blueLED, OUTPUT);
  pinMode(redLED, OUTPUT);
  pinMode(inverterSwitch, OUTPUT);
}

void controlFan() {
  if(sensors.getTempCByIndex(1) > fanOnTemp || sensors.getTempCByIndex(2) > fanOnTemp) {
    digitalWrite(fanControl,HIGH);
  } else {
    digitalWrite(fanControl, LOW);
  }
  // Debug messages
  Serial.print("Water Temp Sensor: "); 
  Serial.println(sensors.getTempCByIndex(0));
  Serial.print("Case Temp Sensor: ");
  Serial.println(sensors.getTempCByIndex(1));
  Serial.print("Batt Temp Sensor: ");
  Serial.println(sensors.getTempCByIndex(2));
}

void controlWaterHeat() {
  if(sensors.getTempCByIndex(0) > waterOffTemp || batteryState < 3) {
    digitalWrite(voltLoadDump,LOW);
    digitalWrite(blueLED, LOW);
  } else {
    digitalWrite(voltLoadDump, HIGH);
    analogWrite(blueLED, 50);
  }
}

void externalGreenLight() {
  if(analogRead(readVoltsGreen) > 204.00) {
    digitalWrite(greenDrainGround, HIGH);
  } else {
    digitalWrite(greenDrainGround, LOW);
  }
}

void externalYellowLight() {
  if(analogRead(readVoltsYellow) > 204.00) {
    digitalWrite(yellowDrainGround, HIGH);
  } else {
    digitalWrite(yellowDrainGround, LOW);
  }
}

void checkButton() {
  if(analogRead(inverterButton) > 204.00) {
    digitalWrite(inverterSwitch, HIGH);
  } else {
    digitalWrite(inverterSwitch, LOW);
  }
}

void turnOffInverter() {
  // turn off inverter if BMS not True
  // turn off inverter if battLevel Low
  // Turn off inverter if fault for 5 seconds
  // Turn on inverter if inverter is off and battlevel goes from Low to Normal
  if(!digitalRead(bmsActiveSignal) || batteryState < 2) {
    if(!inverterTimerLock) {
      inverterTimerLock = true;
      timer.setCounter(0, 0, 3, timer.COUNT_DOWN, confirmLowBatteryOrBMS);
    }
  } else if(inverterTimerLock) {
    inverterTimerBlock = true;
  }

  if(analogRead(readVoltsYellow) < 204) {
      inverterFaultTimerLock = true;
      timer.setCounter(0, 0, 5, timer.COUNT_DOWN, confirmInverterFault);
  } else if(inverterFaultTimerBlock) {
    inverterFaultTimerBlock = true;
  }
}

void confirmLowBatteryOrBMS() {
  if(!digitalRead(bmsActiveSignal) || batteryState < 2 || errorState) {
    if(!inverterTimerBlock && analogRead(readVoltsGreen) > 204) {
      inverterChanging = true;
      timer.setCounter(0, 0, 2, timer.COUNT_DOWN, stopInverterChanging);
    }
  }
  inverterTimerBlock = false;
  inverterTimerLock = false;
}

void confirmInverterFault() {
  if(analogRead(readVoltsYellow) < 204) {
    if(!inverterFaultTimerBlock && analogRead(readVoltsGreen) > 204) {
      inverterChanging = true;
      timer.setCounter(0, 0, 2, timer.COUNT_DOWN, stopInverterChanging);
    }
  }
}

void setBatteryState() {
  double batteryVoltage = (5*(analogRead(batteryLevel)/1023.00))*1.016;
  // Serial.print("Battery Level:    ");
  // Serial.println(analogRead(batteryLevel));
  Serial.print("Battery Voltage:    ");
  Serial.println(batteryVoltage);
  if(batteryVoltage < 2.6) {
    batteryState = 0; // battErrorLOW
  } else if(batteryVoltage <2.9) {
    batteryState = 1; // battLOW
  } else if(batteryVoltage < 3.5) {
    batteryState = 2; // battNORMAL
  } else if(batteryVoltage < 3.65) {
    batteryState = 3; // battHIGH
  } else {
    batteryState = 4; // battErrorHIGH
  }
  Serial.print("Battery State:    ");
  Serial.println(batteryState);
}

void changeInverterState() {
  if(inverterChanging) {
    digitalWrite(inverterSwitch, HIGH);
  }
}

void stopInverterChanging() {
  inverterChanging = false;
}

void controlLightingLoad(){
  if(batteryState < 2) {
    digitalWrite(lightingLoad, LOW);
  } else {
    digitalWrite(lightingLoad, HIGH);
  }
}

void getErrorState(){
  if(0 < batteryState < 4
      || sensors.getTempCByIndex(2) < safeBatteryLow
      || sensors.getTempCByIndex(2) > safeBatteryHigh
      || sensors.getTempCByIndex(1) > safeCaseHigh
      || sensors.getTempCByIndex(0) < safeWaterLow) {
    digitalWrite(greenLED, LOW);
    digitalWrite(redLED, HIGH);
    errorState = true;
  } else {
    digitalWrite(greenLED, HIGH);
    digitalWrite(redLED, LOW);
    errorState = false;
  }
}

void loop() {
  sensors.requestTemperatures();
  setBatteryState();
  getErrorState();
  turnOffInverter(); // only if needed
  controlFan();
  controlWaterHeat();
  externalGreenLight();
  externalYellowLight();
  changeInverterState();
  controlLightingLoad();
  checkButton();
}
