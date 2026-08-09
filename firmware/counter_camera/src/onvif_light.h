/*
 * 照明制御用の簡易HTTPエンドポイント (ONVIF標準サービスではない)
 * 実機21215Pカメラのstatus.xml方式(基本設計書3.2/3.3)に相当する、
 * このデバイス独自の簡易インターフェース。
 */
#pragma once

void onvifLightRegisterRoutes();
