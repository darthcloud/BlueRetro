/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/config.h"
#include "adapter/kb_monitor.h"
#include "cdi.h"

#define CDI_KB_SHIFT 0x01
#define CDI_KB_CAPS 0x02
#define CDI_KB_ALT 0x04
#define CDI_KB_CTRL 0x08

enum {
    CDI_2 = 4,
    CDI_1,
};

static DRAM_ATTR const uint8_t cdi_axes_idx[ADAPTER_MAX_AXES] =
{
/*  AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY, TRIG_L, TRIG_R  */
    0,       1,       0,       1,       0,      1
};

static DRAM_ATTR const struct ctrl_meta cdi_axes_meta[ADAPTER_MAX_AXES] =
{
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 127, .abs_min = 128},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 127, .abs_min = 128, .polarity = 1},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 127, .abs_min = 128},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 127, .abs_min = 128, .polarity = 1},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 127, .abs_min = 128},
    {.size_min = -128, .size_max = 127, .neutral = 0x00, .abs_max = 127, .abs_min = 128},
};

struct cdi_map {
    uint8_t buttons;
    uint8_t align[5];
    uint8_t relative[2];
    int32_t raw_axes[2];
} __packed;

static const uint32_t cdi_mask[4] = {0x0005000F, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t cdi_desc[4] = {0x0000000F, 0x00000000, 0x00000000, 0x00000000};
static DRAM_ATTR const uint32_t cdi_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(CDI_1), 0, BIT(CDI_2), 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
};

static const uint32_t cdi_mouse_mask[4] = {0x190000F0, 0x00000000, 0x00000000, BR_COMBO_MASK};
static const uint32_t cdi_mouse_desc[4] = {0x000000F0, 0x00000000, 0x00000000, 0x00000000};
static const uint32_t cdi_mouse_btns_mask[32] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    BIT(CDI_2), 0, 0, 0,
    BIT(CDI_1), 0, 0, 0,
};

static const uint32_t cdi_kb_mask[4] = {0xE6FF0F0F, 0xFFFFFFFF, 0xFFFFFFFF, 0x0007FFFF | BR_COMBO_MASK};
static const uint32_t cdi_kb_desc[4] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};

