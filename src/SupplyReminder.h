#ifndef SupplyReminder_h
#define SupplyReminder_h

// This header stays free of Arduino dependencies so that test/ can compile it on the host.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Reminders to replace the infusion set (catheter) and the glucose sensor.
//
// A supply has a start (when it was put on) and a lifetime in days. The reminder begins `warnHours`
// before the end of the lifetime and goes on until the supply is confirmed as changed.

enum class SupplyState { UNKNOWN, OK, DUE_SOON, OVERDUE };

struct SupplyStatus {
    SupplyState state;
    // Seconds until the end of the lifetime; negative once it has passed.
    long remainingSeconds;
};

// A clock that has not got the time yet reports an epoch near zero. Nothing can be judged then.
static const uint32_t SUPPLY_MIN_VALID_EPOCH = 1000000000UL;

inline SupplyStatus evaluateSupply(
    bool enabled, uint32_t startEpoch, uint32_t nowEpoch, int lifetimeDays, int warnHours) {
    if (!enabled || startEpoch == 0 || nowEpoch < SUPPLY_MIN_VALID_EPOCH) {
        return {SupplyState::UNKNOWN, 0};
    }
    const long long dueEpoch = static_cast<long long>(startEpoch) + static_cast<long long>(lifetimeDays) * 86400LL;
    const long long remaining = dueEpoch - static_cast<long long>(nowEpoch);
    if (remaining <= 0) {
        return {SupplyState::OVERDUE, static_cast<long>(remaining)};
    }
    if (remaining <= static_cast<long long>(warnHours) * 3600LL) {
        return {SupplyState::DUE_SOON, static_cast<long>(remaining)};
    }
    return {SupplyState::OK, static_cast<long>(remaining)};
}

// The supply goes by its newest start: the one typed on the clock or web page, or the one logged in
// Nightscout, whichever is later.
inline uint32_t latestSupplyStart(uint32_t localEpoch, uint32_t nightscoutEpoch) {
    return localEpoch > nightscoutEpoch ? localEpoch : nightscoutEpoch;
}

// Writes a short length of time such as "45min", "11h" or "3d". Always rounds down, except below one
// minute, which reads as "1min".
inline void formatSupplyDuration(char* out, size_t size, long seconds) {
    if (seconds < 0) {
        seconds = -seconds;
    }
    if (seconds < 3600) {
        long minutes = seconds / 60;
        if (minutes < 1) {
            minutes = 1;
        }
        snprintf(out, size, "%ldmin", minutes);
    } else if (seconds < 48L * 3600L) {
        snprintf(out, size, "%ldh", seconds / 3600L);
    } else {
        snprintf(out, size, "%ldd", seconds / 86400L);
    }
}

// The text scrolled across the clock, for example "Change CATHETER in 11h".
inline void formatSupplyMessage(char* out, size_t size, const char* name, SupplyStatus status) {
    char duration[16];
    formatSupplyDuration(duration, sizeof(duration), status.remainingSeconds);
    if (status.state == SupplyState::OVERDUE) {
        snprintf(out, size, "%s overdue %s", name, duration);
    } else {
        snprintf(out, size, "Change %s in %s", name, duration);
    }
}

// Gentle French sentences for a child. {quoi} is replaced by "le cathéter" or "le capteur" and {duree} by
// the time left. They are written in UTF-8 and converted to the font's Latin-1 before drawing.
static const char* const FRIENDLY_SOON[] = {
    "Coucou ! Bientôt, on change {quoi}, dans {duree}",
    "Stitch est tout excité : on change {quoi} dans {duree} !",
    "Comme une star de la K-pop, brille ! {quoi} se change dans {duree}",
    "Toto demande : c'est quand qu'on change {quoi} ? Dans {duree}, Toto !",
    "Bravo championne ! Dans {duree}, on change {quoi}",
    "Répétition de K-pop terminée ? Dans {duree}, on change {quoi} !",
    "Toto dit à la maîtresse : j'ai tout oublié, sauf {quoi} dans {duree} !",
    "Stitch dit : prépare-toi, {quoi} se change dans {duree} !",
    "Tu es courageuse ! Dans {duree}, on change {quoi}",
    "Tout est prêt ? Dans {duree}, mission : changer {quoi} !",
};
static const char* const FRIENDLY_OVERDUE[] = {
    "Coucou ! Il est temps de changer {quoi}",
    "Stitch dit : c'est l'heure de changer {quoi}, vite !",
    "Une star de la K-pop n'attend pas : on change {quoi} maintenant !",
    "Toto dit : maman, {quoi} est fatigué ! Oui Toto, on le change !",
    "Allez championne, un petit effort : on change {quoi} !",
    "Mission du jour : changer {quoi}. Tu es la meilleure !",
    "Toto a trouvé la solution : changer {quoi} tout de suite !",
    "Stitch sera fier de toi : change {quoi} maintenant !",
    "Hop hop hop ! C'est l'heure de changer {quoi}",
    "Ta chanson K-pop préférée peut attendre : change {quoi} d'abord !",
};

