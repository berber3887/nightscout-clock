#include "BGDisplayFaceCritter.h"

#include "BGDisplayManager.h"
#include "globals.h"

namespace {

// 13x8 sprites, one palette index per pixel (row-major). 0 = transparent,
// 1 = body (dynamic level color), 2 = accent, 3 = eye, 4-7 = extra static colors.
// Cat, Dog, Frog, Fox, Bunny, Narwhal, Whale, Pumpkin, Ghost, Witch, Turkey and Butterfly come from
// adamecker/nightscout-clock; the others are original drawings.

const uint8_t spritePoop[13 * 8] PROGMEM = {
    0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 3, 1, 1, 3, 1, 1, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    0, 1, 1, 3, 1, 1, 3, 1, 1, 0, 0, 0, 0,
    0, 1, 1, 1, 3, 3, 1, 1, 1, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0
};

const uint8_t spriteCat[13 * 8] PROGMEM = {
    0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 3, 1, 1, 3, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 2, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1,
    0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1,
    0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0,
    0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0
};

const uint8_t spriteDog[13 * 8] PROGMEM = {
    0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 3, 1, 1, 1, 3, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 2, 2, 1, 1, 0, 0, 0, 0, 1,
    0, 0, 1, 1, 2, 2, 1, 1, 0, 0, 0, 1, 1,
    0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0,
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
    0, 0, 2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0
};

const uint8_t spriteFrog[13 * 8] PROGMEM = {
    0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 1, 3, 1, 1, 3, 1, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    0, 1, 1, 2, 2, 2, 2, 1, 1, 0, 0, 0, 0,
    0, 1, 1, 1, 2, 2, 1, 1, 1, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0
};

const uint8_t spriteOwl[13 * 8] PROGMEM = {
    1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    1, 2, 2, 1, 1, 1, 2, 2, 1, 0, 0, 0, 0,
    1, 2, 3, 1, 1, 1, 3, 2, 1, 0, 0, 0, 0,
    1, 1, 1, 1, 4, 1, 1, 1, 1, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    0, 1, 1, 5, 1, 5, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0
};

const uint8_t spriteFox[13 * 8] PROGMEM = {
    0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0,
    0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
    1, 1, 3, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
    0, 0, 1, 1, 1, 2, 2, 1, 1, 1, 0, 0, 0,
    0, 0, 0, 1, 1, 2, 2, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0
};

const uint8_t spriteBunny[13 * 8] PROGMEM = {
    0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 2, 0, 1, 2, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 3, 1, 3, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 2, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0
};

const uint8_t spriteNarwhal[13 * 8] PROGMEM = {
    0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 0, 2, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 3, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1,
    0, 0, 1, 1, 2, 1, 1, 1, 0, 0, 0, 1, 1,
    0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0,
    0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0
};

const uint8_t spriteWhale[13 * 8] PROGMEM = {
    0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1,
    0, 1, 1, 3, 1, 1, 1, 3, 1, 1, 1, 1, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1,
    0, 0, 1, 1, 2, 2, 2, 2, 1, 1, 0, 1, 1,
    0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1,
    0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0
};

const uint8_t spriteOctopus[13 * 8] PROGMEM = {
    0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    1, 1, 3, 1, 1, 1, 3, 1, 1, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0
};

const uint8_t spriteTurtle[13 * 8] PROGMEM = {
    0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 2, 1, 1, 1, 0, 0, 0, 0,
    0, 0, 1, 1, 2, 1, 2, 1, 1, 0, 0, 0, 0,
    0, 1, 1, 1, 2, 1, 2, 1, 1, 0, 4, 0, 0,
    1, 1, 1, 1, 1, 2, 1, 1, 1, 4, 3, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 4, 4, 4, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 4, 0, 0, 0,
    0, 0, 4, 4, 0, 0, 4, 4, 0, 0, 0, 0, 0
};

const uint8_t spriteMonkey[13 * 8] PROGMEM = {
    0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    2, 2, 1, 1, 1, 1, 1, 2, 2, 0, 0, 0, 0,
    2, 2, 4, 3, 4, 3, 4, 2, 2, 0, 0, 0, 0,
    0, 1, 4, 4, 4, 4, 4, 1, 0, 0, 0, 0, 0,
    0, 1, 4, 4, 3, 4, 4, 1, 0, 0, 0, 0, 0,
    0, 0, 1, 4, 4, 4, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0
};

const uint8_t spritePumpkin[13 * 8] PROGMEM = {
    0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
    0, 0, 1, 3, 1, 1, 3, 3, 1, 1, 3, 1, 0,
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
    0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 1, 1, 0,
    0, 0, 1, 1, 1, 3, 1, 1, 3, 1, 1, 1, 0,
    0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0
};

const uint8_t spriteGhost[13 * 8] PROGMEM = {
    0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
    0, 0, 1, 3, 1, 1, 1, 1, 3, 1, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
    0, 0, 1, 1, 1, 3, 3, 1, 1, 1, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
    0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0
};

const uint8_t spriteBat[13 * 8] PROGMEM = {
    1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1,
    1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1,
    1, 1, 1, 1, 1, 3, 1, 3, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
    0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0,
    0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0
};

const uint8_t spriteWitch[13 * 8] PROGMEM = {
    0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
    0, 0, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0,
    0, 0, 2, 3, 2, 2, 2, 3, 2, 0, 0, 0, 0,
    0, 0, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0
};

const uint8_t spriteTurkey[13 * 8] PROGMEM = {
    0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 3, 1, 3, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 2, 2, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 2, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    0, 4, 5, 6, 4, 5, 6, 4, 5, 6, 4, 0, 0,
    4, 4, 5, 5, 6, 6, 6, 5, 5, 4, 4, 0, 0
};

const uint8_t spriteDinosaur[13 * 8] PROGMEM = {
    0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 1, 3, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0,
    1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 0, 0,
    1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0
};

const uint8_t spriteButterfly[13 * 8] PROGMEM = {
    0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0,
    0, 1, 1, 1, 1, 2, 1, 1, 1, 1, 0, 0, 0,
    0, 1, 1, 1, 1, 2, 1, 1, 1, 1, 0, 0, 0,
    0, 0, 1, 1, 1, 2, 1, 1, 1, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 2, 1, 1, 1, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 2, 1, 1, 1, 1, 0, 0, 0,
    0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const uint8_t spriteBee[13 * 8] PROGMEM = {
    0, 0, 2, 2, 0, 2, 2, 0, 0, 0, 0, 0, 0,
    0, 2, 2, 2, 0, 2, 2, 2, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 3, 1, 3, 1, 1, 1, 0, 0, 0,
    0, 0, 1, 1, 3, 1, 3, 1, 1, 3, 1, 0, 0,
    0, 0, 1, 1, 3, 1, 3, 1, 1, 1, 1, 0, 0,
    0, 0, 0, 1, 3, 1, 3, 1, 1, 1, 0, 0, 0,
    0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const uint8_t spriteHeart[13 * 8] PROGMEM = {
    0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0,
    1, 2, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0
};

const uint8_t spriteQueen[13 * 8] PROGMEM = {
    0, 2, 0, 2, 0, 2, 0, 2, 0, 0, 0, 0, 0,
    0, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0,
    0, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0,
    0, 4, 5, 3, 5, 3, 5, 4, 0, 0, 0, 0, 0,
    0, 4, 5, 5, 5, 5, 5, 4, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0
};

const uint8_t spriteSanta[13 * 8] PROGMEM = {
    0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0,
    0, 2, 5, 3, 5, 3, 5, 2, 0, 0, 0, 0, 0,
    0, 2, 2, 5, 5, 5, 2, 2, 0, 0, 0, 0, 0,
    0, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0,
    0, 0, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0
};

const uint8_t spriteGift[13 * 8] PROGMEM = {
    0, 0, 2, 0, 2, 0, 2, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 2, 1, 1, 1, 1, 0, 0, 0, 0,
    1, 1, 1, 1, 2, 1, 1, 1, 1, 0, 0, 0, 0,
    0, 1, 1, 1, 2, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 2, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 2, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 2, 1, 1, 1, 0, 0, 0, 0, 0
};

const uint8_t spriteStar[13 * 8] PROGMEM = {
    0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0
};

const uint8_t spriteDrop[13 * 8] PROGMEM = {
    0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0
};

const uint8_t spriteBasketball[13 * 8] PROGMEM = {
    0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 4, 1, 1, 4, 1, 1, 4, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 4, 1, 1, 1, 1, 0, 0, 0, 0,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 0, 0, 0, 0,
    1, 1, 1, 1, 4, 1, 1, 1, 1, 0, 0, 0, 0,
    1, 1, 1, 1, 4, 1, 1, 1, 1, 0, 0, 0, 0,
    0, 4, 1, 1, 4, 1, 1, 4, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0
};

const uint8_t spriteFootball[13 * 8] PROGMEM = {
    0, 0, 1, 1, 4, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 4, 4, 4, 1, 1, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 4, 1, 1, 1, 1, 0, 0, 0, 0,
    1, 4, 1, 1, 1, 1, 1, 4, 1, 0, 0, 0, 0,
    1, 4, 4, 1, 1, 1, 4, 4, 1, 0, 0, 0, 0,
    1, 1, 4, 1, 4, 1, 4, 1, 1, 0, 0, 0, 0,
    0, 1, 1, 1, 4, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0
};

const uint8_t spriteChristmasTree[13 * 8] PROGMEM = {
    0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 5, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 6, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 5, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0
};

const uint8_t spriteSnowman[13 * 8] PROGMEM = {
    0, 0, 0, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 3, 1, 3, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 6, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0
};

const uint8_t spriteBasketballJersey[13 * 8] PROGMEM = {
    1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,
    1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 2, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 2, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 2, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0
};

const uint8_t spriteSnowflake[13 * 8] PROGMEM = {
    1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0,
    0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0,
    1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const uint8_t spriteHen[13 * 8] PROGMEM = {
    0, 0, 0, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 3, 1, 3, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 4, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 5, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 4, 0, 4, 0, 0, 0, 0, 0, 0, 0
};

const uint8_t spriteEasterEggs[13 * 8] PROGMEM = {
    0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
    0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0,
    1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0,
    2, 2, 2, 2, 2, 0, 4, 4, 4, 4, 4, 0, 0,
    1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0,
    5, 5, 5, 5, 5, 0, 2, 2, 2, 2, 2, 0, 0,
    1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0,
    0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0
};

const uint8_t spriteStrawberry[13 * 8] PROGMEM = {
    0, 0, 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    1, 1, 2, 1, 1, 1, 2, 1, 1, 0, 0, 0, 0,
    1, 1, 1, 1, 2, 1, 1, 1, 1, 0, 0, 0, 0,
    1, 1, 2, 1, 1, 1, 2, 1, 1, 0, 0, 0, 0,
    0, 1, 1, 1, 2, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 1, 2, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0
};

const uint8_t spriteSkull[13 * 8] PROGMEM = {
    0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0,
    1, 1, 3, 3, 1, 1, 3, 3, 1, 0, 0, 0, 0,
    1, 1, 3, 3, 1, 1, 3, 3, 1, 0, 0, 0, 0,
    0, 1, 1, 1, 3, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0
};

const uint8_t spriteFlower[13 * 8] PROGMEM = {
    0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    1, 1, 1, 2, 2, 2, 1, 1, 1, 0, 0, 0, 0,
    1, 1, 1, 2, 2, 2, 1, 1, 1, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0
};

const uint8_t spriteInsulinPump[13 * 8] PROGMEM = {
    1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    1, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0,
    1, 2, 2, 2, 2, 1, 4, 4, 4, 4, 4, 4, 0,
    1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 4, 0,
    1, 5, 1, 1, 5, 1, 0, 0, 0, 0, 0, 4, 0,
    1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 4, 0,
    1, 1, 1, 1, 1, 1, 0, 0, 4, 4, 4, 4, 0,
    1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0
};

const uint8_t spriteMoon[13 * 8] PROGMEM = {
    0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 0, 0, 2, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0
};

const uint8_t spriteMotorbike[13 * 8] PROGMEM = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0,
    0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0,
    0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
    0, 4, 4, 0, 0, 1, 1, 1, 0, 0, 4, 4, 0,
    4, 0, 0, 4, 0, 1, 1, 1, 0, 4, 0, 0, 4,
    4, 0, 0, 4, 0, 0, 0, 0, 0, 4, 0, 0, 4,
    0, 4, 4, 0, 0, 0, 0, 0, 0, 0, 4, 4, 0
};

struct CritterColors {
    uint16_t body;  // natural body color (for the NORMAL level)
    uint16_t accent;
    uint16_t eye;
    uint16_t c4;
    uint16_t c5;
    uint16_t c6;
    uint16_t c7;
};

struct CritterDef {
    const uint8_t* sprite;
    CritterColors colors;
};

const CritterDef CRITTERS[] = {
    {spritePoop, {0xC343, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}},  // Poop
    {spriteCat, {0xFC00, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}},  // Cat
    {spriteDog, {0xD343, 0xFD20, 0x0000, 0xFD20, 0xFD20, 0xFD20, 0xFD20}},  // Dog
    {spriteFrog, {0x07E0, 0x87F0, 0x0000, 0x87F0, 0x87F0, 0x87F0, 0x87F0}},  // Frog
    {spriteOwl, {0xC343, 0xFFFF, 0x0000, 0xFD20, 0xFE60, 0xFD20, 0xFD20}},  // Owl
    {spriteFox, {0xFC00, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}},  // Fox
    {spriteBunny, {0xFFFF, 0xF81F, 0x0000, 0xF81F, 0xF81F, 0xF81F, 0xF81F}},  // Bunny
    {spriteNarwhal, {0x94B2, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}},  // Narwhal
    {spriteWhale, {0x439F, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}},  // Whale
    {spriteOctopus, {0xD01F, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}},  // Octopus
    {spriteTurtle, {0x07E0, 0x87F0, 0x0000, 0x8FE0, 0x8FE0, 0x8FE0, 0x8FE0}},  // Turtle
    {spriteMonkey, {0xC343, 0xFD20, 0x0000, 0xFCF4, 0xFCF4, 0xFCF4, 0xFCF4}},  // Monkey
    {spritePumpkin, {0xFC00, 0x07E0, 0x0000, 0x07E0, 0x07E0, 0x07E0, 0x07E0}},  // Pumpkin
    {spriteGhost, {0xFFFF, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}},  // Ghost
    {spriteBat, {0xB01F, 0xFFFF, 0xF800, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}},  // Bat
    {spriteWitch, {0xA81F, 0x07E0, 0x0000, 0x07E0, 0x07E0, 0x07E0, 0x07E0}},  // Witch
    {spriteTurkey, {0xD343, 0xF800, 0x0000, 0xF800, 0xFC00, 0xFFE0, 0xF800}},  // Turkey
    {spriteDinosaur, {0x07E0, 0xFFE0, 0x0000, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0}},  // Dinosaur
    {spriteButterfly, {0xFC9F, 0x8410, 0x0000, 0x8410, 0x8410, 0x8410, 0x8410}},  // Butterfly
    {spriteBee, {0xFFE0, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}},  // Bee
    {spriteHeart, {0xF810, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}},  // Heart
    {spriteQueen, {0xA81F, 0xFFE0, 0x0000, 0xC343, 0xFCF4, 0xFCF4, 0xFCF4}},  // Queen
    {spriteSanta, {0xF800, 0xFFFF, 0x0000, 0xFCF4, 0xFCF4, 0xFCF4, 0xFCF4}},  // Santa
    {spriteGift, {0x07E0, 0xFFE0, 0x0000, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0}},  // Gift
    {spriteStar, {0xFFE0, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}},  // Star
    {spriteDrop, {0x041F, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}},  // Drop
    {spriteBasketball, {0xFC00, 0xFFFF, 0x0000, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF}},  // Basketball
    {spriteFootball, {0xFFFF, 0xFFFF, 0x0000, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF}},  // Football
    {spriteChristmasTree, {0x07E0, 0xFFE0, 0x0000, 0xC343, 0xF800, 0x041F, 0xFFFF}},  // ChristmasTree
    {spriteSnowman, {0xFFFF, 0xFFFF, 0x0000, 0x001F, 0xF800, 0xFD20, 0xFFFF}},  // Snowman
    {spriteBasketballJersey, {0xF800, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}},  // BasketballJersey
    {spriteSnowflake, {0x07FF, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}},  // Snowflake
    {spriteHen, {0xFFFF, 0xFFFF, 0x0000, 0xFD20, 0xF800, 0xFFFF, 0xFFFF}},  // Hen
    {spriteEasterEggs, {0xF81F, 0xFFE0, 0x0000, 0x07FF, 0x07E0, 0xFFFF, 0xFFFF}},  // EasterEggs
    {spriteStrawberry, {0xF800, 0xFFE0, 0x0000, 0x07E0, 0xFFFF, 0xFFFF, 0xFFFF}},  // Strawberry
    {spriteSkull, {0xFFFF, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}},  // Skull
    {spriteFlower, {0xF81F, 0xFFE0, 0x0000, 0x07E0, 0xFFFF, 0xFFFF, 0xFFFF}},  // Flower
    {spriteInsulinPump, {0x041F, 0xFFFF, 0x0000, 0x7BEF, 0xFFFF, 0xFFFF, 0xFFFF}},  // InsulinPump
    {spriteMoon, {0xFFE0, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}},  // Moon
    {spriteMotorbike, {0xF800, 0xFFE0, 0x0000, 0x7BEF, 0xFFFF, 0xFFFF, 0xFFFF}},  // Motorbike
};

}  // namespace

size_t BGDisplayFaceCritter::count() { return sizeof(CRITTERS) / sizeof(CRITTERS[0]); }

BGDisplayFaceCritter::BGDisplayFaceCritter(size_t index) : critterIndex(index < count() ? index : 0) {}

const uint16_t* BGDisplayFaceCritter::getPalette(BG_LEVEL level, bool dataIsOld) const {
    // Static to avoid a stack allocation on each frame. RAM-based because the colors depend on the glucose
    // range.
    static uint16_t palette[8];
    const CritterColors& cc = CRITTERS[critterIndex].colors;
    const uint16_t natural[8] = {0x0000, cc.body, cc.accent, cc.eye, cc.c4, cc.c5, cc.c6, cc.c7};

    // Outside the normal range the whole animal takes the range color; the eyes and the black details
    // keep theirs. Stale data turns it gray.
    bool recolor = dataIsOld;
    uint16_t tint = dataIsOld ? getDataOldColor() : 0;
    if (!dataIsOld && level != BG_LEVEL::NORMAL && level != BG_LEVEL::INVALID) {
        recolor = true;
        tint = getBandColor(level);
    }
    for (int i = 0; i < 8; i++) {
        palette[i] = (recolor && i != 0 && i != 3 && natural[i] != 0x0000) ? tint : natural[i];
    }
    return palette;
}

// Eyes close for a moment every few seconds. A sprite without eyes gets a white sparkle that moves across it.
void BGDisplayFaceCritter::drawSprite(
    const uint8_t* sprite, const uint16_t* palette, long frame) const {
    bool hasEyes = false;
    int pixels = 0;
    for (int i = 0; i < 13 * 8; i++) {
        uint8_t index = pgm_read_byte(&sprite[i]);
        hasEyes = hasEyes || index == 3;
        pixels += index != 0;
    }
    const bool animate = frame >= 0;
    const bool eyesClosed = animate && hasEyes && frame % 30 == 0;
    const int sparkleAt = animate && !hasEyes && pixels > 0 ? static_cast<int>((frame / 2) % pixels) : -1;

    int seen = 0;
    for (int16_t row = 0; row < 8; row++) {
        for (int16_t col = 0; col < 13; col++) {
            uint8_t paletteIndex = pgm_read_byte(&sprite[row * 13 + col]);
            if (paletteIndex == 0) {
                continue;  // transparent
            }
            uint16_t color = palette[paletteIndex];
            if (eyesClosed && paletteIndex == 3) {
                color = palette[1];
            }
            if (seen++ == sparkleAt) {
                color = static_cast<uint16_t>(DISPLAY_COLOR::WHITE);
            }
            DisplayManager.drawPixel(col, row, color, false);
        }
    }
}

unsigned long BGDisplayFaceCritter::getAnimationStepMillis() const { return 150; }

void BGDisplayFaceCritter::showAnimationFrame(
    const std::list<GlucoseReading>& readings, unsigned long frame) const {
    auto bgLevel = bgDisplayManager.getGlucoseIntervals().getBGLevel(readings.back().sgv);
    DisplayManager.clearMatrixPart(0, 0, 13, 8);
    drawSprite(CRITTERS[critterIndex].sprite, getPalette(bgLevel, false), static_cast<long>(frame));
}

void BGDisplayFaceCritter::showReadings(const std::list<GlucoseReading>& readings, bool dataIsOld) const {
    auto lastReading = readings.back();
    auto bgLevel = bgDisplayManager.getGlucoseIntervals().getBGLevel(lastReading.sgv);

    drawSprite(CRITTERS[critterIndex].sprite, getPalette(bgLevel, dataIsOld), -1);
    // The trend arrow sits in the gap between the sprite and the reading.
    showTrendArrow(lastReading, 14, 2, dataIsOld);
    showReading(lastReading, MATRIX_WIDTH - 1, 6, TEXT_ALIGNMENT::RIGHT, FONT_TYPE::MEDIUM, dataIsOld);
    drawTimerBlocks(lastReading, 16, 16, 7);
}