/* Flash bite the bullet here, their is no dominant pattern between the special */
/* keys and the normal code set. So using 6 lookup table for CD-i to keep code readable */
static const uint8_t cdi_kb_code_normal[KBM_MAX] = {
 /* KB_A, KB_D, KB_S, KB_W, MOUSE_X_LEFT, MOUSE_X_RIGHT, MOUSE_Y_DOWN MOUSE_Y_UP */
    0x61, 0x64, 0x73, 0x77, 0x00, 0x00, 0x00, 0x00,
 /* KB_LEFT, KB_RIGHT, KB_DOWN, KB_UP, MOUSE_WX_LEFT, MOUSE_WX_RIGHT, MOUSE_WY_DOWN, MOUSE_WY_UP */
    0x08, 0x0C, 0x0A, 0x0B, 0x00, 0x00, 0x00, 0x00,
 /* KB_Q, KB_R, KB_E, KB_F, KB_ESC, KB_ENTER, KB_LWIN, KB_HASH */
    0x71, 0x72, 0x65, 0x66, 0x1B, 0x0D, 0x1C, 0x60,
 /* MOUSE_RIGHT, KB_Z, KB_LCTRL, MOUSE_MIDDLE, MOUSE_LEFT, KB_X, KB_LSHIFT, KB_SPACE */
    0x00, 0x7A, 0x00, 0x00, 0x00, 0x78, 0x00, 0x20,

 /* KB_B, KB_C, KB_G, KB_H, KB_I, KB_J, KB_K, KB_L */
    0x62, 0x63, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C,
 /* KB_M, KB_N, KB_O, KB_P, KB_T, KB_U, KB_V, KB_Y */
    0x6D, 0x6E, 0x6F, 0x70, 0x74, 0x75, 0x76, 0x79,
 /* KB_1, KB_2, KB_3, KB_4, KB_5, KB_6, KB_7, KB_8 */
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
 /* KB_9, KB_0, KB_BACKSPACE, KB_TAB, KB_MINUS, KB_EQUAL, KB_LEFTBRACE, KB_RIGHTBRACE */
    0x39, 0x30, 0x7F, 0x09, 0x2D, 0x3D, 0x5B, 0x5D,

 /* KB_BACKSLASH, KB_SEMICOLON, KB_APOSTROPHE, KB_GRAVE, KB_COMMA, KB_DOT, KB_SLASH, KB_CAPSLOCK */
    0x5C, 0x3B, 0x27, 0x00, 0x2C, 0x2E, 0x2F, 0x00,
 /* KB_F1, KB_F2, KB_F3, KB_F4, KB_F5, KB_F6, KB_F7, KB_F8 */
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
 /* KB_F9, KB_F10, KB_F11, KB_F12, KB_PSCREEN, KB_SCROLL, KB_PAUSE, KB_INSERT */
    0x80, 0x81, 0x82, 0x83, 0x40, 0x42, 0x43, 0x56,
 /* KB_HOME, KB_PAGEUP, KB_DEL, KB_END, KB_PAGEDOWN, KB_NUMLOCK, KB_KP_DIV, KB_KP_MULTI */
    0x1E, 0x58, 0x06, 0x55, 0x59, 0x00, 0x2F, 0x2A,

 /* KB_KP_MINUS, KB_KP_PLUS, KB_KP_ENTER, KB_KP_1, KB_KP_2, KB_KP_3, KB_KP_4, KB_KP_5 */
    0x2D, 0x2B, 0x0D, 0x31, 0x32, 0x33, 0x34, 0x35,
 /* KB_KP_6, KB_KP_7, KB_KP_8, KB_KP_9, KB_KP_0, KB_KP_DOT, KB_LALT, KB_RCTRL */
    0x36, 0x37, 0x38, 0x39, 0x30, 0x2E, 0x00, 0x00,
 /* KB_RSHIFT, KB_RALT, KB_RWIN */
    0x00, 0x00, 0x1D,
};
static const uint8_t cdi_kb_code_shift[KBM_MAX] = {
 /* KB_A, KB_D, KB_S, KB_W, MOUSE_X_LEFT, MOUSE_X_RIGHT, MOUSE_Y_DOWN MOUSE_Y_UP */
    0x41, 0x44, 0x53, 0x57, 0x00, 0x00, 0x00, 0x00,
 /* KB_LEFT, KB_RIGHT, KB_DOWN, KB_UP, MOUSE_WX_LEFT, MOUSE_WX_RIGHT, MOUSE_WY_DOWN, MOUSE_WY_UP */
    0x11, 0x14, 0x12, 0x13, 0x00, 0x00, 0x00, 0x00,
 /* KB_Q, KB_R, KB_E, KB_F, KB_ESC, KB_ENTER, KB_LWIN, KB_HASH */
    0x51, 0x52, 0x45, 0x46, 0x1B, 0x0D, 0x1C, 0x7E,
 /* MOUSE_RIGHT, KB_Z, KB_LCTRL, MOUSE_MIDDLE, MOUSE_LEFT, KB_X, KB_LSHIFT, KB_SPACE */
    0x00, 0x5A, 0x00, 0x00, 0x00, 0x58, 0x00, 0x20,

 /* KB_B, KB_C, KB_G, KB_H, KB_I, KB_J, KB_K, KB_L */
    0x42, 0x43, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C,
 /* KB_M, KB_N, KB_O, KB_P, KB_T, KB_U, KB_V, KB_Y */
    0x4D, 0x4E, 0x4F, 0x50, 0x54, 0x55, 0x56, 0x59,
 /* KB_1, KB_2, KB_3, KB_4, KB_5, KB_6, KB_7, KB_8 */
    0x21, 0x40, 0x23, 0x24, 0x25, 0x5E, 0x26, 0x2A,
 /* KB_9, KB_0, KB_BACKSPACE, KB_TAB, KB_MINUS, KB_EQUAL, KB_LEFTBRACE, KB_RIGHTBRACE */
    0x28, 0x29, 0x0F, 0x19, 0x5F, 0x2B, 0x7B, 0x7D,

 /* KB_BACKSLASH, KB_SEMICOLON, KB_APOSTROPHE, KB_GRAVE, KB_COMMA, KB_DOT, KB_SLASH, KB_CAPSLOCK */
    0x7C, 0x3A, 0x22, 0x00, 0x3C, 0x3E, 0x3F, 0x00,
 /* KB_F1, KB_F2, KB_F3, KB_F4, KB_F5, KB_F6, KB_F7, KB_F8 */
    0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
 /* KB_F9, KB_F10, KB_F11, KB_F12, KB_PSCREEN, KB_SCROLL, KB_PAUSE, KB_INSERT */
    0x88, 0x89, 0x8A, 0x8B, 0x40, 0x42, 0x43, 0x56,
 /* KB_HOME, KB_PAGEUP, KB_DEL, KB_END, KB_PAGEDOWN, KB_NUMLOCK, KB_KP_DIV, KB_KP_MULTI */
    0x1E, 0x58, 0x57, 0x55, 0x59, 0x00, 0x2F, 0x2A,

 /* KB_KP_MINUS, KB_KP_PLUS, KB_KP_ENTER, KB_KP_1, KB_KP_2, KB_KP_3, KB_KP_4, KB_KP_5 */
    0x2D, 0x2B, 0x0D, 0x31, 0x32, 0x33, 0x34, 0x35,
 /* KB_KP_6, KB_KP_7, KB_KP_8, KB_KP_9, KB_KP_0, KB_KP_DOT, KB_LALT, KB_RCTRL */
    0x36, 0x37, 0x38, 0x39, 0x30, 0x2E, 0x00, 0x00,
 /* KB_RSHIFT, KB_RALT, KB_RWIN */
    0x00, 0x00, 0x1D,
};
static const uint8_t cdi_kb_code_alt[KBM_MAX] = {
 /* KB_A, KB_D, KB_S, KB_W, MOUSE_X_LEFT, MOUSE_X_RIGHT, MOUSE_Y_DOWN MOUSE_Y_UP */
    0xE1, 0xE4, 0xF3, 0xF7, 0x00, 0x00, 0x00, 0x00,
 /* KB_LEFT, KB_RIGHT, KB_DOWN, KB_UP, MOUSE_WX_LEFT, MOUSE_WX_RIGHT, MOUSE_WY_DOWN, MOUSE_WY_UP */
    0x15, 0x18, 0x16, 0x17, 0x00, 0x00, 0x00, 0x00,
 /* KB_Q, KB_R, KB_E, KB_F, KB_ESC, KB_ENTER, KB_LWIN, KB_HASH */
    0xF1, 0xF2, 0xE5, 0xE6, 0x1B, 0x0D, 0x1C, 0xE0,
 /* MOUSE_RIGHT, KB_Z, KB_LCTRL, MOUSE_MIDDLE, MOUSE_LEFT, KB_X, KB_LSHIFT, KB_SPACE */
    0x00, 0xFA, 0x00, 0x00, 0x00, 0xF8, 0x00, 0xA0,

 /* KB_B, KB_C, KB_G, KB_H, KB_I, KB_J, KB_K, KB_L */
    0xE2, 0xE3, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC,
 /* KB_M, KB_N, KB_O, KB_P, KB_T, KB_U, KB_V, KB_Y */
    0xED, 0xEE, 0xEF, 0xF0, 0xF4, 0xF5, 0xF6, 0xF9,
 /* KB_1, KB_2, KB_3, KB_4, KB_5, KB_6, KB_7, KB_8 */
    0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8,
 /* KB_9, KB_0, KB_BACKSPACE, KB_TAB, KB_MINUS, KB_EQUAL, KB_LEFTBRACE, KB_RIGHTBRACE */
    0xB9, 0xB0, 0x0F, 0x19, 0xAD, 0xBD, 0xDB, 0xDD,

 /* KB_BACKSLASH, KB_SEMICOLON, KB_APOSTROPHE, KB_GRAVE, KB_COMMA, KB_DOT, KB_SLASH, KB_CAPSLOCK */
    0xDC, 0xBB, 0xA7, 0x00, 0xAC, 0xAE, 0xAF, 0x00,
 /* KB_F1, KB_F2, KB_F3, KB_F4, KB_F5, KB_F6, KB_F7, KB_F8 */
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
 /* KB_F9, KB_F10, KB_F11, KB_F12, KB_PSCREEN, KB_SCROLL, KB_PAUSE, KB_INSERT */
    0x90, 0x91, 0x92, 0x93, 0xC0, 0xC2, 0xC3, 0xD6,
 /* KB_HOME, KB_PAGEUP, KB_DEL, KB_END, KB_PAGEDOWN, KB_NUMLOCK, KB_KP_DIV, KB_KP_MULTI */
    0x1E, 0xD8, 0xD7, 0xD5, 0xD9, 0x00, 0xAF, 0xAA,

 /* KB_KP_MINUS, KB_KP_PLUS, KB_KP_ENTER, KB_KP_1, KB_KP_2, KB_KP_3, KB_KP_4, KB_KP_5 */
    0xAD, 0xAB, 0x0D, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5,
 /* KB_KP_6, KB_KP_7, KB_KP_8, KB_KP_9, KB_KP_0, KB_KP_DOT, KB_LALT, KB_RCTRL */
    0xB6, 0xB7, 0xB8, 0xB9, 0xB0, 0xAE, 0x00, 0x00,
 /* KB_RSHIFT, KB_RALT, KB_RWIN */
    0x00, 0x00, 0x1D,
};
static const uint8_t cdi_kb_code_ctrl[KBM_MAX] = {
 /* KB_A, KB_D, KB_S, KB_W, MOUSE_X_LEFT, MOUSE_X_RIGHT, MOUSE_Y_DOWN MOUSE_Y_UP */
    0x01, 0x04, 0x13, 0x17, 0x00, 0x00, 0x00, 0x00,
 /* KB_LEFT, KB_RIGHT, KB_DOWN, KB_UP, MOUSE_WX_LEFT, MOUSE_WX_RIGHT, MOUSE_WY_DOWN, MOUSE_WY_UP */
    0x02, 0x05, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00,
 /* KB_Q, KB_R, KB_E, KB_F, KB_ESC, KB_ENTER, KB_LWIN, KB_HASH */
    0x11, 0x12, 0x05, 0x06, 0x1B, 0x0D, 0x1C, 0x60,
 /* MOUSE_RIGHT, KB_Z, KB_LCTRL, MOUSE_MIDDLE, MOUSE_LEFT, KB_X, KB_LSHIFT, KB_SPACE */
    0x00, 0x1A, 0x00, 0x00, 0x00, 0x18, 0x00, 0x20,

 /* KB_B, KB_C, KB_G, KB_H, KB_I, KB_J, KB_K, KB_L */
    0x02, 0x03, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C,
 /* KB_M, KB_N, KB_O, KB_P, KB_T, KB_U, KB_V, KB_Y */
    0x0D, 0x0E, 0x0F, 0x10, 0x14, 0x15, 0x16, 0x19,
 /* KB_1, KB_2, KB_3, KB_4, KB_5, KB_6, KB_7, KB_8 */
    0x31, 0x0, 0x33, 0x34, 0x35, 0x1E, 0x37, 0x38,
 /* KB_9, KB_0, KB_BACKSPACE, KB_TAB, KB_MINUS, KB_EQUAL, KB_LEFTBRACE, KB_RIGHTBRACE */
    0x39, 0x30, 0x1F, 0x19, 0x1F, 0x3D, 0x5B, 0x5D,

 /* KB_BACKSLASH, KB_SEMICOLON, KB_APOSTROPHE, KB_GRAVE, KB_COMMA, KB_DOT, KB_SLASH, KB_CAPSLOCK */
    0x5C, 0x3B, 0x27, 0x00, 0x2C, 0x2E, 0x2F, 0x00,
 /* KB_F1, KB_F2, KB_F3, KB_F4, KB_F5, KB_F6, KB_F7, KB_F8 */
    0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
 /* KB_F9, KB_F10, KB_F11, KB_F12, KB_PSCREEN, KB_SCROLL, KB_PAUSE, KB_INSERT */
    0x98, 0x99, 0x9A, 0x9B, 0x41, 0x42, 0x44, 0x56,
 /* KB_HOME, KB_PAGEUP, KB_DEL, KB_END, KB_PAGEDOWN, KB_NUMLOCK, KB_KP_DIV, KB_KP_MULTI */
    0x1E, 0x58, 0x57, 0x55, 0x59, 0x00, 0x2F, 0x2A,

 /* KB_KP_MINUS, KB_KP_PLUS, KB_KP_ENTER, KB_KP_1, KB_KP_2, KB_KP_3, KB_KP_4, KB_KP_5 */
    0x2D, 0x2B, 0x0D, 0x31, 0x32, 0x33, 0x34, 0x35,
 /* KB_KP_6, KB_KP_7, KB_KP_8, KB_KP_9, KB_KP_0, KB_KP_DOT, KB_LALT, KB_RCTRL */
    0x36, 0x37, 0x38, 0x39, 0x30, 0x2E, 0x00, 0x00,
 /* KB_RSHIFT, KB_RALT, KB_RWIN */
    0x00, 0x00, 0x1D,
};
static const uint8_t cdi_kb_code_ctrl_alt[KBM_MAX] = {
 /* KB_A, KB_D, KB_S, KB_W, MOUSE_X_LEFT, MOUSE_X_RIGHT, MOUSE_Y_DOWN MOUSE_Y_UP */
    0x81, 0x84, 0x93, 0x97, 0x00, 0x00, 0x00, 0x00,
 /* KB_LEFT, KB_RIGHT, KB_DOWN, KB_UP, MOUSE_WX_LEFT, MOUSE_WX_RIGHT, MOUSE_WY_DOWN, MOUSE_WY_UP */
    0x15, 0x18, 0x16, 0x17, 0x00, 0x00, 0x00, 0x00,
 /* KB_Q, KB_R, KB_E, KB_F, KB_ESC, KB_ENTER, KB_LWIN, KB_HASH */
    0x91, 0x92, 0x85, 0x86, 0x1B, 0x0D, 0x1C, 0xE0,
 /* MOUSE_RIGHT, KB_Z, KB_LCTRL, MOUSE_MIDDLE, MOUSE_LEFT, KB_X, KB_LSHIFT, KB_SPACE */
    0x00, 0x9A, 0x00, 0x00, 0x00, 0x98, 0x00, 0xA0,

 /* KB_B, KB_C, KB_G, KB_H, KB_I, KB_J, KB_K, KB_L */
    0x82, 0x83, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C,
 /* KB_M, KB_N, KB_O, KB_P, KB_T, KB_U, KB_V, KB_Y */
    0x8D, 0x8E, 0x8F, 0x90, 0x94, 0x95, 0x96, 0x99,
 /* KB_1, KB_2, KB_3, KB_4, KB_5, KB_6, KB_7, KB_8 */
    0xB1, 0x80, 0xB3, 0xB4, 0xB5, 0x9E, 0xB7, 0xB8,
 /* KB_9, KB_0, KB_BACKSPACE, KB_TAB, KB_MINUS, KB_EQUAL, KB_LEFTBRACE, KB_RIGHTBRACE */
    0xB9, 0xB0, 0x0F, 0x19, 0x9F, 0xBD, 0xDB, 0xDD,

 /* KB_BACKSLASH, KB_SEMICOLON, KB_APOSTROPHE, KB_GRAVE, KB_COMMA, KB_DOT, KB_SLASH, KB_CAPSLOCK */
    0xDC, 0xBB, 0xA7, 0x00, 0xAC, 0xAE, 0xAF, 0x00,
 /* KB_F1, KB_F2, KB_F3, KB_F4, KB_F5, KB_F6, KB_F7, KB_F8 */
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
 /* KB_F9, KB_F10, KB_F11, KB_F12, KB_PSCREEN, KB_SCROLL, KB_PAUSE, KB_INSERT */
    0x90, 0x91, 0x92, 0x93, 0xC1, 0xC2, 0xC4, 0xD6,
 /* KB_HOME, KB_PAGEUP, KB_DEL, KB_END, KB_PAGEDOWN, KB_NUMLOCK, KB_KP_DIV, KB_KP_MULTI */
    0x1E, 0xD8, 0xD7, 0xD5, 0xD9, 0x00, 0xAF, 0xAA,

 /* KB_KP_MINUS, KB_KP_PLUS, KB_KP_ENTER, KB_KP_1, KB_KP_2, KB_KP_3, KB_KP_4, KB_KP_5 */
    0xAD, 0xAB, 0x0D, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5,
 /* KB_KP_6, KB_KP_7, KB_KP_8, KB_KP_9, KB_KP_0, KB_KP_DOT, KB_LALT, KB_RCTRL */
    0xB6, 0xB7, 0xB8, 0xB9, 0xB0, 0xAE, 0x00, 0x00,
 /* KB_RSHIFT, KB_RALT, KB_RWIN */
    0x00, 0x00, 0x1D,
};
static const uint8_t cdi_kb_code_shift_alt[KBM_MAX] = {
 /* KB_A, KB_D, KB_S, KB_W, MOUSE_X_LEFT, MOUSE_X_RIGHT, MOUSE_Y_DOWN MOUSE_Y_UP */
    0xC1, 0xC4, 0xD3, 0xD7, 0x00, 0x00, 0x00, 0x00,
 /* KB_LEFT, KB_RIGHT, KB_DOWN, KB_UP, MOUSE_WX_LEFT, MOUSE_WX_RIGHT, MOUSE_WY_DOWN, MOUSE_WY_UP */
    0x15, 0x18, 0x16, 0x17, 0x00, 0x00, 0x00, 0x00,
 /* KB_Q, KB_R, KB_E, KB_F, KB_ESC, KB_ENTER, KB_LWIN, KB_HASH */
    0xD1, 0xD2, 0xC5, 0xC6, 0x1B, 0x0D, 0x1C, 0xFE,
 /* MOUSE_RIGHT, KB_Z, KB_LCTRL, MOUSE_MIDDLE, MOUSE_LEFT, KB_X, KB_LSHIFT, KB_SPACE */
    0x00, 0xDA, 0x00, 0x00, 0x00, 0xD8, 0x00, 0xA0,

 /* KB_B, KB_C, KB_G, KB_H, KB_I, KB_J, KB_K, KB_L */
    0xC2, 0xC3, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC,
 /* KB_M, KB_N, KB_O, KB_P, KB_T, KB_U, KB_V, KB_Y */
    0xCD, 0xCE, 0xCF, 0xD0, 0xD4, 0xD5, 0xD6, 0xD9,
 /* KB_1, KB_2, KB_3, KB_4, KB_5, KB_6, KB_7, KB_8 */
    0xA1, 0xC0, 0xA3, 0xA4, 0xA5, 0xDE, 0xA6, 0xAA,
 /* KB_9, KB_0, KB_BACKSPACE, KB_TAB, KB_MINUS, KB_EQUAL, KB_LEFTBRACE, KB_RIGHTBRACE */
    0xA8, 0xA9, 0x0F, 0x19, 0xDF, 0xAB, 0xFB, 0xFD,

 /* KB_BACKSLASH, KB_SEMICOLON, KB_APOSTROPHE, KB_GRAVE, KB_COMMA, KB_DOT, KB_SLASH, KB_CAPSLOCK */
    0xFC, 0xBA, 0xA2, 0x00, 0xBC, 0xBE, 0xBF, 0x00,
 /* KB_F1, KB_F2, KB_F3, KB_F4, KB_F5, KB_F6, KB_F7, KB_F8 */
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
 /* KB_F9, KB_F10, KB_F11, KB_F12, KB_PSCREEN, KB_SCROLL, KB_PAUSE, KB_INSERT */
    0x90, 0x91, 0x92, 0x93, 0xC1, 0xC2, 0xC3, 0xD6,
 /* KB_HOME, KB_PAGEUP, KB_DEL, KB_END, KB_PAGEDOWN, KB_NUMLOCK, KB_KP_DIV, KB_KP_MULTI */
    0x1E, 0xD8, 0xD7, 0xD5, 0xD9, 0x00, 0xAF, 0xAA,

 /* KB_KP_MINUS, KB_KP_PLUS, KB_KP_ENTER, KB_KP_1, KB_KP_2, KB_KP_3, KB_KP_4, KB_KP_5 */
    0xAD, 0xAB, 0x0D, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5,
 /* KB_KP_6, KB_KP_7, KB_KP_8, KB_KP_9, KB_KP_0, KB_KP_DOT, KB_LALT, KB_RCTRL */
    0xB6, 0xB7, 0xB8, 0xB9, 0xB0, 0xAE, 0x00, 0x00,
 /* KB_RSHIFT, KB_RALT, KB_RWIN */
    0x00, 0x00, 0x1D,
};

