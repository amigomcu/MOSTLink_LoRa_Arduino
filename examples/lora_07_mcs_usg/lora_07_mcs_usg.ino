//////////////////////////////////////////////////////
// USG(CES) on MOSTLink protocol with MCS
// 
// Sensor connect to these pins on Arduino UNO:
//     1. blue-led(D3): show light-sensor(A0) value
//     2. red-led(D13): show reed-sensor(D12) state
//     3. green-led(D8): control by MCS
//     4. humidity-temperature sensor (D2)
//
// MediaTek Cloud Sandbox(MCS) web-server
//     1. sign in https://mcs.mediatek.com
//     2. Development->Prototype->Create Prototype->import from JSON file
//     3. select "MCS_USG.json" in "Arduinolibraries/MOSTLinkLib/examples/lora_07_mcs_usg" folder
//     4. Create test device, MCS will generate DeviceId,DeviceKey.
//     5. modify strDevice as your "DeviceId,DeviceKey"
//     6. select test device in MCS
//     7. Vertify and Upload this code to Arduino UNO
//
//////////////////////////////////////////////////////

#include "MOSTLora.h"
#include "DHT.h"
#include "MLutility.h"

#if !defined(__LINKIT_ONE__)
#include <MemoryFree.h>
#endif // __LINKIT_ONE__

MOSTLora lora;
DHT dht(2, DHT11);       

#define PIN_LED_CONTROL   8

#define PIN_SENSOR_REED   12
#define PIN_REED_LED      13

#define PIN_SENSOR_LIGHT  A0
#define PIN_LIT_LED       3

int szBuf = 0;
byte buf[100] = {0};
char strTmp[32] = {0};
float fTemperature, fHumidity;
unsigned long tsSensor = millis();

const char *strDevice = "D4AKFZ6S,6LTy6N1OkRlhDfk4";    // LoRa Node #1
const char *strCtrlLed = "CTRL_LED";
const char *strCtrlUpdate = "CTRL_UPDATE";        // refresh sensor
const char *strDispTemperature = "DISP_TEMPERATURE";
const char *strDispHumidity = "DISP_HUMIDITY";
const char *strDispLog = "DISP_LOG";
const char *strDispReed = "DISP_REED";
const char *strDispLight = "DISP_LIGHT";

// callback for rece data
void funcPacketReqData(unsigned char *data, int szData)
{
  memcpy(buf, data, szData);  
  buf[szData] = 0;
  debugSerial.print(F("ReqData= "));
  debugSerial.println((const char*)buf);
}

void funcPacketNotifyMcsCommand(unsigned char *data, int szData)
{
  memcpy(buf, data, szData);  
  buf[szData] = 0;
  debugSerial.print(F("NotifyMcsCommand= "));
  debugSerial.println((const char*)buf);

  int nVal = 0;
  String strValue;
  const char *strCtrl = NULL;
  if (parseDownlink(strCtrlLed, nVal)) {
    digitalWrite(PIN_LED_CONTROL, nVal);
    strValue = strCtrlLed;
    if (0 == nVal)
      strValue += " off";
    else
      strValue += " on";    
  }
  else if (parseDownlink(strCtrlUpdate, nVal)) {
    sendUplinkDHT();
    refreshControlState();
  }

  // send log to MCS (as LoRa ack)
  if (strValue.length() > 0) {
    // add timestamp
    unsigned long tsCurr = millis();
    sprintf(strTmp, " (@%d)", tsCurr);
    strValue += strTmp;
    sendUplink(strDispLog, strValue.c_str());    
  }
}
// parse downlink command from MCS
boolean parseDownlink(const char *strToken, int &nVal) {
  boolean bRet = false;
  const char *strBuf = (const char *)buf;
  if (strstr(strBuf, strToken) == strBuf) {
    const char *strVal = strBuf + strlen(strToken) + 1;
    nVal = atoi(strVal);
    bRet = true;
  }
  return bRet;
}

