// SPDX-License-Identifier: GPL-3.0-only
#ifndef NSX_H
#define NSX_H
#ifdef __wasm__
typedef __UINT8_TYPE__ uint8_t;
typedef __INT8_TYPE__ int8_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __INT16_TYPE__ int16_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __INT32_TYPE__ int32_t;
typedef __UINTPTR_TYPE__ uintptr_t;
typedef __SIZE_TYPE__ size_t;
#else
#include <stdint.h>
#include <stddef.h>
#endif
#define NSX_RATE 22050u
#define NSX_BANK_CAPACITY 0x78000u
#define NSX_MAX_UNIT 8192u
#define NSX_LYRIC_BYTES 1024u
#define NSX_LYRIC_SLOTS 128u
#define NSX_MAX_SYLLABLES 128u
int nsx_init(const uint8_t *bank, size_t size);
void nsx_reset(void);
void nsx_midi(uint8_t status, uint8_t a, uint8_t b);
int nsx_sysex(const uint8_t *data, size_t size); // excludes F0/F7
void nsx_note_on(uint8_t note, uint8_t velocity);
void nsx_note_off(uint8_t note);
void nsx_render(int16_t *out, unsigned count);
void nsx_select(unsigned index);
void nsx_solge(unsigned enabled);
typedef struct {unsigned selected,note,velocity,active,unit,source,target,steady;} nsx_debug_t;
void nsx_debug(nsx_debug_t *out);
const char *nsx_phoneme(unsigned id);
void nsx_sequence(unsigned enabled);
unsigned nsx_selected(void);
unsigned nsx_active(void);
const char *nsx_pronunciation(unsigned index);
void nsx_decode_ima(const uint8_t block[68], int16_t out[128]);
#endif
