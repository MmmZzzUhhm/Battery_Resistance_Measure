/*
 * LED照明 明るさ調整制御 (ADG728経由)
 *
 * 基板上のADG728(I2Cアナログデータセレクタ, addr 0x4C)が、LED1/LED2それぞれの
 * カソード側に直列に入る電流制限抵抗(10/43/150/470Ω)をI2C経由で切り替えることで
 * 明るさを調整する。ADG728はS1-S8の8チャンネルを持ち、書き込む1バイトの各bitが
 * 対応するチャンネルのON(1)/OFF(0)を表す(bitとチャンネルの対応はADG728データシート通り)。
 *   LED1: S1(bit0)=10Ω  S2(bit1)=43Ω  S3(bit2)=150Ω  S4(bit3)=470Ω
 *   LED2: S5(bit4)=10Ω  S6(bit5)=43Ω  S7(bit6)=150Ω  S8(bit7)=470Ω
 * 各LEDにつき同時に有効化するチャンネルは1つ(またはOFFで0個)のみとする。
 */
#pragma once
#include <Arduino.h>

#define LIGHT_LEVEL_OFF 0
#define LIGHT_LEVEL_MIN 1  // 最も明るい (10Ω)
#define LIGHT_LEVEL_MAX 4  // 最も暗い   (470Ω)

enum LightId {
    LIGHT_LED1 = 1,
    LIGHT_LED2 = 2,
};

void lightControlBegin();

// level: 0(消灯)〜4(最も暗い)、1が最も明るい。範囲外は端点にクランプする。
// 戻り値はADG728への書き込み(I2C)が成功したかどうか。
bool lightSetLevel(LightId led, uint8_t level);
uint8_t lightGetLevel(LightId led);

// 後方互換 (ONVIF簡易エンドポイント向け旧API): 両LEDを一括制御する。
void lightOn();   // 両LEDをLIGHT_LEVEL_MINで点灯
void lightOff();  // 両LEDを消灯
bool lightIsOn(); // いずれかのLEDが点灯していればtrue
