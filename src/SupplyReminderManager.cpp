#include "SupplyReminderManager.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Hash.h>
#include <LittleFS.h>
#include <WiFiClientSecure.h>

#include <memory>

#include "BGAlarmManager.h"
#include "DisplayManager.h"
#include "PeripheryManager.h"
#include "ServerManager.h"
#include "SettingsManager.h"
#include "globals.h"

namespace {
const char* SUPPLIES_FILE = "/supplies.json";
const unsigned long SYNC_INTERVAL_MS = 30UL * 60UL * 1000UL;
const unsigned long SYNC_RETRY_MS = 5UL * 60UL * 1000UL;
// Let the first glucose readings come in before the clock asks Nightscout for more.
const unsigned long FIRST_SYNC_DELAY_MS = 40UL * 1000UL;
const unsigned long SCROLL_STEP_MS = 45;
const unsigned long CONFIRMED_SHOW_MS = 2500;
// Pause between two reminders of one round, which is also how long a long press that began on the
// earlier one still counts for it.
const unsigned long LONG_PRESS_GRACE_MS = 3000;
// A reminder that has just ended or been dismissed can still be confirmed with a long press for this long.
const unsigned long CONFIRM_WINDOW_MS = 60000;
const char* SUPPLY_BEEP = "supply:d=16,o=6,b=200:e,p,e";
const char* HINT = " - hold middle = done";
const char* HINT_FRIENDLY = " - appui long sur le bouton du milieu = fait";

inline int indexOf(SupplyItem item) { return static_cast<int>(item); }
inline uint8_t bitOf(SupplyItem item) { return 1 << indexOf(item); }
}  // namespace

// The getter for the instantiated singleton instance
SupplyReminderManager_& SupplyReminderManager_::getInstance() {
    static SupplyReminderManager_ instance;
    return instance;
}

// Initialize the global shared instance
SupplyReminderManager_& supplyReminder = supplyReminder.getInstance();

void SupplyReminderManager_::setup() { load(); }

const char* SupplyReminderManager_::nameOf(SupplyItem item) {
    return item == SupplyItem::CATHETER ? "CATHETER" : "SENSOR";
}

bool SupplyReminderManager_::enabled(SupplyItem item) const {
    return item == SupplyItem::CATHETER ? SettingsManager.settings.supply_catheter_enabled
                                        : SettingsManager.settings.supply_sensor_enabled;
}

int SupplyReminderManager_::lifetimeDays(SupplyItem item) const {
    return item == SupplyItem::CATHETER ? SettingsManager.settings.supply_catheter_days
                                        : SettingsManager.settings.supply_sensor_days;
}

uint32_t SupplyReminderManager_::startOf(SupplyItem item) const {
    return latestSupplyStart(localStart[indexOf(item)].load(), nightscoutStart[indexOf(item)].load());
}

SupplyStatus SupplyReminderManager_::statusOf(SupplyItem item, uint32_t now) const {
    return evaluateSupply(
        enabled(item), startOf(item), now, lifetimeDays(item), SettingsManager.settings.supply_warn_hours);
}

// ---------- Saved dates ----------
// They live in their own file: saving the settings page rewrites the configuration, and a date typed
// on the clock must survive that.

void SupplyReminderManager_::load() {
    if (!LittleFS.exists(SUPPLIES_FILE)) {
        return;
    }
    File file = LittleFS.open(SUPPLIES_FILE, "r");
    if (!file) {
        return;
    }
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error) {
        DEBUG_PRINTF("Supplies: cannot read %s: %s\n", SUPPLIES_FILE, error.c_str());
        return;
    }
    localStart[indexOf(SupplyItem::CATHETER)] = doc["catheter"].as<uint32_t>();
    localStart[indexOf(SupplyItem::SENSOR)] = doc["sensor"].as<uint32_t>();
    nightscoutStart[indexOf(SupplyItem::CATHETER)] = doc["ns_catheter"].as<uint32_t>();
    nightscoutStart[indexOf(SupplyItem::SENSOR)] = doc["ns_sensor"].as<uint32_t>();
}

