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

const uint8_t spriteMermaid[13 * 8] PROGMEM = {
    0, 0, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0,
    0, 4, 4, 2, 2, 4, 4, 0, 0, 0, 0, 0, 0,
    0, 4, 2, 3, 2, 3, 2, 4, 0, 0, 0, 0, 0,
    0, 4, 2, 2, 2, 2, 2, 4, 0, 0, 0, 0, 0,
    0, 0, 4, 1, 1, 1, 4, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0
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
    {spriteMermaid, {0x07FF, 0xFCF4, 0x0000, 0xFD20, 0xFD20, 0xFD20, 0xFD20}},  // Mermaid
    {spriteDinosaur, {0x07E0, 0xFFE0, 0x0000, 0xFFE0, 0xFFE0, 0xFFE0, 0xFFE0}},  // Dinosaur
    {spriteButterfly, {0xFC9F, 0x8410, 0x0000, 0x8410, 0x8410, 0x8410, 0x8410}},  // Butterfly
    {spriteBee, {0xFFE0, 0xFFFF, 0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF}},  // Bee
};

}  // namespace

size_t BGDisplayFaceCritter::count() { return sizeof(CRITTERS) / sizeof(CRITTERS[0]); }

BGDisplayFaceCritter::BGDisplayFaceCritter(size_t index) : critterIndex(index < count() ? index : 0) {}

const uint16_t* BGDisplayFaceCritter::getPalette(BG_LEVEL level, bool dataIsOld) const {
    // Static to avoid a stack allocation on each frame. RAM-based because the body color is computed from
    // the glucose level.
    static uint16_t palette[8];
    const CritterColors& cc = CRITTERS[critterIndex].colors;
    uint16_t body;
    if (dataIsOld) {
        body = getDataOldColor();
    } else {
        switch (level) {
            case BG_LEVEL::URGENT_LOW:
            case BG_LEVEL::URGENT_HIGH:
                body = 0xF800;  // red
                break;
            case BG_LEVEL::WARNING_LOW:
            case BG_LEVEL::WARNING_HIGH:
                body = 0xFFE0;  // yellow
                break;
            default:
                body = cc.body;
                break;
        }
    }
    uint16_t extra[] = {cc.c4, cc.c5, cc.c6, cc.c7};
    palette[0] = 0x0000;  // transparent (unused)
    palette[1] = body;
    palette[2] = dataIsOld ? getDataOldColor() : cc.accent;
    palette[3] = cc.eye;
    for (int i = 0; i < 4; i++) {
        palette[4 + i] = dataIsOld ? getDataOldColor() : extra[i];
    }
    return palette;
}

void BGDisplayFaceCritter::drawSprite(const uint8_t* sprite, const uint16_t* palette) const {
    for (int16_t row = 0; row < 8; row++) {
        for (int16_t col = 0; col < 13; col++) {
            uint8_t paletteIndex = pgm_read_byte(&sprite[row * 13 + col]);
            if (paletteIndex == 0) {
                continue;  // transparent
            }
            DisplayManager.drawPixel(col, row, palette[paletteIndex], false);
        }
    }
}

void BGDisplayFaceCritter::showReadings(const std::list<GlucoseReading>& readings, bool dataIsOld) const {
    auto lastReading = readings.back();
    auto bgLevel = bgDisplayManager.getGlucoseIntervals().getBGLevel(lastReading.sgv);

    drawSprite(CRITTERS[critterIndex].sprite, getPalette(bgLevel, dataIsOld));
    // The trend arrow sits in the gap between the sprite and the reading.
    showTrendArrow(lastReading, 14, 2, dataIsOld);
    showReading(lastReading, MATRIX_WIDTH - 1, 6, TEXT_ALIGNMENT::RIGHT, FONT_TYPE::MEDIUM, dataIsOld);
    drawTimerBlocks(lastReading, 16, 16, 7);
}
