#include "pending_queue.h"
#include <Preferences.h>
#include <ArduinoJson.h>

PendingQueue pendingQueue;
static Preferences prefs;
static const char* NVS_NS  = "childqueue";
static const char* NVS_KEY = "blob";

void PendingQueue::load() {
    prefs.begin(NVS_NS, true);
    size_t len = prefs.getBytesLength(NVS_KEY);
    if (len == sizeof(nextSeq_) + sizeof(count_) + sizeof(records_)) {
        uint8_t buf[sizeof(nextSeq_) + sizeof(count_) + sizeof(records_)];
        prefs.getBytes(NVS_KEY, buf, sizeof(buf));
        size_t off = 0;
        memcpy(&nextSeq_, buf + off, sizeof(nextSeq_)); off += sizeof(nextSeq_);
        memcpy(&count_,   buf + off, sizeof(count_));   off += sizeof(count_);
        memcpy(records_,  buf + off, sizeof(records_));
    } else {
        nextSeq_ = 1;
        count_   = 0;
    }
    prefs.end();
}

void PendingQueue::save() {
    prefs.begin(NVS_NS, false);
    uint8_t buf[sizeof(nextSeq_) + sizeof(count_) + sizeof(records_)];
    size_t off = 0;
    memcpy(buf + off, &nextSeq_, sizeof(nextSeq_)); off += sizeof(nextSeq_);
    memcpy(buf + off, &count_,   sizeof(count_));   off += sizeof(count_);
    memcpy(buf + off, records_,  sizeof(records_));
    prefs.putBytes(NVS_KEY, buf, sizeof(buf));
    prefs.end();
}

void PendingQueue::push(const IwsMeasurement& m, int64_t ts) {
    if (count_ >= PENDING_QUEUE_MAX_RECORDS) {
        // 満杯: 最古の1件を捨てて詰める (電池消耗より欠損を許容)
        memmove(&records_[0], &records_[1], sizeof(Record) * (PENDING_QUEUE_MAX_RECORDS - 1));
        count_--;
    }
    Record& r = records_[count_++];
    r.seq    = nextSeq_++;
    r.ts     = ts;
    r.r_mohm = m.r_mohm;
    r.v      = m.v;
    r.valid  = m.valid ? 1 : 0;
}

String PendingQueue::toJsonArray(size_t maxCount) const {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    size_t n = min(maxCount, (size_t)count_);
    for (size_t i = 0; i < n; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["seq"]    = records_[i].seq;
        o["ts"]     = records_[i].ts;
        o["r_mohm"] = records_[i].r_mohm;
        o["v"]      = records_[i].v;
        o["valid"]  = (bool)records_[i].valid;
    }
    String out;
    serializeJson(doc, out);
    return out;
}

void PendingQueue::ackSeq(uint32_t ackSeq) {
    uint16_t keepFrom = 0;
    while (keepFrom < count_ && records_[keepFrom].seq <= ackSeq) keepFrom++;
    if (keepFrom == 0) return;
    uint16_t remaining = count_ - keepFrom;
    if (remaining > 0) memmove(&records_[0], &records_[keepFrom], sizeof(Record) * remaining);
    count_ = remaining;
    save();
}