void SupplyReminderManager_::persist() {
    JsonDocument doc;
    doc["catheter"] = localStart[indexOf(SupplyItem::CATHETER)].load();
    doc["sensor"] = localStart[indexOf(SupplyItem::SENSOR)].load();
    doc["ns_catheter"] = nightscoutStart[indexOf(SupplyItem::CATHETER)].load();
    doc["ns_sensor"] = nightscoutStart[indexOf(SupplyItem::SENSOR)].load();
    File file = LittleFS.open(SUPPLIES_FILE, FILE_WRITE);
    if (!file) {
        DEBUG_PRINTLN("Supplies: cannot open the file for writing");
        return;
    }
    serializeJson(doc, file);
    file.close();
}

// ---------- Changes from the web page and the buttons ----------

bool SupplyReminderManager_::requestChange(SupplyItem item, uint32_t epoch) {
    const uint32_t now = ServerManager.getUtcEpoch();
    if (now < SUPPLY_MIN_VALID_EPOCH) {
        return false;
    }
    if (epoch == 0) {
        epoch = now;
    }
    // Not in the future (a few minutes of drift between the phone and the clock are fine), and not older
    // than the longest lifetime the settings allow.
    if (epoch > now + 300 || epoch + 40UL * 86400UL < now) {
        return false;
    }
    localStart[indexOf(item)] = epoch;
    persistRequested = true;
    return true;
}

void SupplyReminderManager_::confirmChanged(SupplyItem item, uint32_t now) {
    localStart[indexOf(item)] = now;
    persistRequested = true;
    roundQueue &= ~bitOf(item);
    shownMask &= ~bitOf(item);
    overlayEndedUnconfirmed = false;
    DEBUG_PRINTF("Supplies: %s confirmed as changed\n", nameOf(item));
    startConfirmedOverlay(item);
}

bool SupplyReminderManager_::handleSelectClick() {
    if (!overlayActive) {
        return false;
    }
    if (overlayMode == OverlayMode::SCROLLING) {
        // Dismissed: nothing more this round, the next one comes after the repeat interval.
        roundQueue = 0;
        endOverlay();
    }
    return true;
}

bool SupplyReminderManager_::handleSelectLong() {
    const uint32_t now = ServerManager.getUtcEpoch();
    if (now < SUPPLY_MIN_VALID_EPOCH) {
        return false;
    }
    if (overlayActive) {
        if (overlayMode == OverlayMode::CONFIRMED) {
            return true;
        }
        confirmChanged(overlayItem, now);
        return true;
    }
    if (overlayEndedUnconfirmed && millis() - overlayEndedMillis <= CONFIRM_WINDOW_MS) {
        confirmChanged(overlayEndedItem, now);
        return true;
    }
    return false;
}

// ---------- Nightscout ----------

bool SupplyReminderManager_::shouldSync() const {
    const Settings& settings = SettingsManager.settings;
    if (settings.bg_source != BG_SOURCE::NIGHTSCOUT || settings.nightscout_url.length() == 0) {
        return false;
    }
    if (!enabled(SupplyItem::CATHETER) && !enabled(SupplyItem::SENSOR)) {
        return false;
    }
    if (!firstSyncDone) {
        return millis() >= FIRST_SYNC_DELAY_MS;
    }
    return millis() - lastSyncMillis >= SYNC_INTERVAL_MS;
}

