#ifndef BGDISPLAYFACECRITTER_H
#define BGDISPLAYFACECRITTER_H

#include "BGDisplayFaceTextBase.h"
#include "BGDisplayFaceWithAge.h"

// Cute character faces. Each critter is a 13x8 sprite beside the reading; its body color follows the
// glucose range: it keeps its own colors in range, turns entirely to the range color outside it and
// goes gray when the data is old. Eyes blink; a sprite without eyes sparkles.
// Palette: 0 = transparent, 1 = body, 2 = accent, 3 = eye, 4-7 = extra colors (black ones never change).
class BGDisplayFaceCritter : public BGDisplayFaceTextBase, public BGDisplayFaceWithAge {
public:
    // index is the critter's position in the sprite table; the registration order in
    // BGDisplayManager::setup() follows it.
    explicit BGDisplayFaceCritter(size_t index);
    static size_t count();
    void showReadings(const std::list<GlucoseReading>& readings, bool dataIsOld = false) const override;
    unsigned long getAnimationStepMillis() const override;
    void showAnimationFrame(
        const std::list<GlucoseReading>& readings, unsigned long frame) const override;

private:
    size_t critterIndex;
    const uint16_t* getPalette(BG_LEVEL level, bool dataIsOld) const;
    // frame < 0 draws the still sprite.
    void drawSprite(const uint8_t* sprite, const uint16_t* palette, long frame) const;
};

#endif  // BGDISPLAYFACECRITTER_H
