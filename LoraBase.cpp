//
//  LoraBase.cpp
//  libMOST
//
//  Created by Macbear Chen on 2017/2/2.
//  Copyright © 2017 viWave. All rights reserved.
//

#include "LoraBase.h"
#include "MLutility.h"

///////////////////////
// LoRa Serial bus: RX/TX
///////////////////////
#if defined(__LINKIT_ONE__)
    #define loraSerial Serial1       // for LinkIt ONE

#else // __LINKIT_ONE__

#ifdef USE_VINDUINO
#define loraSerial Serial       // for Vinduino
#else // USE_VINDUINO
    // for arduino Uno
    #include <SoftwareSerial.h>
    const int pinLoraRX = 10;
    const int pinLoraTX = 11;
    //      pinMode(pinLoraRX, INPUT);
    //      pinMode(pinLoraTX, OUTPUT);
    SoftwareSerial loraSerial(pinLoraRX, pinLoraTX);    // RX, TX

#endif // USE_VINDUINO

#endif // __LINKIT_ONE__

/////////////////////////////////////////

LoraBase::LoraBase(int pinP1, int pinP2, int pinBusy)
{
    _pinP1 = pinP1;
    _pinP2 = pinP2;
    _pinBZ = pinBusy;
    _eMode = E_UNKNOWN_LORA_MODE;
    
    _strBuf = (char*)_buf;
}

void LoraBase::begin(long speed)
{
    pinMode(_pinP1, OUTPUT);
    pinMode(_pinP2, OUTPUT);
    pinMode(_pinBZ, INPUT);
    
    loraSerial.begin(speed);
}

// LM-110H1 module for 3 firmware mode:
// a. AT command: LoRaWAN
// b. AT command: MOST
// c. p1p2 mode: MOST
void LoraBase::setFirmwareMode(E_LORA_FW_MODE modeFW)
{
    unsigned char arrayLORAWAN[8] = {0XFF,0X4C,0XCF,0X52,0XA1,0X64,0X47,0X00};
    E_LORA_FW_MODE eModeCurr = E_UNKNOWN_FW_MODE;
    eModeCurr = getFirmwareMode();
    
    switch (modeFW) {
        case E_FW_AAT_LORAWAN:
#ifdef DEBUG_LORA
            debugSerial.print(F("*** FW ---> AAT LoRaWAN ***\n"));
#endif
            if (E_FW_AAT_MOST == eModeCurr)
            {
                // Firmware: AAT MOST --> LoRaWAN
                command("AAT1 LW=0");
                command("AAT1 Save");
                command("AAT1 Reset");
            }
            else if (E_FW_P1P2_MOST == eModeCurr)
            {
                // Firmware: p1p2--> LoRaWAN
                loraSerial.begin(9600);
                setMode(E_LORA_NORMAL);     // p1(0 -> 1) pin for setup
                setMode(E_LORA_SETUP);
                sendData(arrayLORAWAN, 8);
                // success: 41 43 4B 18 21 C9
                loraSerial.begin(57600);
            }
            break;
            
        case E_FW_AAT_MOST:
#ifdef DEBUG_LORA
            debugSerial.print(F("*** FW ---> AAT MOST ***\n"));
#endif
            if (E_FW_AAT_LORAWAN == eModeCurr) {
                command("AAT1 LW=1");
                command("AAT1 Save");
                command("AAT1 Reset");
            }
            else if (E_FW_P1P2_MOST == eModeCurr) {
                setFirmwareMode(E_FW_AAT_LORAWAN);
                setFirmwareMode(E_FW_AAT_MOST);
            }
            break;
            
        case E_FW_P1P2_MOST:
#ifdef DEBUG_LORA
            debugSerial.print(F("*** FW ---> P1P2 MOSTLink ***\n"));
#endif
            if (E_FW_AAT_LORAWAN == eModeCurr) {
                command("AAT2 Lora_Most_Switch=1");
                command("AAT1 Save");
                command("AAT1 Reset");
                loraSerial.begin(9600);
            }
            else if (E_FW_AAT_MOST == eModeCurr) {
                setFirmwareMode(E_FW_AAT_LORAWAN);
                setFirmwareMode(E_FW_P1P2_MOST);
            }
            break;
        default:
            break;
    }
}

E_LORA_FW_MODE LoraBase::getFirmwareMode()
{
    E_LORA_FW_MODE nRet = E_UNKNOWN_FW_MODE;
    loraSerial.begin(57600);
    delay(100);
    const char *strResult = command("AAT1 LW=?");
    if (strlen(strResult) > 0)
    {
        if ('1' == strResult[0]) {
            nRet = E_FW_AAT_MOST;
        }
        else if ('0' == strResult[0]) {
            nRet = E_FW_AAT_LORAWAN;
        }
    }
    else {
        nRet = E_FW_P1P2_MOST;
    }
    return nRet;
}

void LoraBase::setMode(int mode)
{
    if (_eMode == mode)
        return;
    
    if (E_LORA_NORMAL == mode)
        setMode(0, 0);
    else if (E_LORA_WAKEUP == mode)
        setMode(0, 1);
    else if (E_LORA_POWERSAVING == mode) {
        if (E_LORA_SETUP == _eMode) {       // Setup -> Normal -> Power Saving
            setMode(0, 0);
        }
        setMode(1, 0);
    }
    else if (E_LORA_SETUP == mode) {
        if (E_LORA_POWERSAVING == _eMode) { // Power Saving -> Normal -> Setup
            setMode(0, 0);
        }
        setMode(1, 1);
    }
    
    // assign to new state
    _eMode = mode;
}