// Asks Nightscout for the newest treatment of one kind. Returns false when the request failed; an
// empty list is a success that leaves `epochOut` at zero.
bool SupplyReminderManager_::fetchLatestTreatment(const char* eventType, uint32_t& epochOut) {
    epochOut = 0;
    String base = SettingsManager.settings.nightscout_url;
    if (!base.endsWith("/")) {
        base += "/";
    }
    String type = eventType;
    type.replace(" ", "%20");
    const String url = base + "api/v1/treatments?find[eventType]=" + type + "&count=1";

    std::unique_ptr<WiFiClientSecure> secure;
    HTTPClient http;
    bool began;
    if (url.startsWith("https")) {
        secure.reset(new WiFiClientSecure());
        secure->setInsecure();
        began = http.begin(*secure, url);
    } else {
        began = http.begin(url);
    }
    if (!began) {
        return false;
    }
    http.setTimeout(8000);
    http.setConnectTimeout(5000);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http.addHeader("Accept", "application/json");
    const String apiKey = SettingsManager.settings.nightscout_api_key;
    if (apiKey != "") {
        http.addHeader("api-secret", sha1(apiKey));
    }

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        DEBUG_PRINTF("Supplies: Nightscout answered %d for %s\n", code, eventType);
        http.end();
        return false;
    }
    const String body = http.getString();
    http.end();

    JsonDocument filter;
    filter[0]["created_at"] = true;
    filter[0]["mills"] = true;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body, DeserializationOption::Filter(filter));
    if (error || !doc.is<JsonArray>()) {
        DEBUG_PRINTF("Supplies: unexpected Nightscout answer for %s\n", eventType);
        return false;
    }
    JsonArray treatments = doc.as<JsonArray>();
    if (treatments.size() == 0) {
        return true;
    }
    JsonVariant newest = treatments[0];
    if (newest["created_at"].is<const char*>()) {
        epochOut = parseNightscoutTimestamp(newest["created_at"].as<const char*>());
    }
    if (epochOut == 0 && !newest["mills"].isNull()) {
        epochOut = static_cast<uint32_t>(newest["mills"].as<unsigned long long>() / 1000ULL);
    }
    return true;
}

void SupplyReminderManager_::syncFromNightscout() {
    syncResult = 3;
    uint32_t siteChange = 0, sensorStart = 0, sensorChange = 0;
    const bool siteOk = fetchLatestTreatment("Site Change", siteChange);
    const bool startOk = fetchLatestTreatment("Sensor Start", sensorStart);
    const bool changeOk = fetchLatestTreatment("Sensor Change", sensorChange);

    bool changed = false;
    if (siteOk && nightscoutStart[indexOf(SupplyItem::CATHETER)] != siteChange) {
        nightscoutStart[indexOf(SupplyItem::CATHETER)] = siteChange;
        changed = true;
    }
    if (startOk || changeOk) {
        const uint32_t sensor = latestSupplyStart(startOk ? sensorStart : 0, changeOk ? sensorChange : 0);
        if (nightscoutStart[indexOf(SupplyItem::SENSOR)] != sensor) {
            nightscoutStart[indexOf(SupplyItem::SENSOR)] = sensor;
            changed = true;
        }
    }
    if (changed) {
        persistRequested = true;
    }

    const bool allOk = siteOk && startOk && changeOk;
    syncResult = allOk ? 1 : 2;
    firstSyncDone = true;
    // A failed round is tried again sooner than a good one.
    lastSyncMillis = allOk ? millis() : millis() - SYNC_INTERVAL_MS + SYNC_RETRY_MS;
    if (allOk) {
        lastSyncEpoch = ServerManager.getUtcEpoch();
    }
    DEBUG_PRINTF(
        "Supplies: Nightscout sync %s, catheter %u, sensor %u\n", allOk ? "ok" : "incomplete",
        static_cast<unsigned>(nightscoutStart[indexOf(SupplyItem::CATHETER)].load()),
        static_cast<unsigned>(nightscoutStart[indexOf(SupplyItem::SENSOR)].load()));
}

// ---------- Reminder on the matrix ----------

bool SupplyReminderManager_::canShowOverlay() const {
    return !bgAlarmManager.isAlarmActive() && !DisplayManager.isBrightnessOverlayActive() && !MATRIX_OFF;
}

