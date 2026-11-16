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
const char* ENTERED_BY = "nightscout-clock";
const char* HINT_FRIENDLY = " - appui long au milieu = fait";

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
    ignoreUntil[0] = doc["ign_catheter"].as<uint32_t>();
    ignoreUntil[1] = doc["ign_sensor"].as<uint32_t>();
    for (int i = 0; i < 2; i++) {
        const char* key = i == 0 ? "catheter" : "sensor";
        undoLocal[i] = doc["undo"][key]["local"].as<uint32_t>();
        undoNightscout[i] = doc["undo"][key]["ns"].as<uint32_t>();
        undoEpoch[i] = doc["undo"][key]["epoch"].as<uint32_t>();
        postedId[i] = doc["posted"][key]["id"].as<String>();
        postedEpoch[i] = doc["posted"][key]["epoch"].as<uint32_t>();
    }
    for (int k = 0; k < HISTORY_SIZE; k++) {
        history[0][k] = doc["h_catheter"][k].as<uint32_t>();
        history[1][k] = doc["h_sensor"][k].as<uint32_t>();
    }
}

void SupplyReminderManager_::persist() {
    JsonDocument doc;
    doc["catheter"] = localStart[indexOf(SupplyItem::CATHETER)].load();
    doc["sensor"] = localStart[indexOf(SupplyItem::SENSOR)].load();
    doc["ns_catheter"] = nightscoutStart[indexOf(SupplyItem::CATHETER)].load();
    doc["ns_sensor"] = nightscoutStart[indexOf(SupplyItem::SENSOR)].load();
    doc["ign_catheter"] = ignoreUntil[0].load();
    doc["ign_sensor"] = ignoreUntil[1].load();
    for (int i = 0; i < 2; i++) {
        const char* key = i == 0 ? "catheter" : "sensor";
        doc["undo"][key]["local"] = undoLocal[i].load();
        doc["undo"][key]["ns"] = undoNightscout[i].load();
        doc["undo"][key]["epoch"] = undoEpoch[i].load();
        doc["posted"][key]["id"] = postedId[i];
        doc["posted"][key]["epoch"] = postedEpoch[i];
    }
    JsonArray historyCatheter = doc["h_catheter"].to<JsonArray>();
    JsonArray historySensor = doc["h_sensor"].to<JsonArray>();
    for (int k = 0; k < HISTORY_SIZE; k++) {
        historyCatheter.add(history[0][k].load());
        historySensor.add(history[1][k].load());
    }
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
    const bool explicitTime = epoch != 0;
    if (epoch == 0) {
        epoch = now;
    }
    // Not in the future (a few minutes of drift between the phone and the clock are fine), and not older
    // than the longest lifetime the settings allow.
    if (epoch > now + 300 || epoch + 40UL * 86400UL < now) {
        return false;
    }
    rememberUndo(item, epoch);
    localStart[indexOf(item)] = epoch;
    // A date typed by hand that is older than what Nightscout knows is a correction (for example after
    // test confirmations): Nightscout's later notes up to now are set aside, and the date is sent.
    if (explicitTime && epoch < nightscoutStart[indexOf(item)].load()) {
        ignoreUntil[indexOf(item)] = now;
        nightscoutStart[indexOf(item)] = 0;
        syncRequested = true;
    }
    persistRequested = true;
    return true;
}

void SupplyReminderManager_::confirmChanged(SupplyItem item, uint32_t now) {
    rememberUndo(item, now);
    localStart[indexOf(item)] = now;
    persistRequested = true;
    roundQueue &= ~bitOf(item);
    shownMask &= ~bitOf(item);
    overlayEndedUnconfirmed = false;
    DEBUG_PRINTF("Supplies: %s confirmed as changed\n", nameOf(item));
    startConfirmedOverlay(item);
}

void SupplyReminderManager_::rememberUndo(SupplyItem item, uint32_t newEpoch) {
    const int i = indexOf(item);
    undoLocal[i] = localStart[i].load();
    undoNightscout[i] = nightscoutStart[i].load();
    undoEpoch[i] = newEpoch;
}