inline unsigned friendlyVariantCount(bool overdue) {
    return overdue ? sizeof(FRIENDLY_OVERDUE) / sizeof(FRIENDLY_OVERDUE[0])
                   : sizeof(FRIENDLY_SOON) / sizeof(FRIENDLY_SOON[0]);
}

// Turns the UTF-8 accented letters of a text into the font's Latin-1 capitals, in place, because the clock
// writes everything in capitals: "é" becomes the single byte 0xC9. Other characters are kept.
inline void utf8ToLatin1Capitals(char* text) {
    char* out = text;
    for (const unsigned char* in = reinterpret_cast<const unsigned char*>(text); *in; in++) {
        unsigned char c = *in;
        if (c == 0xC3 && in[1] >= 0x80 && in[1] <= 0xBF) {
            c = static_cast<unsigned char>(in[1] + 0x40);
            in++;
            if (c >= 0xE0 && c <= 0xFE && c != 0xF7) {
                c = static_cast<unsigned char>(c - 0x20);
            }
        }
        *out++ = static_cast<char>(c);
    }
    *out = '\0';
}

// A gentle sentence for the supply; `variant` picks one of the sentences of its state.
inline void formatFriendlySupplyMessage(
    char* out, size_t size, bool sensor, SupplyStatus status, unsigned variant) {
    const bool overdue = status.state == SupplyState::OVERDUE;
    const char* tpl = overdue ? FRIENDLY_OVERDUE[variant % friendlyVariantCount(true)]
                              : FRIENDLY_SOON[variant % friendlyVariantCount(false)];
    const char* what = sensor ? "le capteur" : "le cathéter";
    char duration[16];
    formatSupplyDuration(duration, sizeof(duration), status.remainingSeconds);

    size_t used = 0;
    auto append = [&](const char* piece, size_t length) {
        for (size_t i = 0; i < length && used + 1 < size; i++) {
            out[used++] = piece[i];
        }
    };
    for (const char* p = tpl; *p;) {
        if (strncmp(p, "{quoi}", 6) == 0) {
            append(what, strlen(what));
            p += 6;
        } else if (strncmp(p, "{duree}", 7) == 0) {
            append(duration, strlen(duration));
            p += 7;
        } else {
            append(p, 1);
            p++;
        }
    }
    out[used] = '\0';
    utf8ToLatin1Capitals(out);
}

// True when a new round of reminders should start: something is due and either nothing was shown yet
// since it became due or the repeat interval has passed.
inline bool supplyRoundDue(
    bool anyDue, bool shownBefore, unsigned long nowMillis, unsigned long lastRoundMillis,
    int repeatMinutes) {
    if (!anyDue) {
        return false;
    }
    if (!shownBefore) {
        return true;
    }
    return nowMillis - lastRoundMillis >= static_cast<unsigned long>(repeatMinutes) * 60000UL;
}

