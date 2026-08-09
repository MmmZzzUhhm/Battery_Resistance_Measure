#include "onvif_common.h"
#include <WiFi.h>

const char* ONVIF_MANUFACTURER    = "Seiko Solutions";
const char* ONVIF_MODEL           = "CounterCamera-Thin";
const char* ONVIF_FIRMWARE_VERSION = "1.0.0";

bool soapBodyContainsAction(const String& body, const char* actionName) {
    return body.indexOf(actionName) >= 0;
}

String soapEnvelopeWrap(const String& innerXml) {
    String out;
    out.reserve(innerXml.length() + 512);
    out += F("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
              "<SOAP-ENV:Envelope"
              " xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\""
              " xmlns:SOAP-ENC=\"http://www.w3.org/2003/05/soap-encoding\""
              " xmlns:tt=\"http://www.onvif.org/ver10/schema\""
              " xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\""
              " xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\""
              " xmlns:timg=\"http://www.onvif.org/ver20/imaging/wsdl\">"
              "<SOAP-ENV:Header/>"
              "<SOAP-ENV:Body>");
    out += innerXml;
    out += F("</SOAP-ENV:Body></SOAP-ENV:Envelope>");
    return out;
}

String soapFault(const char* reason) {
    String inner = "<SOAP-ENV:Fault><SOAP-ENV:Code><SOAP-ENV:Value>SOAP-ENV:Receiver</SOAP-ENV:Value></SOAP-ENV:Code>"
                   "<SOAP-ENV:Reason><SOAP-ENV:Text xml:lang=\"en\">";
    inner += reason;
    inner += "</SOAP-ENV:Text></SOAP-ENV:Reason></SOAP-ENV:Fault>";
    return soapEnvelopeWrap(inner);
}

String onvifDeviceBaseUrl() {
    IPAddress ip = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP() : WiFi.softAPIP();
    return "http://" + ip.toString();
}