void SupplyReminderManager_::startOverlay(SupplyItem item, uint32_t now) {
    const SupplyStatus status = statusOf(item, now);
    if (SettingsManager.settings.supply_friendly) {
        formatFriendlySupplyMessage(overlayText, sizeof(overlayText), item == SupplyItem::SENSOR, status);
        strncat(overlayText, HINT_FRIENDLY, sizeof(overlayText) - strlen(overlayText) - 1);
    } else {
        formatSupplyMessage(overlayText, sizeof(overlayText), nameOf(item), status);
        strncat(overlayText, HINT, sizeof(overlayText) - strlen(overlayText) - 1);
    }
    overlayColor = status.state == SupplyState::OVERDUE ? COLOR_RED : COLOR_YELLOW;
    // A big-text face may have left the large font active.
    DisplayManager.setFont(FONT_TYPE::MEDIUM);
    overlayTextWidth = static_cast<int>(DisplayManager.getTextWidth(overlayText, 0));
    overlayItem = item;
    overlayMode = OverlayMode::SCROLLING;
    overlayStartMillis = millis();
    overlayLastDrawMillis = 0;
    overlayActive = true;
    DEBUG_PRINTF("Supplies: showing \"%s\"\n", overlayText);
    if (SettingsManager.settings.supply_beep) {
        PeripheryManager.playRTTTLString(SUPPLY_BEEP);
    }
    drawOverlay();
}

void SupplyReminderManager_::startConfirmedOverlay(SupplyItem item) {
    strncpy(overlayText, item == SupplyItem::CATHETER ? "CATH OK" : "SENS OK", sizeof(overlayText) - 1);
    overlayText[sizeof(overlayText) - 1] = '\0';
    overlayColor = COLOR_GREEN;
    DisplayManager.setFont(FONT_TYPE::MEDIUM);
    overlayItem = item;
    overlayMode = OverlayMode::CONFIRMED;
    overlayStartMillis = millis();
    overlayLastDrawMillis = 0;
    overlayActive = true;
    drawOverlay();
}

// Starts the next supply of the round that is still due. Returns whether one was shown.
bool SupplyReminderManager_::startNextQueued(uint32_t now) {
    for (int i = 0; i < 2; i++) {
        const SupplyItem item = static_cast<SupplyItem>(i);
        if (!(roundQueue & bitOf(item))) {
            continue;
        }
        roundQueue &= ~bitOf(item);
        const SupplyState state = statusOf(item, now).state;
        if (state == SupplyState::DUE_SOON || state == SupplyState::OVERDUE) {
            startOverlay(item, now);
            return true;
        }
    }
    return false;
}

void SupplyReminderManager_::endOverlay() {
    overlayEndedUnconfirmed = overlayMode == OverlayMode::SCROLLING;
    overlayEndedItem = overlayItem;
    overlayEndedMillis = millis();
    overlayActive = false;
    nextQueuedAllowedMillis = millis() + LONG_PRESS_GRACE_MS;
}

void SupplyReminderManager_::drawOverlay() {
    overlayLastDrawMillis = millis();
    DisplayManager.clearMatrix();
    DisplayManager.setTextColor(overlayColor);
    if (overlayMode == OverlayMode::SCROLLING) {
        const int x = MATRIX_WIDTH - static_cast<int>((millis() - overlayStartMillis) / SCROLL_STEP_MS);
        DisplayManager.printText(x, 6, overlayText, TEXT_ALIGNMENT::LEFT, 0);
    } else {
        DisplayManager.printText(0, 6, overlayText, TEXT_ALIGNMENT::CENTER, 0);
    }
    DisplayManager.update();
}

