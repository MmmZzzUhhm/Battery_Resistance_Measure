#include "child_registry.h"

namespace {

struct Entry {
    bool   used = false;
    String childId;
    PendingChildUpdate pending;
    ChildLastReading   last;
    bool   seen = false;
};

Entry g_entries[MAX_TRACKED_CHILDREN];

Entry* findOrCreate(const String& childId) {
    Entry* free_ = nullptr;
    for (auto& e : g_entries) {
        if (e.used && e.childId == childId) return &e;
        if (!e.used && !free_) free_ = &e;
    }
    if (free_) {
        free_->used = true;
        free_->childId = childId;
        return free_;
    }
    return nullptr; // 満杯 (MAX_TRACKED_CHILDREN超過)
}

} // namespace

void registrySetPendingConfig(const String& childId, const String& configJson) {
    Entry* e = findOrCreate(childId);
    if (!e) return;
    e->pending.hasConfig = true;
    e->pending.configJson = configJson;
}

void registrySetPendingOta(const String& childId, const String& version, size_t size, const String& md5) {
    Entry* e = findOrCreate(childId);
    if (!e) return;
    e->pending.hasOta = true;
    e->pending.otaVersion = version;
    e->pending.otaSize = size;
    e->pending.otaMd5 = md5;
}

void registryClearPendingOta(const String& childId) {
    Entry* e = findOrCreate(childId);
    if (!e) return;
    e->pending.hasOta = false;
}

bool registryGetPending(const String& childId, PendingChildUpdate& out) {
    for (auto& e : g_entries) {
        if (e.used && e.childId == childId) {
            out = e.pending;
            return true;
        }
    }
    return false;
}

void registryMarkSeen(const String& childId) {
    Entry* e = findOrCreate(childId);
    if (!e) return;
    e->seen = true;
}

void registryUpdateLastReading(const String& childId, int64_t ts, float rMohm, float v, bool valid) {
    Entry* e = findOrCreate(childId);
    if (!e) return;
    e->last.valid  = valid;
    e->last.ts     = ts;
    e->last.r_mohm = rMohm;
    e->last.v      = v;
}

bool registryGetLastReading(const String& childId, ChildLastReading& out) {
    for (auto& e : g_entries) {
        if (e.used && e.childId == childId) {
            out = e.last;
            return true;
        }
    }
    return false;
}

int registrySeenCount() {
    int n = 0;
    for (auto& e : g_entries) if (e.used && e.seen) n++;
    return n;
}

String registrySeenChildId(int index) {
    int i = 0;
    for (auto& e : g_entries) {
        if (e.used && e.seen) {
            if (i == index) return e.childId;
            i++;
        }
    }
    return "";
}
