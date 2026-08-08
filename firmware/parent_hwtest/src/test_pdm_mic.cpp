/*
 * SPH0641LU4H-1 (PDM MEMSマイク) 動作確認
 *
 * 参考実装 (M5_UltraSonicMic) は ESP-IDF legacy I2S driver (driver/i2s.h) の
 * PDM RXクロック関連レジスタが自動設定されない不具合を踏み、レジスタ直叩きの
 * ワークアラウンドが必要だった (結局安定動作せず)。
 * 新実装では ESP-IDF v5系の新PDM RX API (driver/i2s_pdm.h) を使用し、
 * レジスタ直叩みを避ける。
 *
 * 配線: SEL=GND (左チャンネル出力、CLK立下りエッジでデータ確定)
 */
#include "test_pdm_mic.h"
#include "hwtest_common.h"
#include <Arduino.h>
#include <driver/i2s_pdm.h>

#ifndef PIN_PDM_CLK
#define PIN_PDM_CLK 1
#endif
#ifndef PIN_PDM_DATA
#define PIN_PDM_DATA 2
#endif
#ifndef PDM_SAMPLE_RATE_HZ
#define PDM_SAMPLE_RATE_HZ 48000
#endif

void testPdmMic() {
    i2s_chan_handle_t rxHandle = nullptr;
    i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);

    if (i2s_new_channel(&chanCfg, nullptr, &rxHandle) != ESP_OK) {
        report("PDM Mic (SPH0641LU4H-1)", false, "i2s_new_channel失敗");
        return;
    }

    i2s_pdm_rx_gpio_config_t gpioCfg = {};
    gpioCfg.clk = (gpio_num_t)PIN_PDM_CLK;
    gpioCfg.din = (gpio_num_t)PIN_PDM_DATA;
    gpioCfg.invert_flags.clk_inv = false;

    i2s_pdm_rx_config_t pdmCfg = {};
    pdmCfg.clk_cfg  = I2S_PDM_RX_CLK_DEFAULT_CONFIG(PDM_SAMPLE_RATE_HZ);
    pdmCfg.slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
    pdmCfg.gpio_cfg = gpioCfg;

    if (i2s_channel_init_pdm_rx_mode(rxHandle, &pdmCfg) != ESP_OK) {
        report("PDM Mic (SPH0641LU4H-1)", false, "PDM RXモード初期化失敗");
        i2s_del_channel(rxHandle);
        return;
    }
    if (i2s_channel_enable(rxHandle) != ESP_OK) {
        report("PDM Mic (SPH0641LU4H-1)", false, "チャンネル有効化失敗");
        i2s_del_channel(rxHandle);
        return;
    }

    static int16_t buf[1024];
    int16_t  peak       = 0;
    int64_t  sumAbs      = 0;
    size_t   totalSamples = 0;
    unsigned long start = millis();
    while (millis() - start < 500) {
        size_t bytesRead = 0;
        if (i2s_channel_read(rxHandle, buf, sizeof(buf), &bytesRead, pdMS_TO_TICKS(200)) == ESP_OK) {
            size_t n = bytesRead / sizeof(int16_t);
            for (size_t i = 0; i < n; i++) {
                int16_t a = (int16_t)abs((int)buf[i]);
                if (a > peak) peak = a;
                sumAbs += a;
            }
            totalSamples += n;
        }
    }

    i2s_channel_disable(rxHandle);
    i2s_del_channel(rxHandle);

    int32_t avgAbs = (totalSamples > 0) ? (int32_t)(sumAbs / (int64_t)totalSamples) : 0;
    bool ok = (totalSamples > 0) && (peak > 20);
    char d[128];
    snprintf(d, sizeof(d), "サンプル数=%u peak=%d avg=%d (CLK=%d DATA=%d)%s",
        (unsigned)totalSamples, peak, avgAbs, PIN_PDM_CLK, PIN_PDM_DATA,
        ok ? "" : " - 無音/未検出: 配線・SELピン(GND)を確認");
    report("PDM Mic (SPH0641LU4H-1)", ok, d);
}