void SupplyReminderManager_::updateOverlay(uint32_t now) {
    const unsigned long ms = millis();

    if (overlayActive) {
        // A glucose alarm, the display going off or the brightness overlay all take over the matrix.
        if (!canShowOverlay()) {
            roundQueue = 0;
            endOverlay();
            overlayEndedUnconfirmed = false;
            return;
        }
        if (overlayMode == OverlayMode::CONFIRMED) {
            if (ms - overlayStartMillis >= CONFIRMED_SHOW_MS) {
                endOverlay();
                overlayEndedUnconfirmed = false;
            } else if (ms - overlayLastDrawMillis >= SCROLL_STEP_MS) {
                drawOverlay();
            }
            return;
        }
        const SupplyState state = statusOf(overlayItem, now).state;
        const bool stillDue = state == SupplyState::DUE_SOON || state == SupplyState::OVERDUE;
        const unsigned long steps = (ms - overlayStartMillis) / SCROLL_STEP_MS;
        if (!stillDue || steps > static_cast<unsigned long>(MATRIX_WIDTH + overlayTextWidth)) {
            endOverlay();
            // Changed elsewhere, such as on the web page: nothing to confirm any more.
            if (!stillDue) {
                overlayEndedUnconfirmed = false;
            }
        } else if (ms - overlayLastDrawMillis >= SCROLL_STEP_MS) {
            drawOverlay();
        }
        return;
    }

    // Which supplies are due now, and which of them still have to be shown.
    uint8_t dueMask = 0;
    for (int i = 0; i < 2; i++) {
        const SupplyItem item = static_cast<SupplyItem>(i);
        const SupplyState state = statusOf(item, now).state;
        if (state == SupplyState::DUE_SOON || state == SupplyState::OVERDUE) {
            dueMask |= bitOf(item);
        }
    }
    shownMask &= dueMask;
    roundQueue &= dueMask;

    if (roundQueue != 0) {
        if (ms >= nextQueuedAllowedMillis && canShowOverlay()) {
            startNextQueued(now);
        }
        return;
    }
    if (dueMask == 0) {
        return;
    }
    const bool allShown = (dueMask & ~shownMask) == 0;
    if (supplyRoundDue(
            true, allShown, ms, lastRoundMillis, SettingsManager.settings.supply_repeat_minutes) &&
        ms >= nextQueuedAllowedMillis && canShowOverlay()) {
        lastRoundMillis = ms;
        shownMask |= dueMask;
        roundQueue = dueMask;
        startNextQueued(now);
    }
}

// ---------- Main loop ----------

void SupplyReminderManager_::tick() {
    if (persistRequested.exchange(false)) {
        persist();
    }

    const uint32_t now = ServerManager.getUtcEpoch();
    if (now < SUPPLY_MIN_VALID_EPOCH) {
        return;
    }

    if (!overlayActive) {
        const bool manual = syncRequested.exchange(false);
        if (manual && SettingsManager.settings.bg_source != BG_SOURCE::NIGHTSCOUT) {
            syncResult = 4;
        } else if (manual || shouldSync()) {
            syncFromNightscout();
        }
    }

    updateOverlay(now);
}

// ---------- Web page ----------

String SupplyReminderManager_::statusJson() {
    const uint32_t now = ServerManager.getUtcEpoch();
    const Settings& settings = SettingsManager.settings;
    static const char* const SYNC_NAMES[] = {"never", "ok", "failed", "busy", "off"};

    JsonDocument doc;
    doc["now"] = now;
    doc["nightscout"] =
        settings.bg_source == BG_SOURCE::NIGHTSCOUT && settings.nightscout_url.length() > 0;
    doc["sync"] = SYNC_NAMES[syncResult.load() <= 4 ? syncResult.load() : 0];
    doc["last_sync"] = lastSyncEpoch.load();
    for (int i = 0; i < 2; i++) {
        const SupplyItem item = static_cast<SupplyItem>(i);
        const SupplyStatus status = statusOf(item, now);
        const uint32_t start = startOf(item);
        JsonObject object = doc[i == 0 ? "catheter" : "sensor"].to<JsonObject>();
        object["enabled"] = enabled(item);
        object["lifetime_days"] = lifetimeDays(item);
        object["start"] = start;
        object["local"] = localStart[i].load();
        object["nightscout"] = nightscoutStart[i].load();
        object["due"] = start == 0 ? 0 : start + static_cast<uint32_t>(lifetimeDays(item)) * 86400UL;
        object["remaining"] = status.remainingSeconds;
        const char* state = "off";
        if (enabled(item)) {
            switch (status.state) {
                case SupplyState::OK:
                    state = "ok";
                    break;
                case SupplyState::DUE_SOON:
                    state = "soon";
                    break;
                case SupplyState::OVERDUE:
                    state = "overdue";
                    break;
                default:
                    state = "unknown";
                    break;
            }
        }
        object["state"] = state;
    }
    String body;
    serializeJson(doc, body);
    return body;
}
