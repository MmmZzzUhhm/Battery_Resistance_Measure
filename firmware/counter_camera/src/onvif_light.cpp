#include "onvif_light.h"
#include "http_server.h"
#include "light_control.h"

namespace {

void handleLightOn() {
    lightOn();
    httpServer.send(200, "application/json", "{\"ok\":true,\"state\":\"on\"}");
}

void handleLightOff() {
    lightOff();
    httpServer.send(200, "application/json", "{\"ok\":true,\"state\":\"off\"}");
}

void handleLightStatus() {
    char out[96];
    snprintf(out, sizeof(out), "{\"on\":%s,\"led1\":%u,\"led2\":%u}",
        lightIsOn() ? "true" : "false", lightGetLevel(LIGHT_LED1), lightGetLevel(LIGHT_LED2));
    httpServer.send(200, "application/json", out);
}

// /onvif/light/set?led=1|2&level=0-4
void handleLightSet() {
    if (!httpServer.hasArg("led") || !httpServer.hasArg("level")) {
        httpServer.send(400, "application/json", "{\"ok\":false,\"error\":\"led and level required\"}");
        return;
    }
    int ledArg = httpServer.arg("led").toInt();
    int levelArg = httpServer.arg("level").toInt();
    if (ledArg != LIGHT_LED1 && ledArg != LIGHT_LED2) {
        httpServer.send(400, "application/json", "{\"ok\":false,\"error\":\"led must be 1 or 2\"}");
        return;
    }
    if (levelArg < 0 || levelArg > LIGHT_LEVEL_MAX) {
        httpServer.send(400, "application/json", "{\"ok\":false,\"error\":\"level must be 0-4\"}");
        return;
    }
    LightId led = (LightId)ledArg;
    bool ok = lightSetLevel(led, (uint8_t)levelArg);
    char out[64];
    snprintf(out, sizeof(out), "{\"ok\":%s,\"led\":%d,\"level\":%u}", ok ? "true" : "false", ledArg, lightGetLevel(led));
    httpServer.send(ok ? 200 : 500, "application/json", out);
}

} // namespace

void onvifLightRegisterRoutes() {
    httpServer.on("/onvif/light/on",     HTTP_GET, handleLightOn);
    httpServer.on("/onvif/light/off",    HTTP_GET, handleLightOff);
    httpServer.on("/onvif/light/status", HTTP_GET, handleLightStatus);
    httpServer.on("/onvif/light/set",    HTTP_GET, handleLightSet);
}
