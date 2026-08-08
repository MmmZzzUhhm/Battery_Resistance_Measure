#include "hwtest_common.h"

static TestResult g_results[16];
static int        g_resultCount = 0;

void testResultsReset() {
    g_resultCount = 0;
}

void report(const char* name, bool pass, const String& detail) {
    if (g_resultCount >= (int)(sizeof(g_results) / sizeof(g_results[0]))) return;
    g_results[g_resultCount++] = { name, pass, detail };
    Serial.printf("[%s] %-24s %s\n", pass ? " OK " : "FAIL", name, detail.c_str());
}

int testResultCount() {
    return g_resultCount;
}

const TestResult& testResultAt(int i) {
    return g_results[i];
}

void printSummary() {
    int passCount = 0;
    Serial.println("\n==================== 検査結果サマリ ====================");
    for (int i = 0; i < g_resultCount; i++) {
        Serial.printf("  [%s] %s\n", g_results[i].pass ? " OK " : "FAIL", g_results[i].name);
        if (g_results[i].pass) passCount++;
    }
    Serial.printf("---------------------------------------------------------\n");
    Serial.printf("  %d / %d 項目 PASS\n", passCount, g_resultCount);
    Serial.println("==========================================================\n");
}
