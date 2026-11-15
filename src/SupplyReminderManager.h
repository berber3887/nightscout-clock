#ifndef SupplyReminderManager_h
#define SupplyReminderManager_h

#include <Arduino.h>

#include <atomic>

#include "SupplyReminder.h"

enum class SupplyItem : uint8_t { CATHETER = 0, SENSOR = 1 };

// Reminds the wearer to replace the catheter and the sensor.
//
// The start of each supply comes from two places, and the later one counts: Nightscout's treatments
// ("Site Change", "Sensor Start", "Sensor Change") and the confirmations made on the clock's buttons or
// on the web page. The reminder scrolls across the matrix from `supply_warn_hours` before the end of the
// lifetime, and repeats until the change is confirmed.
class SupplyReminderManager_ {
public:
    static SupplyReminderManager_& getInstance();
    void setup();
    // Main loop only.
    void tick();

    // True while a reminder owns the matrix; the glucose faces must not draw then.
    bool isOverlayActive() const { return overlayActive; }
    // Middle button, main loop only. They return true when the press was used here.
    // A click dismisses the reminder on the screen; a long press confirms that supply as changed.
    bool handleSelectClick();
    bool handleSelectLong();

    // Safe to call from the web server task. `epoch` 0 means now. Returns false when the clock does not
    // know the time yet or the time asked for is not sensible.
    bool requestChange(SupplyItem item, uint32_t epoch);
    void requestSync() { syncRequested = true; }
    // JSON for the web page, safe to call from the web server task.
    String statusJson();

private:
    SupplyReminderManager_() {
        for (int i = 0; i < 2; i++) {
            localStart[i] = 0;
            nightscoutStart[i] = 0;
        }
    }

    enum class OverlayMode : uint8_t { SCROLLING, CONFIRMED };

    // Epochs are 32 bits so that the web server task reads them in one piece.
    std::atomic<uint32_t> localStart[2];
    std::atomic<uint32_t> nightscoutStart[2];
    std::atomic<bool> persistRequested{false};
    std::atomic<bool> syncRequested{false};
    // 0 never tried, 1 ok, 2 failed, 3 syncing, 4 the data source is not Nightscout.
    std::atomic<uint8_t> syncResult{0};
    std::atomic<uint32_t> lastSyncEpoch{0};

    bool firstSyncDone = false;
    unsigned long lastSyncMillis = 0;

    bool overlayActive = false;
    OverlayMode overlayMode = OverlayMode::SCROLLING;
    SupplyItem overlayItem = SupplyItem::CATHETER;
    char overlayText[64] = {0};
    uint16_t overlayColor = 0;
    unsigned long overlayStartMillis = 0;
    unsigned long overlayLastDrawMillis = 0;
    int overlayTextWidth = 0;
    // Supplies still to be shown in the round that is under way, one bit per SupplyItem.
    uint8_t roundQueue = 0;
    // Supplies that were shown since they became due, so a newly due one is shown at once while the
    // others wait for the repeat interval.
    uint8_t shownMask = 0;
    unsigned long lastRoundMillis = 0;
    unsigned long nextQueuedAllowedMillis = 0;
    // A long press is only noticed when the button is released, so a press that began on a reminder
    // still counts for a moment after the reminder has gone.
    unsigned long overlayEndedMillis = 0;
    bool overlayEndedUnconfirmed = false;
    SupplyItem overlayEndedItem = SupplyItem::CATHETER;

    bool enabled(SupplyItem item) const;
    int lifetimeDays(SupplyItem item) const;
    uint32_t startOf(SupplyItem item) const;
    SupplyStatus statusOf(SupplyItem item, uint32_t now) const;
    static const char* nameOf(SupplyItem item);

    void load();
    void persist();
    bool shouldSync() const;
    void syncFromNightscout();
    bool fetchLatestTreatment(const char* eventType, uint32_t& epochOut);

    bool canShowOverlay() const;
    void startOverlay(SupplyItem item, uint32_t now);
    void startConfirmedOverlay(SupplyItem item);
    bool startNextQueued(uint32_t now);
    void confirmChanged(SupplyItem item, uint32_t now);
    void endOverlay();
    void drawOverlay();
    void updateOverlay(uint32_t now);
};

extern SupplyReminderManager_& supplyReminder;

#endif
