/*
 * 照明制御用の簡易HTTPエンドポイント (ONVIF標準サービスではない)
 * 実機21215Pカメラのstatus.xml方式(基本設計書3.2/3.3)に相当する、
 * このデバイス独自の簡易インターフェース。
 *
 * /onvif/light/on|off     : 両LED一括ON(最明)/OFF (後方互換)
 * /onvif/light/status     : {"on":bool,"led1":0-4,"led2":0-4}
 * /onvif/light/set        : ?led=1|2&level=0-4 でLED毎に明るさレベルを設定
 */
#pragma once

void onvifLightRegisterRoutes();
