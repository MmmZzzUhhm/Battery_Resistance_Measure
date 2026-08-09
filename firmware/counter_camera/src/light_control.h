/*
 * 照明制御 (GPIO直結想定)
 *
 * PIN_LIGHT_CTRLはユーザーから実機ピン番号の連絡待ちのため、
 * 未定義時は安全側でno-op(GPIO操作を一切行わない)とする。
 */
#pragma once

void lightControlBegin();
void lightOn();
void lightOff();
bool lightIsOn();
