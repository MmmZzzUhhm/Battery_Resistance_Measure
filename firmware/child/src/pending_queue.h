/*
 * 未送信測定データキュー (NVS/Preferencesにblob保存、電源断/Deep Sleepをまたいで保持)
 * docs/protocol.md §1 の測定データスキーマに対応。
 */
#pragma once
#include <Arduino.h>
#include "iws7817.h"
#include "protocol.h"

class PendingQueue {
public:
    void   load();
    void   save();
    void   push(const IwsMeasurement& m, int64_t ts);
    size_t size() const { return count_; }

    // 先頭(古い順)から最大maxCount件をJSON配列文字列にする
    String toJsonArray(size_t maxCount) const;

    // seq <= ackSeq の項目をすべて破棄する
    void ackSeq(uint32_t ackSeq);

private:
    struct Record {
        uint32_t seq;
        int64_t  ts;
        float    r_mohm;
        float    v;
        uint8_t  valid;
    };

    uint32_t nextSeq_ = 1;
    uint16_t count_   = 0;
    Record   records_[PENDING_QUEUE_MAX_RECORDS];
};

extern PendingQueue pendingQueue;
