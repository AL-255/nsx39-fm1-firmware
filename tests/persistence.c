// SPDX-License-Identifier: GPL-3.0-only
// Synthetic flash only: cuts at every program byte, rollover erase cuts,
// unknown-data protection, duplicate saves and seed precedence. No hardware I/O.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../firmware/src/voice.c"
#include "../firmware/src/performance.c"
#include "../firmware/src/instrument.c"
#include "../firmware/src/persistence.c"
static uint8_t flash[0x100000],baseline[0x100000];
static int budget=-1,programs,erases,fail_commit_read;
static int save_read(uint32_t off,void*p,unsigned n){assert(off>=SAVE_LO&&n<=SAVE_HI-off);if(fail_commit_read&&n==4&&!memcmp(flash+off,"NSXC",4)){fail_commit_read=0;return -1;}memcpy(p,flash+off,n);return 0;}
static int save_write(uint32_t off,const void*p,unsigned n){assert(off>=SAVE_LO&&n<=SAVE_HI-off);const uint8_t*s=p;programs++;for(unsigned i=0;i<n;i++){if(!budget)return -1;if(budget>0)budget--;assert((flash[off+i]&s[i])==s[i]);flash[off+i]&=s[i];}return 0;}
static int save_erase(uint32_t off){assert(off>=SAVE_LO&&off+4096<=SAVE_HI&&!(off&4095));erases++;for(unsigned i=0;i<4096;i++){if(!budget)return -1;if(budget>0)budget--;flash[off+i]=255;}return 0;}
static void boot(void){nsx_reset();performance_init(0);assert(instrument_init(0,0));persistence_load();}
static uint32_t fm1_ms;static unsigned updating;static struct{unsigned notes;}fm1_in;
static uint8_t wire[192];static unsigned wire_n;
static unsigned ota_pack7(const uint8_t*in,unsigned n,uint8_t*out){unsigned acc=0,bits=0,k=0;while(n--){acc|=(unsigned)*in++<<bits;bits+=8;while(bits>=7){out[k++]=acc&127;acc>>=7;bits-=7;}}if(bits)out[k++]=acc&127;return k;}
static unsigned ota_unpack7(const uint8_t*in,unsigned n,uint8_t*out,unsigned max){unsigned acc=0,bits=0,k=0;while(n--){acc|=(unsigned)*in++<<bits;bits+=7;while(bits>=8){if(k<max)out[k]=acc&255;k++;acc>>=8;bits-=8;}}return k;}
static int ota_wire_send(const uint8_t*p,unsigned n){assert(n<=sizeof wire);memcpy(wire,p,n);wire_n=n;return 0;}
static int settings_flush(void){return persistence_flush();}
#define MEMORY_LOCK() ((void)0)
#define MEMORY_UNLOCK() ((void)0)
#include "../firmware/src/memory_midi.c"
static void command(unsigned cmd,const uint8_t*data,unsigned len){uint8_t p[192]={125,78,83,88,1,0,7};p[5]=cmd;if(len)memcpy(p+7,data,len);unsigned sum=0;for(unsigned i=0;i<7+len;i++)sum+=p[i];p[7+len]=(uint8_t)(-sum&127);wire_n=0;assert(memory_sysex(p,len+8));assert(wire_n>=10);assert(wire[6]==(cmd|64)&&wire[7]==7);}
static void stage(const uint8_t*state){for(unsigned i=0;i<9;i++){uint8_t data[150];data[0]=i;unsigned n=STATE_BYTES-i*128;if(n>128)n=128;n=ota_pack7(state+i*128,n,data+1);command(2,data,n+1);assert(wire[8]==0);}}
int main(void){
 for(int cut=0;cut<=1124;cut++){
  memset(flash,255,sizeof flash);budget=-1;boot();instrument_octave_set(1);budget=cut;int result=persistence_flush();budget=-1;boot();assert(instrument_octave()==(result?0u:1u));instrument_octave_set(2);assert(!persistence_flush());boot();assert(instrument_octave()==2);
 }
 memset(flash,255,sizeof flash);boot();assert(instrument_octave()==0);assert(!persistence_flush());unsigned count=programs;assert(!persistence_flush());assert(programs==(int)count);
 instrument_octave_set(1);assert(!persistence_flush());instrument_octave_set(2);assert(!persistence_flush());memcpy(baseline,flash,sizeof flash);
 for(int cut=0;cut<=1140;cut++){
  memcpy(flash,baseline,sizeof flash);budget=-1;boot();assert(instrument_octave()==2);instrument_octave_set(1);budget=cut;int result=persistence_flush();budget=-1;boot();assert(instrument_octave()==(result?2u:1u));
  assert(!memcmp(flash,baseline,SAVE_LO));assert(!memcmp(flash+SAVE_HI,baseline+SAVE_HI,sizeof flash-SAVE_HI));
  unsigned restored=instrument_octave();instrument_octave_set(restored?0:1);assert(!persistence_flush());boot();assert(instrument_octave()==(restored?0u:1u));
 }
 memcpy(flash,baseline,sizeof flash);budget=-1;boot();for(unsigned i=0;i<3;i++){instrument_octave_set(i%2);assert(!persistence_flush());}assert((save_address&~4095u)==SAVE_LO+4096);memcpy(baseline,flash,sizeof flash);unsigned old=instrument_octave();
 for(int cut=0;cut<16+4096+1124;cut+=17){memcpy(flash,baseline,sizeof flash);budget=-1;boot();instrument_octave_set(old?0:1);budget=cut;int result=persistence_flush();budget=-1;boot();assert(instrument_octave()==(result?old:(old?0u:1u)));unsigned restored=instrument_octave();instrument_octave_set(restored?0:1);assert(!persistence_flush());boot();assert(instrument_octave()==(restored?0u:1u));}
 memset(flash,255,sizeof flash);memset(flash+SAVE_LO,0x5a,SAVE_HI-SAVE_LO);budget=-1;boot();count=programs;unsigned e=erases;assert(persistence_flush()==-2);assert(programs==(int)count&&erases==(int)e);for(unsigned i=SAVE_LO;i<SAVE_HI;i++)assert(flash[i]==0x5a);
 memset(flash,255,sizeof flash);boot();uint8_t future[STATE_BYTES];instrument_export(future,0);future[4]=2;memcpy(flash+SAVE_LO,future,sizeof future);memcpy(flash+SAVE_LO+4096,future,sizeof future);boot();count=programs;e=erases;assert(persistence_flush()==-2);assert(programs==(int)count&&erases==(int)e);
 memset(flash,255,sizeof flash);boot();instrument_octave_set(2);assert(!persistence_flush());uint8_t seed[STATE_BYTES];instrument_defaults();nsx_adsr(0,250);instrument_export(seed,0);assert(instrument_init(seed,sizeof seed));persistence_load();assert(instrument_value(SET_ATTACK)==250);assert(instrument_octave()==0);assert(!persistence_flush());instrument_defaults();assert(instrument_init(seed,sizeof seed));persistence_load();assert(instrument_value(SET_ATTACK)==250);
 instrument_export(seed,0);seed[40]^=1;assert(!instrument_import(seed,sizeof seed,0));
 // Exercise the actual firmware SysEx implementation, not just a JS peer.
 memset(flash,255,sizeof flash);boot();uint8_t desired[STATE_BYTES],actual[STATE_BYTES];instrument_octave_set(1);instrument_export(desired,0);instrument_octave_set(0);
 for(unsigned i=0;i<9;i++){uint8_t chunk=i;command(1,&chunk,1);assert(wire[8]==i);unsigned n=STATE_BYTES-i*128;if(n>128)n=128;assert(ota_unpack7(wire+9,wire_n-11,actual+i*128,n)==n);}assert(instrument_valid(actual,sizeof actual));
 stage(desired);fm1_in.notes=1;command(3,0,0);assert(wire[8]==3&&instrument_octave()==0);fm1_in.notes=0;
 stage(desired);command(3,0,0);assert(wire[8]==0&&instrument_octave()==1);boot();assert(instrument_octave()==1);
 desired[100]^=1;stage(desired);command(3,0,0);assert(wire[8]==2);boot();assert(instrument_octave()==1);
 memset(flash,255,sizeof flash);boot();assert(!persistence_flush());instrument_octave_set(1);fail_commit_read=1;assert(persistence_flush()==-1);instrument_octave_set(2);assert(!persistence_flush());boot();assert(instrument_octave()==2);
 memset(flash,255,sizeof flash);boot();assert(!persistence_flush());instrument_octave_set(1);fail_commit_read=1;assert(persistence_flush()==-1);instrument_octave_set(0);assert(!persistence_flush());boot();assert(instrument_octave()==0);
 puts("Journal passed: every program cut, erase rollover cuts, foreign storage, deduplication, seed precedence and CRC rejection");return 0;
}
