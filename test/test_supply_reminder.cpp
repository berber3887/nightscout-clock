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

static void friendlyMessagesAreGentleAndAscii() {
    char text[96];
    formatFriendlySupplyMessage(text, sizeof(text), false, {SupplyState::DUE_SOON, 11 * 3600 + 20});
    assert(strcmp(text, "Coucou ! Bientot, on change le catheter, dans 11h") == 0);
    formatFriendlySupplyMessage(text, sizeof(text), true, {SupplyState::OVERDUE, -(3 * 3600)});
    assert(strcmp(text, "Coucou ! Il est temps de changer le capteur") == 0);
    for (const char* c = text; *c; c++) {
        assert(static_cast<unsigned char>(*c) < 128);
    }
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
    friendlyMessagesAreGentleAndAscii();
    roundsStartWhenDueAndRepeat();
    nightscoutTimestampsAreParsed();
    badTimestampsGiveZero();
    printf("test_supply_reminder: all tests passed\n");
    return 0;
}
