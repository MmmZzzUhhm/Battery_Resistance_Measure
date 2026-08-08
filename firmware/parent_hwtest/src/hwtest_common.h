/*
 * 動作確認チェックアプリ共通: 検査結果の記録とサマリ表示
 */
#pragma once
#include <Arduino.h>

struct TestResult {
    const char* name;
    bool        pass;
    String      detail;
};

void testResultsReset();
void report(const char* name, bool pass, const String& detail);
int  testResultCount();
const TestResult& testResultAt(int i);
void printSummary();