void IRAM_ATTR cdi_init_buffer(int32_t dev_mode, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_KB:
            /* Use acc cfg to choose KB type */
            switch (config.out_cfg[wired_data->index].acc_mode) {
                case ACC_RUMBLE:
                    wired_data->output[0] = 0x40;
                    wired_data->output[1] = 0x10;
                    wired_data->output[2] = 0x00;
                    wired_data->output[3] = 0x00;
                    break;
                default:
                    wired_data->output[0] = 0x80;
                    wired_data->output[1] = 0x00;
                    break;
            }
            break;
        default:
        {
            struct cdi_map *map = (struct cdi_map *)wired_data->output;

            wired_data->output[0] = 0x40;
            wired_data->output[1] = 0x00;
            wired_data->output[2] = 0x00;
            for (uint32_t i = 0; i < 2; i++) {
                map->raw_axes[i] = 0;
                map->relative[i] = 1;
            }
            memset(wired_data->output_mask, 0xFF, sizeof(struct cdi_map));
            break;
        }
    }
}

void cdi_meta_init(struct wired_ctrl *ctrl_data) {
    memset((void *)ctrl_data, 0, sizeof(*ctrl_data)*WIRED_MAX_DEV);

    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        for (uint32_t j = 0; j < ADAPTER_MAX_AXES; j++) {
            switch (config.out_cfg[i].dev_mode) {
                case DEV_KB:
                    ctrl_data[i].mask = cdi_kb_mask;
                    ctrl_data[i].desc = cdi_kb_desc;
                    goto exit_axes_loop;
                case DEV_MOUSE:
                    ctrl_data[i].mask = cdi_mouse_mask;
                    ctrl_data[i].desc = cdi_mouse_desc;
                    ctrl_data[i].axes[j].meta = &cdi_axes_meta[j];
                    break;
                default:
                    ctrl_data[i].mask = cdi_mask;
                    ctrl_data[i].desc = cdi_desc;
                    ctrl_data[i].axes[j].meta = &cdi_axes_meta[j];
                    break;
            }
        }
exit_axes_loop:
        ;
    }
}