bool SupplyReminderManager_::requestUndo(SupplyItem item) {
    const int i = indexOf(item);
    if (undoEpoch[i].load() == 0 || localStart[i].load() != undoEpoch[i].load()) {
        return false;
    }
    undoRequested[i] = true;
    return true;
}

// Main loop: puts the dates back as they were before the last confirmation, and removes the note the
// clock sent to Nightscout for it.
void SupplyReminderManager_::applyUndo(SupplyItem item) {
    const int i = indexOf(item);
    const uint32_t undone = undoEpoch[i].load();
    if (undone == 0 || localStart[i].load() != undone) {
        return;
    }
    localStart[i] = undoLocal[i].load();
    nightscoutStart[i] = undoNightscout[i].load();
    ignoreUntil[i] = 0;
    undoEpoch[i] = 0;
    shownMask &= ~bitOf(item);
    if (postedEpoch[i] == undone && postedId[i].length() > 0) {
        if (deleteTreatment(postedId[i])) {
            postedId[i] = "";
            postedEpoch[i] = 0;
        } else {
            // Could not remove it now: set it aside so that it does not come back, and remove it later.
            ignoreUntil[i] = ServerManager.getUtcEpoch();
        }
        syncRequested = true;
    }
    persistRequested = true;
    DEBUG_PRINTF("Supplies: %s confirmation taken back\n", nameOf(item));
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

// One request to Nightscout with the clock's secret. `reply` receives the answer when wanted.
bool SupplyReminderManager_::nightscoutRequest(
    const char* method, const String& url, const String& body, int& code, String* reply) {
    code = 0;
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
    if (body.length() > 0) {
        http.addHeader("Content-Type", "application/json");
    }
    const String apiKey = SettingsManager.settings.nightscout_api_key;
    if (apiKey != "") {
        http.addHeader("api-secret", sha1(apiKey));
    }
    code = http.sendRequest(method, body);
    if (reply != nullptr && code > 0) {
        *reply = http.getString();
    }
    http.end();
    return code > 0;
}

static String nightscoutBase() {
    String base = SettingsManager.settings.nightscout_url;
    if (!base.endsWith("/")) {
        base += "/";
    }
    return base;
}

// Asks Nightscout for the newest treatments of one kind, newest first. Returns false when the request
// failed; an empty list is a success with a count of zero.
bool SupplyReminderManager_::fetchTreatments(const char* eventType, TreatmentRef* out, int max, int& count) {
    count = 0;
    String type = eventType;
    type.replace(" ", "%20");
    const String url = nightscoutBase() + "api/v1/treatments?find[eventType]=" + type + "&count=" + String(max);

    int code = 0;
    String body;
    if (!nightscoutRequest("GET", url, "", code, &body) || code != HTTP_CODE_OK) {
        DEBUG_PRINTF("Supplies: Nightscout answered %d for %s\n", code, eventType);
        return false;
    }

    JsonDocument filter;
    filter[0]["created_at"] = true;
    filter[0]["mills"] = true;
    filter[0]["_id"] = true;
    filter[0]["enteredBy"] = true;
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, body, DeserializationOption::Filter(filter));
    if (error || !doc.is<JsonArray>()) {
        DEBUG_PRINTF("Supplies: unexpected Nightscout answer for %s\n", eventType);
        return false;
    }
    for (JsonVariant treatment : doc.as<JsonArray>()) {
        if (count >= max) {
            break;
        }
        uint32_t epoch = 0;
        if (treatment["created_at"].is<const char*>()) {
            epoch = parseNightscoutTimestamp(treatment["created_at"].as<const char*>());
        }
        if (epoch == 0 && !treatment["mills"].isNull()) {
            epoch = static_cast<uint32_t>(treatment["mills"].as<unsigned long long>() / 1000ULL);
        }
        if (epoch != 0) {
            out[count].epoch = epoch;
            out[count].id = treatment["_id"].as<String>();
            out[count].fromClock = treatment["enteredBy"].as<String>() == ENTERED_BY;
            count++;
        }
    }
    return true;
}

