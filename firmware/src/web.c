// SPDX-License-Identifier: GPL-3.0-only
// Same native engine, compiled to WebAssembly for listening before installation.
#include "nsx.h"
#include "performance.h"
#include "instrument.h"
uint8_t web_bank[NSX_BANK_CAPACITY];
int16_t web_audio[128];
void *memset(void*d,int c,size_t n){uint8_t*p=d;while(n--)*p++=(uint8_t)c;return d;}
void *memcpy(void*d,const void*s,size_t n){uint8_t*p=d;const uint8_t*q=s;while(n--)*p++=*q++;return d;}
unsigned web_bank_ptr(void){return (unsigned)(uintptr_t)web_bank;}
unsigned web_audio_ptr(void){return (unsigned)(uintptr_t)web_audio;}
int web_init(void){int ok=nsx_init(web_bank,sizeof web_bank);performance_init(nsx_factory_lyrics());unsigned n;const uint8_t*s=nsx_seed_state(&n);return ok&&instrument_init(s,n);}
void web_render(void){nsx_render(web_audio,128);}

unsigned web_last(void){performance_state_t s;performance_snapshot(&s);return s.has_last?s.last:128;}
unsigned web_next(void){performance_state_t s;performance_snapshot(&s);return s.next;}
unsigned web_position(void){performance_state_t s;performance_snapshot(&s);return s.cursor;}
static uint8_t web_state[STATE_BYTES];
unsigned web_state_ptr(void){instrument_export(web_state,0);return (unsigned)(uintptr_t)web_state;}
int web_state_apply(void){return instrument_import(web_state,STATE_BYTES,0);}

static nsx_art_debug_t web_art;
unsigned web_art_ptr(void){nsx_art_debug(&web_art);return (unsigned)(uintptr_t)&web_art;}