void cdi_ctrl_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct cdi_map map_tmp;
    int32_t *raw_axes = (int32_t *)(wired_data->output + 8);

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons |= cdi_btns_mask[i];
                wired_data->cnt_mask[i] = ctrl_data->btns[0].cnt_mask[i];
            }
            else {
                map_tmp.buttons &= ~cdi_btns_mask[i];
                wired_data->cnt_mask[i] = 0;
            }
        }
    }

    for (uint32_t i = 0; i < 2; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & cdi_desc[0])) {
            map_tmp.relative[cdi_axes_idx[i]] = 0;
            raw_axes[cdi_axes_idx[i]] = ctrl_data->axes[i].value;
        }
        wired_data->cnt_mask[axis_to_btn_id(i)] = ctrl_data->axes[i].cnt_mask;
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp) - 8);

#ifdef CONFIG_BLUERETRO_RAW_OUTPUT
    printf("{\"log_type\": \"wired_output\", \"axes\": [%ld, %ld], \"btns\": %d}\n",
        raw_axes[cdi_axes_idx[0]], raw_axes[cdi_axes_idx[1]], map_tmp.buttons);
#endif
}

static void cdi_mouse_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    struct cdi_map map_tmp;
    int32_t *raw_axes = (int32_t *)(wired_data->output + 8);

    memcpy((void *)&map_tmp, wired_data->output, sizeof(map_tmp));

    for (uint32_t i = 0; i < ARRAY_SIZE(generic_btns_mask); i++) {
        if (ctrl_data->map_mask[0] & BIT(i)) {
            if (ctrl_data->btns[0].value & generic_btns_mask[i]) {
                map_tmp.buttons |= cdi_mouse_btns_mask[i];
            }
            else {
                map_tmp.buttons &= ~cdi_mouse_btns_mask[i];
            }
        }
    }

    for (uint32_t i = 2; i < 4; i++) {
        if (ctrl_data->map_mask[0] & (axis_to_btn_mask(i) & cdi_mouse_desc[0])) {
            if (ctrl_data->axes[i].relative) {
                map_tmp.relative[cdi_axes_idx[i]] = 1;
                atomic_add(&raw_axes[cdi_axes_idx[i]], ctrl_data->axes[i].value);
            }
            else {
                map_tmp.relative[cdi_axes_idx[i]] = 0;
                raw_axes[cdi_axes_idx[i]] = ctrl_data->axes[i].value;
            }
        }
    }

    memcpy(wired_data->output, (void *)&map_tmp, sizeof(map_tmp) - 8);
}