// Logs a confirmed change in Nightscout. Returns true when Nightscout accepted it; `idOut` then holds
// the id Nightscout gave the note, when it reported one.
bool SupplyReminderManager_::postTreatment(SupplyItem item, uint32_t epoch, String& idOut) {
    idOut = "";
    char stamp[32];
    formatNightscoutTimestamp(stamp, sizeof(stamp), epoch);
    JsonDocument doc;
    doc["eventType"] = item == SupplyItem::CATHETER ? "Site Change" : "Sensor Change";
    doc["created_at"] = stamp;
    doc["enteredBy"] = ENTERED_BY;
    doc["notes"] = "Confirmed on the Nightscout clock";
    String body;
    serializeJson(doc, body);

    int code = 0;
    String reply;
    const bool sent = nightscoutRequest("POST", nightscoutBase() + "api/v1/treatments", body, code, &reply);
    DEBUG_PRINTF("Supplies: Nightscout answered %d to the %s change\n", code, nameOf(item));
    if (!sent || (code != HTTP_CODE_OK && code != HTTP_CODE_CREATED)) {
        return false;
    }
    JsonDocument filter;
    filter[0]["_id"] = true;
    JsonDocument answer;
    if (!deserializeJson(answer, reply, DeserializationOption::Filter(filter)) && answer.is<JsonArray>() &&
        answer.size() > 0) {
        idOut = answer[0]["_id"].as<String>();
    }
    return true;
}

// Removes a note the clock sent. Returns true when Nightscout accepted the removal.
bool SupplyReminderManager_::deleteTreatment(const String& id) {
    if (id.length() == 0) {
        return false;
    }
    int code = 0;
    const bool sent = nightscoutRequest("DELETE", nightscoutBase() + "api/v1/treatments/" + id, "", code, nullptr);
    DEBUG_PRINTF("Supplies: Nightscout answered %d to removing note %s\n", code, id.c_str());
    return sent && (code == HTTP_CODE_OK || code == HTTP_CODE_NO_CONTENT);
}

// A change confirmed here is newer than anything Nightscout knows: send it. Also catches up after a
// restart or a spell without network, because it compares the saved dates rather than remembering a
// request.
void SupplyReminderManager_::writePendingChanges(uint32_t now) {
    const Settings& settings = SettingsManager.settings;
    if (!settings.supply_nightscout_write) {
        writeResult = 3;
        return;
    }
    if (!firstSyncDone || settings.bg_source != BG_SOURCE::NIGHTSCOUT || settings.nightscout_url.length() == 0) {
        return;
    }
    for (int i = 0; i < 2; i++) {
        const SupplyItem item = static_cast<SupplyItem>(i);
        const uint32_t local = localStart[i].load();
        if (!enabled(item) || local == 0 || local <= nightscoutStart[i].load() || local > now + 300) {
            continue;
        }
        if (millis() < nextWriteMillis[i]) {
            continue;
        }
        String id;
        if (postTreatment(item, local, id)) {
            writeFailures[i] = 0;
            writeResult = 1;
            nightscoutStart[i] = local;
            postedId[i] = id;
            postedEpoch[i] = local;
            persistRequested = true;
            syncRequested = true;  // read the list back so the history shows it
        } else {
            writeResult = 2;
            writeFailures[i]++;
            nextWriteMillis[i] = millis() + (writeFailures[i] < 4 ? 60000UL : 3600000UL);
        }
    }
}

