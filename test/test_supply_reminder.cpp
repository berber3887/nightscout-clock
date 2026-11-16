// Tests the logic behind the catheter and sensor change reminders.

#include <cassert>
#include <cstdio>
#include <cstring>

#include "SupplyReminder.h"

// 2026-11-10 08:00:00 UTC
static const uint32_t START = 1794297600UL;
static const uint32_t DAY = 86400UL;
static const uint32_t HOUR = 3600UL;

static void disabledOrUnknownSuppliesAreNeverDue() {
    assert(evaluateSupply(false, START, START + 10 * DAY, 3, 12).state == SupplyState::UNKNOWN);
    assert(evaluateSupply(true, 0, START + 10 * DAY, 3, 12).state == SupplyState::UNKNOWN);
    // The clock has no time yet.
    assert(evaluateSupply(true, START, 5000, 3, 12).state == SupplyState::UNKNOWN);
}

static void stateFollowsTheLifetime() {
    assert(evaluateSupply(true, START, START + 1 * DAY, 3, 12).state == SupplyState::OK);
    // Warning begins exactly 12 hours before the end of the 3 days.
    assert(evaluateSupply(true, START, START + 3 * DAY - 12 * HOUR - 1, 3, 12).state == SupplyState::OK);
    assert(evaluateSupply(true, START, START + 3 * DAY - 12 * HOUR, 3, 12).state == SupplyState::DUE_SOON);
    assert(evaluateSupply(true, START, START + 3 * DAY - 1, 3, 12).state == SupplyState::DUE_SOON);
    assert(evaluateSupply(true, START, START + 3 * DAY, 3, 12).state == SupplyState::OVERDUE);
    assert(evaluateSupply(true, START, START + 4 * DAY, 3, 12).state == SupplyState::OVERDUE);
}

static void remainingSecondsAreSigned() {
    SupplyStatus soon = evaluateSupply(true, START, START + 3 * DAY - 2 * HOUR, 3, 12);
    assert(soon.remainingSeconds == 2 * HOUR);
    SupplyStatus late = evaluateSupply(true, START, START + 3 * DAY + 5 * HOUR, 3, 12);
    assert(late.remainingSeconds == -static_cast<long>(5 * HOUR));
}

static void sensorUsesSevenDays() {
    assert(evaluateSupply(true, START, START + 6 * DAY, 7, 12).state == SupplyState::OK);
    assert(evaluateSupply(true, START, START + 6 * DAY + 13 * HOUR, 7, 12).state == SupplyState::DUE_SOON);
    assert(evaluateSupply(true, START, START + 7 * DAY, 7, 12).state == SupplyState::OVERDUE);
}

static void aWiderWarningStartsEarlier() {
    assert(evaluateSupply(true, START, START + 3 * DAY - 24 * HOUR, 3, 24).state == SupplyState::DUE_SOON);
    assert(evaluateSupply(true, START, START + 3 * DAY - 24 * HOUR, 3, 12).state == SupplyState::OK);
}

static void newestStartWins() {
    assert(latestSupplyStart(0, 0) == 0);
    assert(latestSupplyStart(100, 0) == 100);
    assert(latestSupplyStart(0, 200) == 200);
    assert(latestSupplyStart(300, 200) == 300);
    assert(latestSupplyStart(100, 200) == 200);
}

static void durationsAreShort() {
    char text[16];
    formatSupplyDuration(text, sizeof(text), 10);
    assert(strcmp(text, "1min") == 0);
    formatSupplyDuration(text, sizeof(text), 45 * 60 + 30);
    assert(strcmp(text, "45min") == 0);
    formatSupplyDuration(text, sizeof(text), 3600);
    assert(strcmp(text, "1h") == 0);
    formatSupplyDuration(text, sizeof(text), 11 * 3600 + 1800);
    assert(strcmp(text, "11h") == 0);
    formatSupplyDuration(text, sizeof(text), 47 * 3600);
    assert(strcmp(text, "47h") == 0);
    formatSupplyDuration(text, sizeof(text), 3 * 86400L + 3600);
    assert(strcmp(text, "3d") == 0);
    formatSupplyDuration(text, sizeof(text), -5 * 3600);
    assert(strcmp(text, "5h") == 0);
}

