// SPDX-License-Identifier: GPL-3.0-only
// Synthetic flash only: cuts at every program byte, rollover erase cuts,
// unknown-data protection, duplicate saves and firmware-seed independence. No hardware I/O.
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
static int settings_initialize(uint32_t a,uint32_t b){return persistence_initialize(a,b);}
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
 memset(flash,255,sizeof flash);boot();uint8_t future[STATE_BYTES];instrument_export(future,0);future[4]=3;memcpy(flash+SAVE_LO,future,sizeof future);memcpy(flash+SAVE_LO+4096,future,sizeof future);boot();count=programs;e=erases;assert(persistence_flush()==-2);assert(programs==(int)count&&erases==(int)e);
 memset(flash,255,sizeof flash);boot();instrument_octave_set(2);assert(!persistence_flush());uint8_t seed[STATE_BYTES];instrument_defaults();nsx_adsr(0,250);instrument_export(seed,0);assert(instrument_init(seed,sizeof seed));persistence_load();assert(instrument_value(SET_ATTACK)==0);assert(instrument_octave()==2);assert(!persistence_flush());instrument_defaults();assert(instrument_init(seed,sizeof seed));persistence_load();assert(instrument_value(SET_ATTACK)==0);assert(instrument_octave()==2);
 instrument_export(seed,0);seed[40]^=1;assert(!instrument_import(seed,sizeof seed,0));
 // Migrate a committed v1 selection to v2 exactly once, including journal ownership.
 memset(flash,255,sizeof flash);boot();instrument_export(seed,7);seed[4]=1;seed[60]=6;sw32(seed+12,state_crc(seed,sizeof seed));memcpy(flash+SAVE_LO,seed,sizeof seed);memcpy(flash+SAVE_LO+STATE_BYTES,"NSXC",4);boot();assert(instrument_value(SET_LYRIC)==8);assert(!persistence_flush());boot();assert(instrument_value(SET_LYRIC)==8);instrument_export(seed,0);assert(seed[4]==2);
 // Exercise the actual firmware SysEx implementation, not just a JS peer.
 memset(flash,255,sizeof flash);boot();uint8_t desired[STATE_BYTES],actual[STATE_BYTES];instrument_octave_set(1);instrument_export(desired,0);instrument_octave_set(0);
 for(unsigned i=0;i<9;i++){uint8_t chunk=i;command(1,&chunk,1);assert(wire[8]==i);unsigned n=STATE_BYTES-i*128;if(n>128)n=128;assert(ota_unpack7(wire+9,wire_n-11,actual+i*128,n)==n);}assert(instrument_valid(actual,sizeof actual));
 stage(desired);fm1_in.notes=1;command(3,0,0);assert(wire[8]==3&&instrument_octave()==0);fm1_in.notes=0;
 stage(desired);command(3,0,0);assert(wire[8]==0&&instrument_octave()==1);boot();assert(instrument_octave()==1);
 desired[100]^=1;stage(desired);command(3,0,0);assert(wire[8]==2);boot();assert(instrument_octave()==1);
 memset(flash,255,sizeof flash);boot();assert(!persistence_flush());instrument_octave_set(1);fail_commit_read=1;assert(persistence_flush()==-1);instrument_octave_set(2);assert(!persistence_flush());boot();assert(instrument_octave()==2);
 memset(flash,255,sizeof flash);boot();assert(!persistence_flush());instrument_octave_set(1);fail_commit_read=1;assert(persistence_flush()==-1);instrument_octave_set(0);assert(!persistence_flush());boot();assert(instrument_octave()==0);
 // Explicit initialization needs matching complete-backup and state checksums.
 memset(flash,255,sizeof flash);memset(flash+SAVE_LO,0x5a,SAVE_HI-SAVE_LO);boot();uint32_t backup_crc;assert(!persistence_storage_crc(&backup_crc));instrument_export(seed,0);uint32_t config_crc=save_u32(seed+12);memcpy(baseline,flash,sizeof flash);
 count=programs;e=erases;assert(persistence_initialize(backup_crc^1,config_crc)==-4);assert(persistence_initialize(backup_crc,config_crc^1)==-4);assert(programs==(int)count&&erases==(int)e&&!memcmp(flash,baseline,sizeof flash));
 assert(!persistence_initialize(backup_crc,config_crc));assert(save_have);boot();assert(instrument_octave()==0);instrument_octave_set(2);assert(!persistence_flush());boot();assert(instrument_octave()==2);for(unsigned i=0;i<20;i++){instrument_octave_set(i%3);assert(!persistence_flush());boot();assert(instrument_octave()==i%3);}
 // Every sampled initialization cut keeps the newest committed state, and an
 // explicit fresh-backup initialization can recover each interrupted phase.
 for(int cut=0;cut<4096+1124+16+4096;cut+=17){
  memset(flash,255,sizeof flash);budget=-1;boot();instrument_octave_set(2);assert(!persistence_flush());memset(flash+SAVE_LO+4096,0x5a,4096);assert(!persistence_storage_crc(&backup_crc));instrument_export(seed,0);config_crc=save_u32(seed+12);
  budget=cut;(void)persistence_initialize(backup_crc,config_crc);budget=-1;boot();assert(instrument_octave()==2);assert(!persistence_storage_crc(&backup_crc));instrument_export(seed,0);assert(!persistence_initialize(backup_crc,save_u32(seed+12)));boot();assert(instrument_octave()==2);
  assert(!memcmp(flash,baseline,SAVE_LO));assert(!memcmp(flash+SAVE_HI,baseline+SAVE_HI,sizeof flash-SAVE_HI));
 }
 // Initialization must preserve a newer committed copy whose readback failed
 // during the prior save, not just the last copy tracked in volatile metadata.
 memset(flash,255,sizeof flash);boot();assert(!persistence_flush());instrument_octave_set(1);assert(!persistence_flush());instrument_octave_set(2);assert(!persistence_flush());instrument_octave_set(1);fail_commit_read=1;assert(persistence_flush()==-1);assert(!persistence_storage_crc(&backup_crc));instrument_export(seed,0);budget=17;assert(persistence_initialize(backup_crc,save_u32(seed+12))==-1);budget=-1;boot();assert(instrument_octave()==1);
 // Protocol backup reads are fixed to the journal; stale proof and busy keys
 // refuse initialization without erasing, before an acknowledged valid request.
 memset(flash,255,sizeof flash);memset(flash+SAVE_LO,0x5a,SAVE_HI-SAVE_LO);boot();uint8_t chunk=63;command(4,&chunk,1);assert(wire[8]==63);assert(ota_unpack7(wire+9,wire_n-11,actual,128)==128);for(unsigned i=0;i<128;i++)assert(actual[i]==0x5a);
 assert(!persistence_storage_crc(&backup_crc));instrument_export(seed,0);uint8_t proof[12],packed[14];sw32(proof,backup_crc);sw32(proof+4,save_u32(seed+12));sw32(proof+8,0x54494e49u);assert(ota_pack7(proof,12,packed)==14);
 e=erases;fm1_in.notes=1;uint8_t error[2]={127,3};command(4,&chunk,1);assert(!memcmp(wire+8,error,2));command(5,packed,14);assert(wire[8]==3&&erases==(int)e);fm1_in.notes=0;
 proof[0]^=1;ota_pack7(proof,12,packed);command(5,packed,14);assert(wire[8]==7&&erases==(int)e);proof[0]^=1;ota_pack7(proof,12,packed);command(5,packed,14);assert(wire[8]==0&&erases==(int)e+2);boot();assert(save_have);
 puts("Journal passed: every program cut, erase rollover cuts, foreign storage, deduplication, firmware-seed independence and CRC rejection");return 0;
}