static void cdi_kb_from_generic(struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    if (!atomic_test_bit(&wired_data->flags, WIRED_KBMON_INIT)) {
        kbmon_init(ctrl_data->index, cdi_kb_id_to_scancode);
        kbmon_set_typematic(ctrl_data->index, 1, 500000, 90000);
        atomic_set_bit(&wired_data->flags, WIRED_KBMON_INIT);
    }
    kbmon_update(ctrl_data->index, ctrl_data);
}

void cdi_kb_id_to_scancode(uint32_t dev_id, uint8_t type, uint8_t id) {
    if (id < KBM_MAX) {
        uint8_t kb_buf[3] = {0};
        uint8_t scancode, change = 0;
        uint8_t *special = &wired_adapter.data[dev_id].output[4];
        uint8_t char_set = 0x00;

        /* Update status of special keys */
        if (type == KBMON_TYPE_MAKE) {
            switch (id) {
                case KB_LSHIFT:
                case KB_RSHIFT:
                    *special |= CDI_KB_SHIFT;
                    change = 1;
                    break;
                case KB_CAPSLOCK:
                    *special ^= CDI_KB_CAPS;
                    change = 1;
                    break;
                case KB_LALT:
                case KB_RALT:
                    *special |= CDI_KB_ALT;
                    change = 1;
                    break;
                case KB_LCTRL:
                case KB_RCTRL:
                    *special |= CDI_KB_CTRL;
                    change = 1;
                    break;
            }
        }
        else {
            switch (id) {
                case KB_LSHIFT:
                case KB_RSHIFT:
                    *special &= ~CDI_KB_SHIFT;
                    change = 1;
                    break;
                case KB_LALT:
                case KB_RALT:
                    *special &= ~CDI_KB_ALT;
                    change = 1;
                    break;
                case KB_LCTRL:
                case KB_RCTRL:
                    *special &= ~CDI_KB_CTRL;
                    change = 1;
                    break;
            }
        }

        /* Get keycode base on status of special keys */
        switch (*special & ~CDI_KB_CAPS) {
            case CDI_KB_SHIFT:
                scancode = cdi_kb_code_shift[id];
                if ((*special & CDI_KB_CAPS) && ((scancode >= 0x41 && scancode <= 0x5A) || (scancode >= 0x61 && scancode <= 0x7A))) {
                    scancode = cdi_kb_code_normal[id];
                }
                break;
            case CDI_KB_ALT:
                scancode = cdi_kb_code_alt[id];
                break;
            case CDI_KB_CTRL:
                scancode = cdi_kb_code_ctrl[id];
                break;
            case CDI_KB_CTRL | CDI_KB_ALT:
                scancode = cdi_kb_code_ctrl_alt[id];
                break;
            case CDI_KB_SHIFT | CDI_KB_ALT:
                scancode = cdi_kb_code_shift_alt[id];
                break;
            default:
                scancode = cdi_kb_code_normal[id];
                if ((*special & CDI_KB_CAPS) && ((scancode >= 0x41 && scancode <= 0x5A) || (scancode >= 0x61 && scancode <= 0x7A))) {
                    scancode = cdi_kb_code_shift[id];
                }
                break;
        }

        /* Extended keys */
        if ((id >= KB_F9 && id <= KB_INSERT) || (id >= KB_PAGEUP && id <= KB_PAGEDOWN) || (id >= KB_KP_DIV && id <= KB_KP_ENTER)) {
            if (config.out_cfg[dev_id].acc_mode == ACC_MEM) { // If type 'Z' KB
                char_set = 0x01;
            }
            else {
                return;
            }
        }

        if (scancode) {
            switch (type) {
                case KBMON_TYPE_MAKE:
                    kb_buf[0] = *special;
                    kb_buf[1] = char_set;
                    kb_buf[2] = scancode;
                    break;
                case KBMON_TYPE_BREAK:
                    kb_buf[0] = *special;
                    kb_buf[1] = 0x01;
                    kb_buf[2] = 0x00;
                    if (config.out_cfg[dev_id].acc_mode == ACC_RUMBLE) { // If type 'T' KB
                        if (id >= KB_F1 && id <= KB_F8) {
                            kb_buf[2] = scancode;
                        }
                    }
                    break;
            }
            change = 1;
        }
        else if (change) {
            /* Update special keys only */
            kb_buf[0] = *special;
            kb_buf[1] = 0x01;
            kb_buf[2] = 0x00;
        }

        if (change) {
            kbmon_set_code(dev_id, kb_buf, 3);
        }
    }
}

