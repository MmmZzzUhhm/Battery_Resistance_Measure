#include "test_sht31.h"
#include "hwtest_common.h"
#include <Wire.h>
#include <Adafruit_SHT31.h>

#ifndef SHT31_I2C_ADDR
#define SHT31_I2C_ADDR 0x44
#endif

void testSht31() {
    Adafruit_SHT31 sht31;
    if (!sht31.begin(SHT31_I2C_ADDR)) {
        char d[80];
        snprintf(d, sizeof(d), "begin()失敗 - I2Cバス上に見つからない (addr 0x%02X)", SHT31_I2C_ADDR);
        report("SHT31 温湿度", false, d);
        return;
    }

    float t = sht31.readTemperature();
    float h = sht31.readHumidity();
    bool ok = !isnan(t) && !isnan(h);
    char d[64];
    if (ok) {
        snprintf(d, sizeof(d), "温度=%.2f C 湿度=%.1f %%RH", t, h);
    } else {
        snprintf(d, sizeof(d), "読み取り失敗 (NaN) - 配線/電源を確認");
    }
    report("SHT31 温湿度", ok, d);
}
