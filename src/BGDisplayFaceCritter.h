#ifndef BGDISPLAYFACECRITTER_H
#define BGDISPLAYFACECRITTER_H

#include "BGDisplayFaceTextBase.h"
#include "BGDisplayFaceWithAge.h"

// Cute character faces. Each critter is a 13x8 sprite beside the reading; its body color follows the
// glucose level (natural, yellow or red) and goes gray when the data is old.
// Palette: 0 = transparent, 1 = body (dynamic), 2 = accent, 3 = eye, 4-7 = extra static colors.
class BGDisplayFaceCritter : public BGDisplayFaceTextBase, public BGDisplayFaceWithAge {
public:
    // index is the critter's position in the sprite table; the registration order in
    // BGDisplayManager::setup() follows it.
    explicit BGDisplayFaceCritter(size_t index);
    static size_t count();
    void showReadings(const std::list<GlucoseReading>& readings, bool dataIsOld = false) const override;

private:
    size_t critterIndex;
    const uint16_t* getPalette(BG_LEVEL level, bool dataIsOld) const;
    void drawSprite(const uint8_t* sprite, const uint16_t* palette) const;
};

#endif  // BGDISPLAYFACECRITTER_H