// Days since 1970-01-01 of a civil date (proleptic Gregorian calendar).
inline long long supplyDaysFromCivil(int year, int month, int day) {
    year -= month <= 2 ? 1 : 0;
    const long long era = (year >= 0 ? year : year - 399) / 400;
    const long long yearOfEra = year - era * 400;
    const long long dayOfYear = (153LL * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const long long dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + dayOfEra - 719468;
}

// Civil date of a day count since 1970-01-01 (the inverse of supplyDaysFromCivil).
inline void supplyCivilFromDays(long long days, int& year, int& month, int& day) {
    days += 719468;
    const long long era = (days >= 0 ? days : days - 146096) / 146097;
    const long long dayOfEra = days - era * 146097;
    const long long yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
    const long long dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    const long long mp = (5 * dayOfYear + 2) / 153;
    day = static_cast<int>(dayOfYear - (153 * mp + 2) / 5 + 1);
    month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    year = static_cast<int>(yearOfEra + era * 400 + (month <= 2 ? 1 : 0));
}

// Writes a Unix time as the UTC timestamp Nightscout expects, such as "2026-11-13T21:05:00.000Z".
inline void formatNightscoutTimestamp(char* out, size_t size, uint32_t epoch) {
    int year, month, day;
    supplyCivilFromDays(epoch / 86400UL, year, month, day);
    const uint32_t secondOfDay = epoch % 86400UL;
    snprintf(
        out, size, "%04d-%02d-%02dT%02d:%02d:%02d.000Z", year, month, day,
        static_cast<int>(secondOfDay / 3600), static_cast<int>(secondOfDay % 3600 / 60),
        static_cast<int>(secondOfDay % 60));
}

// Merges two lists of change times, each newest first, into one list newest first. Entries within an
// hour of one already kept are dropped: a sensor change is usually followed by its start. Unused slots
// of `out` are set to 0. Returns how many entries were kept.
inline int mergeSupplyHistory(const uint32_t* a, int countA, const uint32_t* b, int countB, uint32_t* out, int max) {
    int i = 0, j = 0, kept = 0;
    while (kept < max && (i < countA || j < countB)) {
        uint32_t next;
        if (j >= countB || (i < countA && a[i] >= b[j])) {
            next = a[i++];
        } else {
            next = b[j++];
        }
        if (next == 0) {
            continue;
        }
        if (kept > 0 && out[kept - 1] - next < 3600UL) {
            continue;
        }
        out[kept++] = next;
    }
    for (int k = kept; k < max; k++) {
        out[k] = 0;
    }
    return kept;
}

// Removes from a newest-first list the entries that fall after `localEpoch` and up to `ignoreUntil`.
// They are the notes made by mistake before the user set the real date by hand: the date typed
// wins over them, while a change logged after the correction still counts. Returns the new count;
// the freed slots are set to 0.
inline int dropIgnoredSupplyChanges(uint32_t* list, int count, uint32_t localEpoch, uint32_t ignoreUntil) {
    int kept = 0;
    for (int i = 0; i < count; i++) {
        const bool ignored = ignoreUntil != 0 && list[i] > localEpoch && list[i] <= ignoreUntil;
        if (!ignored) {
            list[kept++] = list[i];
        }
    }
    for (int i = kept; i < count; i++) {
        list[i] = 0;
    }
    return kept;
}

// Reads a Nightscout timestamp such as "2026-11-13T21:05:00.000Z" (an offset such as "+01:00" is
// honoured) and returns its Unix time, or 0 when the text is not a timestamp.
inline uint32_t parseNightscoutTimestamp(const char* text) {
    if (text == nullptr) {
        return 0;
    }
    int year, month, day, hour, minute, second = 0;
    int consumed = 0;
    if (sscanf(text, "%4d-%2d-%2dT%2d:%2d%n", &year, &month, &day, &hour, &minute, &consumed) != 5) {
        return 0;
    }
    if (year < 2001 || year > 2100 || month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 ||
        minute > 59) {
        return 0;
    }
    const char* rest = text + consumed;
    if (*rest == ':') {
        int secondsConsumed = 0;
        if (sscanf(rest, ":%2d%n", &second, &secondsConsumed) != 1 || second > 60) {
            return 0;
        }
        rest += secondsConsumed;
    }
    if (*rest == '.') {
        rest++;
        while (*rest >= '0' && *rest <= '9') {
            rest++;
        }
    }
    long long offsetSeconds = 0;
    if (*rest == '+' || *rest == '-') {
        const int sign = *rest == '-' ? -1 : 1;
        int offsetHours = 0, offsetMinutes = 0;
        if (sscanf(rest + 1, "%2d:%2d", &offsetHours, &offsetMinutes) < 2 &&
            sscanf(rest + 1, "%2d%2d", &offsetHours, &offsetMinutes) < 2) {
            return 0;
        }
        offsetSeconds = sign * (offsetHours * 3600LL + offsetMinutes * 60LL);
    }
    const long long epoch = supplyDaysFromCivil(year, month, day) * 86400LL + hour * 3600LL +
                            minute * 60LL + second - offsetSeconds;
    if (epoch < static_cast<long long>(SUPPLY_MIN_VALID_EPOCH) || epoch > 0xFFFFFFFFLL) {
        return 0;
    }
    return static_cast<uint32_t>(epoch);
}

#endif
