// SPDX-License-Identifier: GPL-3.0-only
// Same native engine, compiled to WebAssembly for listening before installation.
#include "nsx.h"
#include "performance.h"
uint8_t web_bank[NSX_BANK_CAPACITY];
int16_t web_audio[128];
void *memset(void*d,int c,size_t n){uint8_t*p=d;while(n--)*p++=(uint8_t)c;return d;}
void *memcpy(void*d,const void*s,size_t n){uint8_t*p=d;const uint8_t*q=s;while(n--)*p++=*q++;return d;}
unsigned web_bank_ptr(void){return (unsigned)(uintptr_t)web_bank;}
unsigned web_audio_ptr(void){return (unsigned)(uintptr_t)web_audio;}
int web_init(void){int ok=nsx_init(web_bank,sizeof web_bank);performance_init(nsx_factory_lyrics());return ok;}
void web_render(void){nsx_render(web_audio,128);}

unsigned web_last(void){performance_state_t s;performance_snapshot(&s);return s.has_last?s.last:128;}
unsigned web_next(void){performance_state_t s;performance_snapshot(&s);return s.next;}
unsigned web_position(void){performance_state_t s;performance_snapshot(&s);return s.cursor;}
