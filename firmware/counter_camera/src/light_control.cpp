#include "light_control.h"
#include <Arduino.h>

#ifndef PIN_LIGHT_CTRL
#define PIN_LIGHT_CTRL -1
#endif

namespace {
bool g_lightOn = false;
}

void lightControlBegin() {
    if (PIN_LIGHT_CTRL < 0) {
        Serial.println("[Light] WARNING: PIN_LIGHT_CTRL未設定のため照明制御は無効化 (no-op)");
        return;
    }
    pinMode(PIN_LIGHT_CTRL, OUTPUT);
    digitalWrite(PIN_LIGHT_CTRL, LOW);
    g_lightOn = false;
}

void lightOn() {
    if (PIN_LIGHT_CTRL < 0) return;
    digitalWrite(PIN_LIGHT_CTRL, HIGH);
    g_lightOn = true;
}

void lightOff() {
    if (PIN_LIGHT_CTRL < 0) return;
    digitalWrite(PIN_LIGHT_CTRL, LOW);
    g_lightOn = false;
}

bool lightIsOn() {
    return g_lightOn;
}
