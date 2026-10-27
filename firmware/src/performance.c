// SPDX-License-Identifier: GPL-3.0-only
// Musical panel/preview state. Never writes flash and never changes voice data.
#include "performance.h"
static uint8_t memories[1024];
static const uint8_t *factory;
static unsigned lengths[16],mode,cursor,manual,last,has_last,solfege_note;
static unsigned solfege_id(unsigned note){static const uint8_t degrees[12]={0,0,1,1,2,3,3,4,4,5,5,6},ids[7]={50,114,101,95,25,111,32};return ids[degrees[note%12]];}
void performance_init(const uint8_t *data){
 mode=cursor=manual=last=has_last=0;solfege_note=60;
 for(unsigned i=0;i<1024;i++)memories[i]=data?data[i]:((i%64)?128:0);
 factory=memories;
 for(unsigned b=0;b<16;b++){lengths[b]=0;while(lengths[b]<64&&memories[b*64+lengths[b]]<128)lengths[b]++;}
}
void performance_mode(unsigned m){mode=factory?m%18u:0;cursor=0;if(!mode)nsx_select(manual);}
void performance_rotate(int delta){int next=(int)mode+delta%18;if(next<0)next+=18;performance_mode((unsigned)next);}
void performance_manual(unsigned syllable){manual=syllable&127u;if(!mode)nsx_select(manual);}
void performance_select(int delta){
 if(!mode)performance_manual((unsigned)((int)manual+delta));
 else if(mode>=2&&delta<0)cursor=0; // Right is intentionally reserved; no retrigger.
}
static unsigned upcoming(void){return mode>=2?factory[(mode-2)*64+cursor]:mode==1?solfege_id(solfege_note):manual;}
unsigned performance_upcoming(unsigned offset){return mode>=2?factory[(mode-2)*64+(cursor+offset)%lengths[mode-2]]:upcoming();}
const uint8_t *performance_lyrics(void){return memories;}
unsigned performance_manual_value(void){return manual;}
int performance_set_lyrics(const uint8_t *table){
 for(unsigned b=0;b<16;b++){unsigned n=0;while(n<64&&table[b*64+n]<128)n++;if(!n||n==64)return 0;for(unsigned j=n;j<64;j++)if(table[b*64+j]!=128)return 0;}
 unsigned changed=0;if(mode>=2)for(unsigned i=0;i<64;i++)if(memories[(mode-2)*64+i]!=table[(mode-2)*64+i])changed=1;
 for(unsigned i=0;i<1024;i++)memories[i]=table[i];
 for(unsigned b=0;b<16;b++){lengths[b]=0;while(memories[b*64+lengths[b]]<128)lengths[b]++;}
 if(changed)cursor=0;return 1;
}
void performance_trigger(uint8_t note,uint8_t velocity){
 if(!velocity){nsx_note_off(note);return;}
 solfege_note=note;unsigned syllable=upcoming();nsx_sequence(0);nsx_select(syllable);nsx_note_on(note,velocity);
 last=syllable;has_last=1;if(mode>=2&&++cursor>=lengths[mode-2])cursor=0;
}
void performance_revoice(uint8_t note,uint8_t velocity){
 if(mode==1||!has_last){performance_trigger(note,velocity);return;}
 nsx_sequence(0);nsx_select(last);nsx_revoice(note,velocity);if(!mode)nsx_select(manual);
}
void performance_midi(uint8_t st,uint8_t a,uint8_t b){
 if(st==0x90){performance_trigger(a,b);return;}
 nsx_midi(st,a,b);
 if((st&15)==0&&!mode)manual=nsx_selected();
}
void performance_snapshot(performance_state_t*out){out->mode=mode;out->cursor=cursor;out->length=mode>=2?lengths[mode-2]:1;out->next=upcoming();out->last=last;out->has_last=has_last;}

int performance_sysex(const uint8_t*data,size_t size){int result=nsx_sysex(data,size);if(result>0&&!mode)manual=nsx_selected();return result;}
