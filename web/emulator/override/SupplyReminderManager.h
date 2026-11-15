// Replaces the firmware's SupplyReminderManager.h: the clock preview has no catheter or sensor reminders,
// so the display and button code finds nothing to show or confirm.
#ifndef SupplyReminderManager_h
#define SupplyReminderManager_h

class SupplyReminderManager_ {
public:
    bool isOverlayActive() const { return false; }
    bool handleSelectClick() { return false; }
    bool handleSelectLong() { return false; }
};

static SupplyReminderManager_ supplyReminder;

#endif
