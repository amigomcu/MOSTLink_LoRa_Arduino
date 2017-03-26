//////////////////////////////////////////////////////
// ShareCourse on MOSTLink protocol with MCS
// 
// Starter: uplink to MCS
// 1. DHT22 sensor for humidity and temperature
//
//////////////////////////////////////////////////////

#include "MOSTLora.h"
#include "MLutility.h"

MOSTLora lora;

// DHT sensor related:
#if defined(__LINKIT_ONE__)
#include "LDHT.h"
LDHT dht(2, DHT22);      // DHT22, DHT11
#else     // __LINKIT_ONE__
#include "DHT.h"
DHT dht(2, DHT22);      // DHT22, DHT11
#endif    // __LINKIT_ONE__

int szBuf = 0;
byte buf[100] = {0};
char *strTmp = (char*)buf;
float fTemperature, fHumidity;

unsigned long tsSensor = millis();

// TODO: fill your own deviceKey, deviceID from MCS
const char *strDevice = "DAqjhn8J,oYWff0vnH0O8x4xr";  // usgmostlink

const char *strDispTemperature = "DISP_T";
const char *strDispHumidity = "DISP_H";

// callback for rece data
void funcPacketReqData(unsigned char *data, int szData)
{
  memcpy(buf, data, szData);  
  buf[szData] = 0;
  debugSerial.print(F("ReqData= "));
  debugSerial.println((const char*)buf);

  sendUplinkDHT();
}

//
void setup() {
#ifdef DEBUG_LORA
  Serial.begin(9600);  // use serial port for log monitor
  Serial.println(F("*** lora_07_mcs_starter ***"));
#endif // DEBUG_LORA    
  lora.begin();
  // custom LoRa config by your environment setting
  // config setting: frequency, group, data-rate, power, wakeup-time
  lora.writeConfig(915000, 0, 0, 7, 5);
  lora.setMode(E_LORA_POWERSAVING);         // E_LORA_POWERSAVING

  delay(1000);

  // custom callback
  lora.setCallbackPacketReqData(funcPacketReqData);

  // init sensor for humidity & temperature
  dht.begin();
  int i = 0;
  boolean bReadDHT = false;
  while (!bReadDHT && i < 8) {
    delay(700);
    bReadDHT = readSensorDHT(fHumidity, fTemperature, true);
    i++;
  }
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

void sendUplinkDHT() {
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

  unsigned long tsCurr = millis();
  if (tsCurr > tsSensor + 5000) {
    tsSensor = tsCurr;
    readSensorDHT(fHumidity, fTemperature, true);

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
      }
      else if (buf[1] == 's') {
        readSensorDHT(fHumidity, fTemperature, true);
      }
      else if (buf[1] == '9') {
        lora.sendPacketSendMCSCommand((buf + 2), strlen((char*)buf + 2));
      }
    }
  }  
}


bool readSensorDHT(float &fTemp, float &fHumi, bool bShowResult)
{
    bool bRet = true;
#if defined(__LINKIT_ONE__)  
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    if(dht.read())
    {
        fTemp = dht.readTemperature();
        fHumi = dht.readHumidity();
        if (bShowResult) {
            Serial.println("------------------------------");
            Serial.print("Temperature Celcius = ");
            Serial.print(dht.readTemperature());
            Serial.println("C");
    
            Serial.print("Temperature Fahrenheit = ");
            Serial.print(dht.readTemperature(false));
            Serial.println("F");
    
            Serial.print("Humidity = ");
            Serial.print(dht.readHumidity());
            Serial.println("%");
    
            Serial.print("HeatIndex = ");
            Serial.print(dht.readHeatIndex(fTemp,fHumi));
            Serial.println("C");
    
            Serial.print("DewPoint = ");
            Serial.print(dht.readDewPoint(fTemp,fHumi));
            Serial.println("C");
        }
    }
#else     // __LINKIT_ONE__
    bRet = dht.readSensor(fHumidity, fTemperature, true);
#endif    // __LINKIT_ONE__

    return bRet;
}

