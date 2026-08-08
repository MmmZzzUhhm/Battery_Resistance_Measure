#include "iws7817.h"
#include <Wire.h>

#define IWS7817_BYTES 10
#define IWS7817_HDR0  0x49
#define IWS7817_HDR1  0x57

IwsMeasurement readIWS7817(uint8_t i2cAddr) {
    IwsMeasurement d = {};
    uint8_t buf[IWS7817_BYTES];
    uint8_t n = Wire.requestFrom(i2cAddr, (uint8_t)IWS7817_BYTES);
    if (n != IWS7817_BYTES) {
        Serial.printf("[IWS7817] Read failed: %d/%d bytes\n", n, IWS7817_BYTES);
        return d;
    }
    for (int i = 0; i < IWS7817_BYTES; i++) buf[i] = Wire.read();
    if (buf[0] != IWS7817_HDR0 || buf[1] != IWS7817_HDR1) {
        Serial.printf("[IWS7817] Bad header: %02X %02X\n", buf[0], buf[1]);
        return d;
    }
    // ビッグエンディアン float → リトルエンディアン
    uint8_t rb[4] = {buf[5], buf[4], buf[3], buf[2]};
    uint8_t vb[4] = {buf[9], buf[8], buf[7], buf[6]};
    memcpy(&d.r_mohm, rb, 4);
    memcpy(&d.v,      vb, 4);
    d.valid = true;
    return d;
}