void cdi_from_generic(int32_t dev_mode, struct wired_ctrl *ctrl_data, struct wired_data *wired_data) {
    switch (dev_mode) {
        case DEV_KB:
            cdi_kb_from_generic(ctrl_data, wired_data);
            break;
        case DEV_MOUSE:
            cdi_mouse_from_generic(ctrl_data, wired_data);
            break;
        default:
            cdi_ctrl_from_generic(ctrl_data, wired_data);
            break;
    }
}

void IRAM_ATTR cdi_gen_turbo_mask(struct wired_data *wired_data) {
    struct cdi_map *map_mask = (struct cdi_map *)wired_data->output_mask;

    memset(map_mask, 0xFF, sizeof(*map_mask));

    for (uint32_t i = 0; i < ARRAY_SIZE(cdi_btns_mask); i++) {
        uint8_t mask = wired_data->cnt_mask[i] >> 1;

        if (cdi_btns_mask[i] && mask) {
            if (wired_data->cnt_mask[i] & 1) {
                if (!(mask & wired_data->frame_cnt)) {
                    map_mask->buttons &= ~cdi_btns_mask[i];
                }
            }
            else {
                if (!((mask & wired_data->frame_cnt) == mask)) {
                    map_mask->buttons &= ~cdi_btns_mask[i];
                }
            }
        }
    }

    for (uint32_t i = 0; i < 2; i++) {
        uint8_t btn_id = axis_to_btn_id(i);
        uint8_t mask = wired_data->cnt_mask[btn_id] >> 1;
        if (mask) {
            if (wired_data->cnt_mask[btn_id] & 1) {
                if (!(mask & wired_data->frame_cnt)) {
                    map_mask->raw_axes[cdi_axes_idx[i]] = cdi_axes_meta[i].neutral;
                }
            }
            else {
                if (!((mask & wired_data->frame_cnt) == mask)) {
                    map_mask->raw_axes[cdi_axes_idx[i]] = cdi_axes_meta[i].neutral;
                }
            }
        }
    }
}