static void friendlyMessagesUseAccentsAndVary() {
    char text[160];
    // "é" and "ô" become the font's Latin-1 capitals É (0xC9) and Ô (0xD4).
    formatFriendlySupplyMessage(text, sizeof(text), false, {SupplyState::DUE_SOON, 11 * 3600 + 20}, 0);
    assert(strcmp(text, "Coucou ! Bient\xD4t, on change le cath\xC9ter, dans 11h") == 0);
    formatFriendlySupplyMessage(text, sizeof(text), true, {SupplyState::OVERDUE, -(3 * 3600)}, 0);
    assert(strcmp(text, "Coucou ! Il est temps de changer le capteur") == 0);

    // Every sentence of both states fits, names the supply and has no UTF-8 left over.
    for (int overdue = 0; overdue < 2; overdue++) {
        const SupplyStatus status = overdue ? SupplyStatus{SupplyState::OVERDUE, -7200}
                                            : SupplyStatus{SupplyState::DUE_SOON, 7200};
        assert(friendlyVariantCount(overdue) >= 8);
        for (unsigned v = 0; v < friendlyVariantCount(overdue); v++) {
            formatFriendlySupplyMessage(text, sizeof(text), true, status, v);
            assert(strstr(text, "le capteur") != nullptr);
            assert(strstr(text, "{") == nullptr);
            assert(strlen(text) < 100);
            for (const char* c = text; *c; c++) {
                assert(static_cast<unsigned char>(*c) != 0xC3);
            }
            if (!overdue) {
                assert(strstr(text, "2h") != nullptr);
            }
        }
    }
    // The variants differ, and the choice wraps around.
    char other[160];
    formatFriendlySupplyMessage(text, sizeof(text), false, {SupplyState::DUE_SOON, 7200}, 1);
    formatFriendlySupplyMessage(other, sizeof(other), false, {SupplyState::DUE_SOON, 7200}, 2);
    assert(strcmp(text, other) != 0);
    formatFriendlySupplyMessage(
        other, sizeof(other), false, {SupplyState::DUE_SOON, 7200}, 1 + friendlyVariantCount(false));
    assert(strcmp(text, other) == 0);
}

static void timestampsRoundTripAndHistoryMerges() {
    char stamp[32];
    formatNightscoutTimestamp(stamp, sizeof(stamp), 1794744000);
    assert(parseNightscoutTimestamp(stamp) == 1794744000);
    formatNightscoutTimestamp(stamp, sizeof(stamp), 1583020800 + 86400 + 3661);  // leap day 2020 + 1 day
    assert(strcmp(stamp, "2020-03-02T01:01:01.000Z") == 0);
    for (uint32_t e = 1700000000; e < 1900000000; e += 7777777) {
        formatNightscoutTimestamp(stamp, sizeof(stamp), e);
        assert(parseNightscoutTimestamp(stamp) == e);
    }

    const uint32_t starts[] = {9000000, 5000000, 1000000};
    const uint32_t changes[] = {8999000, 7000000, 3000000};
    uint32_t merged[4];
    const int kept = mergeSupplyHistory(starts, 3, changes, 3, merged, 4);
    // 8999000 is within an hour of 9000000 and is dropped.
    assert(kept == 4);
    assert(merged[0] == 9000000 && merged[1] == 7000000 && merged[2] == 5000000 && merged[3] == 3000000);
    uint32_t few[5];
    assert(mergeSupplyHistory(starts, 1, changes, 0, few, 5) == 1);
    assert(few[0] == 9000000 && few[1] == 0 && few[4] == 0);
}

static void correctionIgnoresMistakenNotes() {
    // Real change on day 100, tests on day 200 (notes at 200, 201, 202), correction made at 210.
    uint32_t list[5] = {202, 201, 200, 100, 50};
    int kept = dropIgnoredSupplyChanges(list, 5, 100, 210);
    assert(kept == 2 && list[0] == 100 && list[1] == 50 && list[2] == 0);
    // A change after the correction still counts.
    uint32_t later[5] = {300, 202, 100, 0, 0};
    kept = dropIgnoredSupplyChanges(later, 3, 100, 210);
    assert(kept == 2 && later[0] == 300 && later[1] == 100);
    // Nothing to ignore without a correction.
    uint32_t plain[3] = {9, 8, 7};
    assert(dropIgnoredSupplyChanges(plain, 3, 5, 0) == 3);
    // A newer local confirmation: the window is empty.
    uint32_t newer[3] = {250, 202, 100};
    assert(dropIgnoredSupplyChanges(newer, 3, 250, 210) == 3);
}

