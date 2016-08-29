#include "MLPacketGen.h"

MLPayloadGen::MLPayloadGen(uint16_t cmdId, uint16_t resInterval,uint8_t dataLen, uint8_t *data, uint8_t version) {
    _version = version;
    _cmdId = cmdId;
    _resInterval = resInterval;
    _dataLen = dataLen;
    memcpy(_data, data, dataLen);
}

int MLPayloadGen::getPayload(uint8_t *payload) {
    payload[0] = _version;
    payload[1] = _cmdId >> 8;
    payload[2] = _cmdId & 0x0F;
    payload[3] = _resInterval >> 8;
    payload[4] = _resInterval & 0x0F;
    payload[5] = _dataLen;
    memcpy(&payload[6], _data, _dataLen);
    return _dataLen + 6;
}

MLPacketGen::MLPacketGen(uint8_t ackBit, uint8_t receiverFlag, uint8_t packetType, uint8_t direction, uint64_t receiverID, uint64_t senderID, uint8_t version) {
    _ackBit = ackBit;
    _receiverFlag = receiverFlag;
    _packetType = packetType;
    _direction = direction;
    _receiverID = receiverID;
    _senderID = senderID;
    _version = version;
    _mlPayloadGen = NULL;
}

int MLPacketGen::setMLPayload(uint16_t cmdId, uint16_t resInterval,uint8_t dataLen, uint8_t *data, uint8_t version) {
    _mlPayloadGen = new MLPayloadGen(cmdId, resInterval, dataLen, data, version);
    return 0;
}

int MLPacketGen::getMLPayload(uint8_t *payload) {
    if (_mlPayloadGen == NULL)
        return 0;
    else
        return _mlPayloadGen->getPayload(payload);
}

int MLPacketGen::getMLPacket(uint8_t *mlpacket) {
    uint8_t pos = 0;
    uint8_t mlpayload[84];

    mlpacket[0] = 0xFB;
    mlpacket[1] = 0xFC;
    mlpacket[2] = _version;
    mlpacket[4] = (_direction & 0x01) << 3 | (_packetType & 0x01) << 2 | (_receiverFlag & 0x01) << 1 | (_ackBit & 0x01);
    mlpacket[5] = (_receiverID & 0xFF00000000000000) >> 56;
    mlpacket[6] = (_receiverID & 0x00FF000000000000) >> 48;
    mlpacket[7] = (_receiverID & 0x0000FF0000000000) >> 40;
    mlpacket[8] = (_receiverID & 0x000000FF00000000) >> 32;
    mlpacket[9] = (_receiverID & 0x00000000FF000000) >> 24;
    mlpacket[10] = (_receiverID & 0x0000000000FF0000) >> 16;
    mlpacket[11] = (_receiverID & 0x000000000000FF00) >> 8;
    mlpacket[12] = _receiverID & 0x00000000000000FF;
    pos = 13;
    if (_direction == 1) {
        mlpacket[13] = (_senderID & 0xFF00000000000000) >> 56;
        mlpacket[14] = (_senderID & 0x00FF000000000000) >> 48;
        mlpacket[15] = (_senderID & 0x0000FF0000000000) >> 40;
        mlpacket[16] = (_senderID & 0x000000FF00000000) >> 32;
        mlpacket[17] = (_senderID & 0x00000000FF000000) >> 24;
        mlpacket[18] = (_senderID & 0x0000000000FF0000) >> 16;
        mlpacket[19] = (_senderID & 0x000000000000FF00) >> 8;
        mlpacket[20] = _senderID & 0x00000000000000FF;
        pos = 21;
    }
    uint8_t payloadLen = getMLPayload(mlpayload);
    if (payloadLen <=0)
        return -1;
    uint8_t totalLen = payloadLen + pos + 1;
    mlpacket[3] = totalLen;
    mlpacket[pos] = getCrc(mlpacket, pos);
    memcpy(&mlpacket[pos+1], mlpayload, payloadLen);
    mlpacket[totalLen-1] = getCrc(mlpacket, totalLen-1);
    return totalLen;
}

uint8_t MLPacketGen::getCrc(const uint8_t *dataBuffer, const uint8_t length) {									
    uint8_t crc = 0;

    for (uint8_t i=0; i<length; i++) {
        crc^=dataBuffer[i];
    }
    printf("%x\n", crc);
    return crc;
}

