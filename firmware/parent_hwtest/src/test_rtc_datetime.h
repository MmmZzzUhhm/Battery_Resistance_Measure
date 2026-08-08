/*
 * RTC(PCF8563T)の現在日時を、値を書き換えずにそのまま読み取るテスト。
 * testRtcPcf8563()(設定→読出検証)より前に呼ぶこと。
 * バッテリーバックアップで前回設定した時刻が保持されているかの確認に使う。
 */
#pragma once

void testRtcReadTime();