static void accentsBecomeLatin1Capitals() {
    char text[] = "é è ê à ô ç î ï û ù É ! a";
    utf8ToLatin1Capitals(text);
    assert(strcmp(text, "\xC9 \xC8 \xCA \xC0 \xD4 \xC7 \xCE \xCF \xDB \xD9 \xC9 ! a") == 0);
}

static void messagesNameTheSupply() {
    char text[48];
    formatSupplyMessage(text, sizeof(text), "CATHETER", {SupplyState::DUE_SOON, 11 * 3600 + 20});
    assert(strcmp(text, "Change CATHETER in 11h") == 0);
    formatSupplyMessage(text, sizeof(text), "SENSOR", {SupplyState::OVERDUE, -(3 * 3600 + 5)});
    assert(strcmp(text, "SENSOR overdue 3h") == 0);
    formatSupplyMessage(text, sizeof(text), "SENSOR", {SupplyState::DUE_SOON, 90});
    assert(strcmp(text, "Change SENSOR in 1min") == 0);
}

static void roundsStartWhenDueAndRepeat() {
    const unsigned long minute = 60000UL;
    assert(!supplyRoundDue(false, false, 1000, 0, 10));
    assert(!supplyRoundDue(false, true, 99 * minute, 0, 10));
    // Newly due: shown at once.
    assert(supplyRoundDue(true, false, 1000, 0, 10));
    // Shown already: wait for the repeat interval.
    assert(!supplyRoundDue(true, true, 10 * minute - 1, 0, 10));
    assert(supplyRoundDue(true, true, 10 * minute, 0, 10));
    assert(supplyRoundDue(true, true, 5 * minute + 3, 3, 5));
}

static void nightscoutTimestampsAreParsed() {
    assert(parseNightscoutTimestamp("2026-11-10T08:00:00.000Z") == START);
    assert(parseNightscoutTimestamp("2026-11-10T08:00:00Z") == START);
    assert(parseNightscoutTimestamp("2026-11-10T08:00Z") == START);
    // The same instant written with offsets.
    assert(parseNightscoutTimestamp("2026-11-10T09:00:00.000+01:00") == START);
    assert(parseNightscoutTimestamp("2026-11-10T09:00:00+0100") == START);
    assert(parseNightscoutTimestamp("2026-11-10T03:00:00-05:00") == START);
    // Leap day and a year boundary.
    assert(parseNightscoutTimestamp("2028-02-29T00:00:00Z") == 1835395200UL);
    assert(parseNightscoutTimestamp("2027-01-01T00:00:00Z") == 1798761600UL);
}

static void badTimestampsGiveZero() {
    assert(parseNightscoutTimestamp(nullptr) == 0);
    assert(parseNightscoutTimestamp("") == 0);
    assert(parseNightscoutTimestamp("yesterday") == 0);
    assert(parseNightscoutTimestamp("2026-13-10T08:00:00Z") == 0);
    assert(parseNightscoutTimestamp("2026-11-10T25:00:00Z") == 0);
    assert(parseNightscoutTimestamp("1969-11-10T08:00:00Z") == 0);
}

int main() {
    disabledOrUnknownSuppliesAreNeverDue();
    stateFollowsTheLifetime();
    remainingSecondsAreSigned();
    sensorUsesSevenDays();
    aWiderWarningStartsEarlier();
    newestStartWins();
    durationsAreShort();
    messagesNameTheSupply();
    friendlyMessagesUseAccentsAndVary();
    accentsBecomeLatin1Capitals();
    correctionIgnoresMistakenNotes();
    timestampsRoundTripAndHistoryMerges();
    roundsStartWhenDueAndRepeat();
    nightscoutTimestampsAreParsed();
    badTimestampsGiveZero();
    printf("test_supply_reminder: all tests passed\n");
    return 0;
}
