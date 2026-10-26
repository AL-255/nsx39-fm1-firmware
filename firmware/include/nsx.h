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
typedef __INT64_TYPE__ int64_t;
typedef __UINT64_TYPE__ uint64_t;
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
const uint8_t *nsx_factory_lyrics(void);
void nsx_midi(uint8_t status, uint8_t a, uint8_t b);
int nsx_sysex(const uint8_t *data, size_t size); // excludes F0/F7
void nsx_note_on(uint8_t note, uint8_t velocity);
void nsx_note_off(uint8_t note);
void nsx_revoice(uint8_t note,uint8_t velocity);
void nsx_render(int16_t *out, unsigned count);
void nsx_select(unsigned index);
void nsx_solge(unsigned enabled);
typedef struct {unsigned selected,note,velocity,active,unit,source,target,steady;} nsx_debug_t;
enum {NSX_ART_AUTO_PITCH=1,NSX_ART_AUTO_DYNAMICS=2,NSX_ART_VIBRATO_TYPE=3,
 NSX_ART_VIBRATO_RATE=4,NSX_ART_VIBRATO_DEPTH=5,NSX_ART_VIBRATO_DELAY=7,
 NSX_ART_PORTAMENTO=8,NSX_ART_NOISE=10,NSX_ART_CONNECTION=12};
enum {NSX_ENV_IDLE,NSX_ENV_ATTACK,NSX_ENV_DECAY,NSX_ENV_SUSTAIN,NSX_ENV_RELEASE};
typedef struct {unsigned adsr[4],stage,level,progress,vibrato_type,vibrato_rate,vibrato_depth,vibrato_delay;
 unsigned auto_pitch,auto_dynamics,portamento,noise,connection,hold,release_unit;int pitch_cents;} nsx_art_debug_t;
void nsx_articulation(unsigned parameter,unsigned value);
unsigned nsx_articulation_get(unsigned parameter);
void nsx_adsr(unsigned parameter,unsigned value); // A/D/R milliseconds; S 0..100 percent.
void nsx_adsr_knob(unsigned parameter,int delta);
void nsx_art_debug(nsx_art_debug_t *out);
void nsx_panel_button(unsigned button);
int32_t nsx_vibrato_q16(void);
void nsx_debug(nsx_debug_t *out);
const char *nsx_phoneme(unsigned id);
void nsx_sequence(unsigned enabled);
unsigned nsx_selected(void);
unsigned nsx_active(void);
const char *nsx_pronunciation(unsigned index);
void nsx_decode_ima(const uint8_t block[68], int16_t out[128]);
#endif
