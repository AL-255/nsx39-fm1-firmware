// SPDX-License-Identifier: GPL-3.0-only
// Portable, bounded configuration codec shared by firmware and preview.
#include "instrument.h"
static unsigned register_octave,display_script;
static uint32_t seed_id;
static uint8_t original_lyrics[1024];
static unsigned sr16(const uint8_t*p){return p[0]|(unsigned)p[1]<<8;}
static uint32_t sr32(const uint8_t*p){return sr16(p)|(uint32_t)sr16(p+2)<<16;}
static void sw32(uint8_t*p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(i*8));}
uint32_t state_crc(const uint8_t*p,unsigned n){uint32_t c=~0u;for(unsigned i=0;i<n;i++){c^=i>=12&&i<16?0:p[i];for(unsigned k=0;k<8;k++)c=(c>>1)^((c&1)?0xedb88320u:0);}return ~c;}
static const unsigned limits[18]={2000,2000,100,4000,3,127,127,16383,1,1,127,127,2,1,15,127,2,2};
static const unsigned art_ids[9]={3,4,5,7,1,2,8,10,12};
unsigned instrument_octave(void){return register_octave;}
unsigned instrument_script(void){return display_script;}
void instrument_octave_set(unsigned n){register_octave=n>2?2:n;}
void instrument_script_set(unsigned n){display_script=n%3;}
int instrument_valid(const uint8_t*p,unsigned n){
 if(n!=STATE_BYTES||p[0]!='N'||p[1]!='S'||p[2]!='X'||p[3]!='S'||sr32(p+4)!=1||sr32(p+8)!=STATE_BYTES||sr32(p+12)!=state_crc(p,n))return 0;
 for(unsigned i=24;i<32;i++)if(p[i])return 0;
 for(unsigned i=0;i<32;i++)if(sr16(p+32+i*2)>(i<18?limits[i]:0))return 0;
 for(unsigned b=0;b<16;b++){unsigned k=0;while(k<64&&p[96+b*64+k]<128)k++;if(!k||k==64)return 0;for(;k<64;k++)if(p[96+b*64+k]!=128)return 0;}
 return 1;
}
void instrument_export(uint8_t*p,unsigned seq){
 for(unsigned i=0;i<STATE_BYTES;i++)p[i]=0;p[0]='N';p[1]='S';p[2]='X';p[3]='S';sw32(p+4,1);sw32(p+8,STATE_BYTES);sw32(p+16,seed_id);sw32(p+20,seq);
 nsx_art_debug_t a;nsx_art_debug(&a);unsigned v[18];for(unsigned i=0;i<4;i++)v[i]=a.adsr[i];for(unsigned i=0;i<9;i++)v[4+i]=nsx_articulation_get(art_ids[i]);
 performance_state_t s;performance_snapshot(&s);v[13]=a.hold;v[14]=s.mode;v[15]=performance_manual_value();v[16]=display_script;v[17]=register_octave;
 for(unsigned i=0;i<18;i++){p[32+i*2]=(uint8_t)v[i];p[33+i*2]=(uint8_t)(v[i]>>8);}
 const uint8_t*lyrics=performance_lyrics();for(unsigned i=0;i<1024;i++)p[96+i]=lyrics[i];sw32(p+12,state_crc(p,STATE_BYTES));
}
int instrument_import(const uint8_t*p,unsigned n,int match){
 if(!instrument_valid(p,n)||(match&&sr32(p+16)!=seed_id))return 0;
 performance_set_lyrics(p+96);for(unsigned i=0;i<4;i++)nsx_adsr(i,sr16(p+32+i*2));for(unsigned i=0;i<9;i++)nsx_articulation(art_ids[i],sr16(p+40+i*2));
 nsx_midi(0xb0,64,sr16(p+58)?127:0);performance_state_t current;performance_snapshot(&current);if(current.mode!=sr16(p+60))performance_mode(sr16(p+60));performance_manual(sr16(p+62));display_script=sr16(p+64);register_octave=sr16(p+66);return 1;
}
void instrument_defaults(void){nsx_reset();performance_init(original_lyrics);register_octave=display_script=0;}
int instrument_init(const uint8_t*seed,unsigned n){
 const uint8_t*p=performance_lyrics();for(unsigned i=0;i<1024;i++)original_lyrics[i]=p[i];register_octave=display_script=0;seed_id=state_crc(p,1024);
 if(n){if(!instrument_import(seed,n,0))return 0;seed_id=sr32(seed+12);}return 1;
}
static unsigned item_index(unsigned item){static const unsigned map[]={17,16,14,15,0,1,2,3,8,9,4,5,6,7,10,11,12,13};return item<18?map[item]:0;}
unsigned instrument_value(unsigned item){
 unsigned i=item_index(item);if(i<4){nsx_art_debug_t a;nsx_art_debug(&a);return a.adsr[i];}if(i<13)return nsx_articulation_get(art_ids[i-4]);
 if(i==13){nsx_art_debug_t a;nsx_art_debug(&a);return a.hold;}if(i==14){performance_state_t s;performance_snapshot(&s);return s.mode;}if(i==15)return performance_manual_value();if(i==16)return display_script;return register_octave;
}
void instrument_change(unsigned item,int delta){
 if(item>=18)return;unsigned i=item_index(item);int step=i==0||i==1?10:i==3?20:i==7?25:1;
 int value=(int)instrument_value(item)+delta*step;if(value<0)value=0;if(value>(int)limits[i])value=(int)limits[i];
 if(i<4)nsx_adsr(i,(unsigned)value);else if(i<13)nsx_articulation(art_ids[i-4],(unsigned)value);else if(i==13)nsx_midi(0xb0,64,value?127:0);else if(i==14)performance_mode((unsigned)value);else if(i==15)performance_manual((unsigned)value);else if(i==16)display_script=(unsigned)value;else register_octave=(unsigned)value;
}