void SupplyReminderManager_::syncFromNightscout() {
    syncResult = 3;
    TreatmentRef siteRefs[HISTORY_SIZE], startRefs[HISTORY_SIZE], changeRefs[HISTORY_SIZE];
    int siteCount = 0, startCount = 0, changeCount = 0;
    const bool siteOk = fetchTreatments("Site Change", siteRefs, HISTORY_SIZE, siteCount);
    const bool startOk = fetchTreatments("Sensor Start", startRefs, HISTORY_SIZE, startCount);
    const bool changeOk = fetchTreatments("Sensor Change", changeRefs, HISTORY_SIZE, changeCount);

    // The notes the clock itself sent by mistake, set aside by a correction typed by hand, are removed
    // from Nightscout so that they do not come back; notes entered elsewhere are left alone.
    auto removeOwnIgnored = [this](TreatmentRef* refs, int count, int supply) {
        const uint32_t local = localStart[supply].load(), until = ignoreUntil[supply].load();
        for (int k = 0; k < count; k++) {
            if (until != 0 && refs[k].fromClock && refs[k].epoch > local && refs[k].epoch <= until) {
                deleteTreatment(refs[k].id);
            }
        }
    };
    removeOwnIgnored(siteRefs, siteCount, 0);
    removeOwnIgnored(startRefs, startCount, 1);
    removeOwnIgnored(changeRefs, changeCount, 1);

    uint32_t sites[HISTORY_SIZE] = {0}, starts[HISTORY_SIZE] = {0}, changes[HISTORY_SIZE] = {0};
    for (int k = 0; k < HISTORY_SIZE; k++) {
        sites[k] = k < siteCount ? siteRefs[k].epoch : 0;
        starts[k] = k < startCount ? startRefs[k].epoch : 0;
        changes[k] = k < changeCount ? changeRefs[k].epoch : 0;
    }
    siteCount = dropIgnoredSupplyChanges(sites, siteCount, localStart[0].load(), ignoreUntil[0].load());
    startCount = dropIgnoredSupplyChanges(starts, startCount, localStart[1].load(), ignoreUntil[1].load());
    changeCount = dropIgnoredSupplyChanges(changes, changeCount, localStart[1].load(), ignoreUntil[1].load());
    const uint32_t siteChange = siteCount > 0 ? sites[0] : 0;
    const uint32_t sensorStart = startCount > 0 ? starts[0] : 0;
    const uint32_t sensorChange = changeCount > 0 ? changes[0] : 0;

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
    if (siteOk) {
        for (int k = 0; k < HISTORY_SIZE; k++) {
            history[0][k] = sites[k];
        }
    }
    if (startOk && changeOk) {
        uint32_t merged[HISTORY_SIZE];
        mergeSupplyHistory(starts, startCount, changes, changeCount, merged, HISTORY_SIZE);
        for (int k = 0; k < HISTORY_SIZE; k++) {
            history[1][k] = merged[k];
        }
    }
    if (changed || siteOk || (startOk && changeOk)) {
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
        // A different sentence each time: pick at random, never the same twice in a row.
        const bool overdue = status.state == SupplyState::OVERDUE;
        const unsigned count = friendlyVariantCount(overdue);
        unsigned variant = random(count);
        if (count > 1 && variant == lastFriendlyVariant[overdue ? 1 : 0]) {
            variant = (variant + 1) % count;
        }
        lastFriendlyVariant[overdue ? 1 : 0] = variant;
        formatFriendlySupplyMessage(
            overlayText, sizeof(overlayText), item == SupplyItem::SENSOR, status, variant);
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
        for (int i = 0; i < 2; i++) {
            if (undoRequested[i].exchange(false)) {
                applyUndo(static_cast<SupplyItem>(i));
            }
        }
        const bool manual = syncRequested.exchange(false);
        if (manual && SettingsManager.settings.bg_source != BG_SOURCE::NIGHTSCOUT) {
            syncResult = 4;
        } else if (manual || shouldSync()) {
            syncFromNightscout();
        }
        writePendingChanges(now);
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
    static const char* const WRITE_NAMES[] = {"none", "ok", "failed", "off"};
    doc["write"] = WRITE_NAMES[writeResult.load() <= 3 ? writeResult.load() : 0];
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
        object["undo"] = undoEpoch[i].load() != 0 && localStart[i].load() == undoEpoch[i].load();
        JsonArray changes = object["history"].to<JsonArray>();
        for (int k = 0; k < HISTORY_SIZE; k++) {
            const uint32_t epoch = history[i][k].load();
            if (epoch != 0) {
                changes.add(epoch);
            }
        }
    }
    String body;
    serializeJson(doc, body);
    return body;
}
