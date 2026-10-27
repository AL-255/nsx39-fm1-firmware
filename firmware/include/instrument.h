// SPDX-License-Identifier: GPL-3.0-only
#ifndef INSTRUMENT_H
#define INSTRUMENT_H
#include "performance.h"
#define STATE_BYTES 1120u
#define STATE_VALUES 18u
enum {SET_OCTAVE,SET_SCRIPT,SET_LYRIC,SET_SYLLABLE,SET_ATTACK,SET_DECAY,SET_SUSTAIN,SET_RELEASE,
 SET_PITCH,SET_DYNAMICS,SET_VIBTYPE,SET_VIBRATE,SET_VIBDEPTH,SET_VIBDELAY,SET_GLIDE,SET_NOISE,SET_JOIN,SET_HOLD,SET_RESET,SET_COUNT};
unsigned instrument_octave(void);
unsigned instrument_script(void);
void instrument_octave_set(unsigned value);
void instrument_script_set(unsigned value);
int instrument_init(const uint8_t *seed,unsigned size);
int instrument_valid(const uint8_t *data,unsigned size);
int instrument_import(const uint8_t *data,unsigned size,int match_seed);
void instrument_export(uint8_t *data,unsigned sequence);
void instrument_defaults(void);
unsigned instrument_value(unsigned item);
void instrument_change(unsigned item,int delta);
uint32_t state_crc(const uint8_t *data,unsigned n);
#endif