//
void setup() {
  Serial.begin(9600);  // use serial port for log monitor

  pinMode(PIN_SENSOR_REED, INPUT);
  pinMode(PIN_REED_LED, OUTPUT);
  
  pinMode(PIN_SENSOR_LIGHT, INPUT);
  pinMode(PIN_LIT_LED, OUTPUT);
  
  pinMode(PIN_LED_CONTROL, OUTPUT);

  digitalWrite(PIN_LED_CONTROL, HIGH);
  digitalWrite(PIN_REED_LED, HIGH);
  digitalWrite(PIN_LIT_LED, HIGH);
    
  lora.begin();
  // custom LoRa config by your environment setting
  lora.writeConfig(915555, 0, 0, 7, 5);
  lora.setMode(E_LORA_POWERSAVING);         // E_LORA_POWERSAVING

  delay(1000);

  // custom callback
  lora.setCallbackPacketReqData(funcPacketReqData);
  lora.setCallbackPacketNotifyMcsCommand(funcPacketNotifyMcsCommand);

  // init sensor for humidity & temperature
  dht.begin();
  int i = 0;
  boolean bReadDHT = false;
  while (!bReadDHT && i < 8) {
    delay(700);
    bReadDHT = dht.readSensor(fHumidity, fTemperature, true);
    i++;
  }

  // login MCS
  lora.sendPacketReqLoginMCS((uint8_t*)strDevice, strlen(strDevice));

  // init MCS control state:
  refreshControlState();
}

// send uplink command to MCS
void sendUplink(const char *strID, const char *strValue)
{
  String strCmd = strDevice;
  strCmd += ",";
  strCmd += MLutility::generateChannelData(strID, strValue);
  
  lora.sendPacketSendMCSCommand((uint8_t*)strCmd.c_str(), strCmd.length());
  delay(500);
}

void refreshControlState() {
  String strCmd = strDevice;
  strCmd += ",";
  strCmd += MLutility::generateChannelData(strCtrlLed, digitalRead(PIN_LED_CONTROL));
    
  int nReedState = digitalRead(PIN_SENSOR_REED);
  digitalWrite(PIN_REED_LED, nReedState);  
  strCmd += "\n";
  strCmd += MLutility::generateChannelData(strDispReed, nReedState);
  
  int nLit = analogRead(PIN_SENSOR_LIGHT);
  strCmd += "\n";
  strCmd += MLutility::generateChannelData(strDispLight, nLit);
  lora.sendPacketSendMCSCommand((uint8_t*)strCmd.c_str(), strCmd.length());
  delay(500);

  sendUplink(strCtrlUpdate, "0");
}

void sendUplinkDHT() {
  dht.readSensor(fHumidity, fTemperature, false);
  fTemperature = dht.convertCtoF(fTemperature);
  
  String strCmd = strDevice;
  strCmd += ",";
  strCmd += MLutility::generateChannelData(strDispHumidity, fHumidity);
  strCmd += "\n";
  strCmd += MLutility::generateChannelData(strDispTemperature, fTemperature);

  lora.sendPacketSendMCSCommand((uint8_t*)strCmd.c_str(), strCmd.length());
  delay(500);

  Serial.print(strCmd.length());
  Serial.println(F(" count chars"));
}

void loop() {
  lora.run();
  delay(100);

// reed sensor
  int nReedState = digitalRead(PIN_SENSOR_REED);
  digitalWrite(PIN_REED_LED, nReedState);
  
  int nLit = analogRead(PIN_SENSOR_LIGHT);    
  int nLitLed = map(nLit, 0, 1023, 0, 255);   // analog light-sensor convert to blue-led (PWM)
  analogWrite(PIN_LIT_LED, nLitLed);
    
  unsigned long tsCurr = millis();
  if (tsCurr > tsSensor + 5000) {
    tsSensor = tsCurr;
    dht.readSensor(fHumidity, fTemperature, true);
    fTemperature = dht.convertCtoF(fTemperature);

    Serial.print(fTemperature);
    Serial.print(F(" *F, "));
    Serial.print(F("LightSensor: "));
    Serial.print(nLit);
    Serial.print(F(", BlueLed(PWM): "));
    Serial.println(nLitLed);

    Serial.print(F("timestamp: "));
    Serial.println(tsCurr);

  }

  // command to send (for debug)
  if (Serial.available())
    inputBySerial();
}

void inputBySerial()
{
  int countBuf = MLutility::readSerial((char*)buf);
  if (countBuf > 1) {
    buf[countBuf] = 0;
    if (buf[0] == '/')
    {
      if (buf[1] == '1') {
        lora.sendPacketReqLoginMCS((uint8_t*)strDevice, strlen(strDevice));
      }
      else if (buf[1] == '2') {
        sendUplinkDHT();
      }
      else if (buf[1] == '3') {
      }
      else if (buf[1] == '4') {
        refreshControlState();
      }
      else if (buf[1] == 's') {
        dht.readSensor(fHumidity, fTemperature, true);
      }
      else if (buf[1] == '9') {
        lora.sendPacketSendMCSCommand((buf + 2), strlen((char*)buf + 2));
      }
    }
  }  
}
