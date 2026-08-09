#include "onvif_routes.h"
#include "http_server.h"
#include "onvif_device_service.h"
#include "onvif_media_service.h"
#include "onvif_imaging_service.h"
#include "onvif_light.h"

namespace {

void handleDeviceService() {
    String body = httpServer.hasArg("plain") ? httpServer.arg("plain") : "";
    httpServer.send(200, "application/soap+xml", onvifHandleDeviceService(body));
}

void handleMediaService() {
    String body = httpServer.hasArg("plain") ? httpServer.arg("plain") : "";
    httpServer.send(200, "application/soap+xml", onvifHandleMediaService(body));
}

void handleImagingService() {
    String body = httpServer.hasArg("plain") ? httpServer.arg("plain") : "";
    httpServer.send(200, "application/soap+xml", onvifHandleImagingService(body));
}

} // namespace

void onvifRegisterRoutes() {
    httpServer.on("/onvif/device_service",  HTTP_POST, handleDeviceService);
    httpServer.on("/onvif/media_service",   HTTP_POST, handleMediaService);
    httpServer.on("/onvif/imaging_service", HTTP_POST, handleImagingService);
    httpServer.on("/onvif/snapshot",        HTTP_GET,  onvifHandleSnapshotRoute);
    onvifLightRegisterRoutes();
}
