#pragma once
#include <Arduino.h>

// SOAPリクエストボディ(<SOAP-ENV:Body>内)を受け取り、SOAP応答(Envelope込み)を返す。
String onvifHandleDeviceService(const String& requestBody);
