#include "sync_common.h"
#include "storage_sd.h"
#include "child_registry.h"

uint32_t processIncomingMeasurements(const String& childId, const String& batteryId, JsonArrayConst arr) {
    JsonDocument outDoc;
    JsonArray outArr = outDoc.to<JsonArray>();
    uint32_t maxSeq = 0;

    for (JsonObjectConst rec : arr) {
        int64_t  ts    = rec["ts"]     | (int64_t)0;
        float    r     = rec["r_mohm"] | -999.0f;
        float    v     = rec["v"]      | -999.0f;
        bool     valid = rec["valid"]  | false;
        uint32_t seq   = rec["seq"]    | (uint32_t)0;
        if (seq >= maxSeq) {
            maxSeq = seq;
            registryUpdateLastReading(childId, ts, r, v, valid);
        }

        sdAppendHistory(childId.c_str(), batteryId.c_str(), ts, r, v, valid);

        JsonObject o = outArr.add<JsonObject>();
        o["child_id"]    = childId;
        o["battery_id"]  = batteryId;
        o["seq"]         = seq;
        o["ts"]          = ts;
        o["r_mohm"]      = r;
        o["v"]           = v;
        o["valid"]       = valid;
    }

    if (outArr.size() > 0) {
        String out;
        serializeJson(outDoc, out);
        sdEnqueueForCloud(childId.c_str(), out);
    }
    return maxSeq;
}
