// SPDX-License-Identifier: GPL-3.0-only
#ifndef PERFORMANCE_H
#define PERFORMANCE_H
#include "nsx.h"
typedef struct {unsigned mode,cursor,length,next,last,has_last;} performance_state_t;
// Input has already passed nsx_init's bounds, CRC and marker validation.
void performance_init(const uint8_t *factory);
void performance_mode(unsigned mode);
void performance_rotate(int delta);
void performance_select(int delta);
void performance_trigger(uint8_t note,uint8_t velocity);
void performance_revoice(uint8_t note,uint8_t velocity);
void performance_midi(uint8_t status,uint8_t a,uint8_t b);
int performance_sysex(const uint8_t *data,size_t size);
void performance_snapshot(performance_state_t *out);
void performance_manual(unsigned syllable);
unsigned performance_upcoming(unsigned offset);
const uint8_t *performance_lyrics(void);
int performance_set_lyrics(const uint8_t *table);
unsigned performance_manual_value(void);
#endif
