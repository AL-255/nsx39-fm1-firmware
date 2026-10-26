// SPDX-License-Identifier: GPL-3.0-only
// Same native engine, compiled to WebAssembly for listening before installation.
#include "nsx.h"
uint8_t web_bank[NSX_BANK_CAPACITY];
int16_t web_audio[128];
void *memset(void*d,int c,size_t n){uint8_t*p=d;while(n--)*p++=(uint8_t)c;return d;}
void *memcpy(void*d,const void*s,size_t n){uint8_t*p=d;const uint8_t*q=s;while(n--)*p++=*q++;return d;}
unsigned web_bank_ptr(void){return (unsigned)(uintptr_t)web_bank;}
unsigned web_audio_ptr(void){return (unsigned)(uintptr_t)web_audio;}
int web_init(void){return nsx_init(web_bank,sizeof web_bank);}
void web_render(void){nsx_render(web_audio,128);}
