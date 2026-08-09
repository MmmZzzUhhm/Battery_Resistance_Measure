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
    String out = String("{\"on\":") + (lightIsOn() ? "true" : "false") + "}";
    httpServer.send(200, "application/json", out);
}

} // namespace

void onvifLightRegisterRoutes() {
    httpServer.on("/onvif/light/on",     HTTP_GET, handleLightOn);
    httpServer.on("/onvif/light/off",    HTTP_GET, handleLightOff);
    httpServer.on("/onvif/light/status", HTTP_GET, handleLightStatus);
}
