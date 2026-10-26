// SPDX-License-Identifier: GPL-3.0-only
// Musical panel/preview state. Never writes flash and never changes voice data.
#include "performance.h"
static const uint8_t *factory;
static unsigned lengths[16],mode,cursor,manual,last,has_last;
void performance_init(const uint8_t *data){
 factory=data;mode=cursor=manual=last=has_last=0;
 for(unsigned b=0;b<16;b++){lengths[b]=0;if(data)while(lengths[b]<64&&data[b*64+lengths[b]]<128)lengths[b]++;}
}
void performance_mode(unsigned m){mode=factory?(m&15u):0;cursor=0;if(!mode)nsx_select(manual);}
void performance_rotate(int delta){performance_mode((unsigned)((int)mode+delta));}
void performance_manual(unsigned syllable){manual=syllable&127u;if(!mode)nsx_select(manual);}
void performance_select(int delta){
 if(!mode)performance_manual((unsigned)((int)manual+delta));
 else if(delta<0)cursor=0; // Right is intentionally reserved; no retrigger.
}
static unsigned upcoming(void){return mode?factory[mode*64+cursor]:manual;}
void performance_trigger(uint8_t note,uint8_t velocity){
 if(!velocity){nsx_note_off(note);return;}
 unsigned syllable=upcoming();nsx_sequence(0);nsx_select(syllable);nsx_note_on(note,velocity);
 last=syllable;has_last=1;if(mode&&++cursor>=lengths[mode])cursor=0;
}
void performance_revoice(uint8_t note,uint8_t velocity){
 if(!has_last){performance_trigger(note,velocity);return;}
 nsx_sequence(0);nsx_select(last);nsx_revoice(note,velocity);if(!mode)nsx_select(manual);
}
void performance_midi(uint8_t st,uint8_t a,uint8_t b){
 if(st==0x90){performance_trigger(a,b);return;}
 nsx_midi(st,a,b);
 if((st&15)==0&&!mode)manual=nsx_selected();
}
void performance_snapshot(performance_state_t*out){out->mode=mode;out->cursor=cursor;out->length=mode?lengths[mode]:1;out->next=upcoming();out->last=last;out->has_last=has_last;}

int performance_sysex(const uint8_t*data,size_t size){int result=nsx_sysex(data,size);if(result>0&&!mode)manual=nsx_selected();return result;}