/////////////////////////////////////////
// setup(1,1), normal(0,0), wakeup(0,1), powersaving(1,0)
void LoraBase::setMode(int p1, int p2)
{
    digitalWrite(_pinP1, p1);  // setup(1,1), normal(0,0)
    digitalWrite(_pinP2, p2);
    delay(200);
}

boolean LoraBase::available()
{
    return loraSerial.available();
}

/////////////////////////////////////////
// send data via LoRa
int LoraBase::sendData(uint8_t *data, int szData)
{
    waitUntilReady(3000);
    int nRet = loraSerial.write(data, szData);
    delay(100);
#ifdef DEBUG_LORA
    debugSerial.print(F("Send > "));
    MLutility::printBinary(data, szData);
#endif // DEBUG_LORA
    return nRet;
}

/////////////////////////////////////////
// send string via LoRa
int LoraBase::sendData(const char *strData)
{
    if (NULL == strData)
        return 0;
    
    int nRet = sendData((uint8_t*)strData, strlen(strData));
    
#ifdef DEBUG_LORA
    debugSerial.print(F(">>> "));
    debugSerial.println(strData);
#endif // DEBUG_LORA
    return nRet;
}

/////////////////////////////////////////
// run loop to rece data
void LoraBase::run()
{
    if (available()) {
        receData();
    }
}
/////////////////////////////////////////
// receive data via LoRa
int LoraBase::receData()
{
    _szBuf = 0;
    if (!loraSerial.available())
        return _szBuf;

    int i, nRssi = 0;
    for (i = 0; i < 6; i++) {
        int nCharRead = 0;
        while (loraSerial.available() && (_szBuf < MAX_SIZE_BUF)) {
            int c = loraSerial.read();
            _buf[_szBuf] = c;
            
            _szBuf++;
            nCharRead++;
            delay(1);
        }
        
        delay(300);
        if (nCharRead > 0) {
#ifdef DEBUG_LORA
            debugSerial.print(nCharRead);
            debugSerial.print(F(") "));
#endif // DEBUG_LORA
            if (E_LORA_WAKEUP == _eMode) {      // get RSSI at last character
                _szBuf--;
#ifdef DEBUG_LORA
                nRssi = _buf[_szBuf];
                debugSerial.print(nRssi);
                debugSerial.print(F(" rssi. "));
#endif // DEBUG_LORA
            }
        }
    }
    if (_szBuf > 0) {
        _buf[_szBuf] = 0;
        
#ifdef DEBUG_LORA
        debugSerial.print(F("\nRece < "));
        MLutility::printBinary(_buf, _szBuf);
        debugSerial.print(F("<<< "));
        debugSerial.println((char*)_buf);
#endif // DEBUG_LORA
    }

    return _szBuf;
}

boolean LoraBase::isBusy()
{
    boolean bRet = true;
    int nBusy = 0;
    
    if (_pinBZ >= 14) {     // analog BZ
        nBusy = analogRead(_pinBZ);
        bRet = (nBusy < 512);
    }
    else {                  // digital BZ
        const int nBusy = digitalRead(_pinBZ);
        bRet = (nBusy < 1);
    }
    
    /*    if (bRet) {
     #ifdef DEBUG_LORA
     const char *strBZ = " busy ...";
     debugSerial.print(nBusy, DEC);
     debugSerial.println(strBZ);
     #endif // DEBUG_LORA
     }
     */
    return bRet;
}

boolean LoraBase::waitUntilReady(unsigned long timeout)
{
    boolean bRet = true;
    unsigned long tsStart = millis();
    while (isBusy()) {
        delay(100);
        if (timeout < millis() - tsStart) {
            bRet = false;
            break;
        }
    }
#ifdef DEBUG_LORA
    debugSerial.print((millis() - tsStart));
    debugSerial.println(F(" Ready"));
#endif // DEBUG_LORA
    return bRet;
}

///////////////////////
// for AT command
///////////////////////
#define MAX_SIZE_CMD     60

// Flash string (to reduce memory usage in SRAM)
char *LoraBase::command(const __FlashStringHelper *strCmd)
{
    // read back a char
    MLutility::Fcopy(_strBuf, strCmd);
    return command(_strBuf);
}

char *LoraBase::command(const char *strCmd)
{
    if (NULL == strCmd || strlen(strCmd) > MAX_SIZE_CMD - 3)
        return NULL;
    
    char strFull[MAX_SIZE_CMD] = {0};
    sprintf(strFull, "%s\r\n", strCmd);     // add CR,LR
    sendData(strFull);
    
    unsigned long tsStart = millis();

    int szRece = 0;
    while (millis() - tsStart < 6000) {     // response in 6000ms
        szRece = receData();
        if (szRece > 0) {
            break;
        }
        delay(100);
    }
    _strBuf[szRece] = 0;
#ifdef DEBUG_LORA
    if (szRece > 0) {
        debugSerial.println(F("+++ AT Response is OK."));
    }
    else {
        debugSerial.println(F("!!! AT Response nothing."));
    }
#endif // DEBUG_LORA
    
    return _strBuf;
}

