// SPDX-License-Identifier: GPL-3.0-only
// A/B journal in the documented autosave area, outside OTA staging. Never
// erase the newest committed sector or claim an unfamiliar storage format.
#include "instrument.h"
#define SAVE_LO 0xe5000u
#define SAVE_HI 0xe7000u
#define SAVE_STRIDE 0x480u
#define SAVE_INTENT 0xe00u
#define SAVE_COMMIT 0x4358534eu
static int save_read(uint32_t off,void*data,unsigned n);
static int save_write(uint32_t off,const void*data,unsigned n);
static int save_erase(uint32_t off);
static uint8_t save_record[STATE_BYTES],save_previous[STATE_BYTES],save_verify[256];
static uint32_t save_sequence,save_attempt_sequence,save_address;
static int save_have,save_status,save_uncertain;
static int save_equal(const uint8_t*a,const uint8_t*b,unsigned n){for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
static uint32_t save_u32(const uint8_t*p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int save_empty(uint32_t off,unsigned n){while(n){unsigned k=n>256?256:n;if(save_read(off,save_verify,k))return 0;for(unsigned i=0;i<k;i++)if(save_verify[i]!=255)return 0;off+=k;n-=k;}return 1;}
static int save_intent(uint32_t sector,uint32_t target,int current){
 if(save_read(sector+SAVE_INTENT,save_verify,16)||save_u32(save_verify)!=0x4958534eu||save_u32(save_verify+12)!=state_crc(save_verify,16))return 0;
 unsigned to=save_u32(save_verify+4);if((to!=SAVE_LO&&to!=SAVE_LO+4096)||to==sector)return 0;
 return !current||(to==target&&save_u32(save_verify+8)==save_sequence);
}
static int save_owned(uint32_t sector){
 static const uint8_t head[12]={'N','S','X','S',2,0,0,0,0x60,4,0,0},old_head[12]={'N','S','X','S',1,0,0,0,0x60,4,0,0};
 for(unsigned slot=0;slot<3;slot++){
  unsigned off=sector+slot*SAVE_STRIDE;
  if(!save_empty(off,STATE_BYTES+4)){
   if(save_read(off,save_verify,12))return 0;
   if(!save_equal(save_verify,head,12)&&!save_equal(save_verify,old_head,12)){
    // A sequentially interrupted first header may precede any committed
    // record. Accept only an exact known prefix followed entirely by erase.
    unsigned k=0,j=0;while(k<12&&save_verify[k]==head[k])k++;while(j<12&&save_verify[j]==old_head[j])j++;if(j>k)k=j;
    if(!k||k==12||!save_empty(off+k,STATE_BYTES+4-k))return 0;
   }
  }
  if(!save_empty(off+STATE_BYTES+4,SAVE_STRIDE-STATE_BYTES-4))return 0;
 }
 if(!save_empty(sector+3*SAVE_STRIDE,SAVE_INTENT-3*SAVE_STRIDE))return 0;
 if(!save_empty(sector+SAVE_INTENT,16)&&!save_intent(sector,0,0))return 0;
 return save_empty(sector+SAVE_INTENT+16,4096-SAVE_INTENT-16);
}
static void persistence_load(void){
 save_have=save_status=save_uncertain=0;save_sequence=save_attempt_sequence=save_address=0;
 for(unsigned sector=SAVE_LO;sector<SAVE_HI;sector+=4096)for(unsigned slot=0;slot<3;slot++){
  unsigned off=sector+slot*SAVE_STRIDE;
  if(save_read(off,save_record,STATE_BYTES)||save_read(off+STATE_BYTES,save_verify,4)||save_u32(save_verify)!=SAVE_COMMIT||!instrument_valid(save_record,STATE_BYTES))continue;
  unsigned sequence=save_u32(save_record+20);
  if(!save_have||(int32_t)(sequence-save_sequence)>0){save_have=1;save_sequence=sequence;save_address=off;for(unsigned i=0;i<STATE_BYTES;i++)save_previous[i]=save_record[i];}
 }
 save_attempt_sequence=save_sequence;
 // Device memories/settings are independent of the package's initial seed.
 // Firmware updates must not replace an existing committed configuration.
 if(save_have)instrument_import(save_previous,STATE_BYTES,0);
}
static int save_commit_record(uint32_t target,unsigned next){
 if(save_write(target,save_record,STATE_BYTES)){save_status=-1;return -1;}
 for(unsigned i=0;i<STATE_BYTES;i+=256){unsigned n=STATE_BYTES-i;if(n>256)n=256;if(save_read(target+i,save_verify,n)||!save_equal(save_verify,save_record+i,n)){save_status=-1;return -1;}}
 uint8_t commit[4]={'N','S','X','C'};
 save_uncertain=1;
 if(save_write(target+STATE_BYTES,commit,4)||save_read(target+STATE_BYTES,save_verify,4)||save_u32(save_verify)!=SAVE_COMMIT){save_status=-1;return -1;}
 save_have=1;save_address=target;save_sequence=next;for(unsigned i=0;i<STATE_BYTES;i++)save_previous[i]=save_record[i];save_status=save_uncertain=0;return 0;
}
static int persistence_flush(void){
 unsigned next=save_attempt_sequence+1;instrument_export(save_record,next);
 if(save_have&&!save_uncertain&&save_equal(save_record+4,save_previous+4,4)&&save_equal(save_record+16,save_previous+16,4)&&save_equal(save_record+32,save_previous+32,STATE_BYTES-32)){save_status=0;return 0;}
 // A commit may reach flash even if its verification read fails. Do not reuse
 // its sequence during this boot, or a later acknowledged save could tie it.
 save_attempt_sequence=next;
 unsigned active=save_have?save_address&~4095u:0,target=0;
 if(active)for(unsigned slot=0;slot<3;slot++)if(save_empty(active+slot*SAVE_STRIDE,SAVE_STRIDE)){target=active+slot*SAVE_STRIDE;break;}
 if(!target)for(unsigned sector=SAVE_LO;sector<SAVE_HI;sector+=4096){
  if(sector==active||(!save_owned(sector)&&!(active&&save_intent(active,sector,1))))continue;
  if(active&&!save_intent(active,sector,1)){
   uint8_t intent[16]={'N','S','X','I'};
   for(unsigned i=0;i<4;i++){intent[4+i]=(uint8_t)(sector>>(i*8));intent[8+i]=(uint8_t)(save_sequence>>(i*8));}
   uint32_t crc=state_crc(intent,16);for(unsigned i=0;i<4;i++)intent[12+i]=(uint8_t)(crc>>(i*8));
   if(save_write(active+SAVE_INTENT,intent,16)||!save_intent(active,sector,1)){save_status=-1;return -1;}
  }
  if(!save_empty(sector,4096)&&save_erase(sector)){save_status=-1;return -1;}target=sector;break;
 }
 if(!target){save_status=-2;return -2;}
 return save_commit_record(target,next);
}

// Read/check only the fixed settings journal. No caller-supplied addresses.
static int persistence_storage_crc(uint32_t*out){
 uint32_t c=~0u;
 for(unsigned off=SAVE_LO;off<SAVE_HI;off+=256){
  if(save_read(off,save_verify,256))return -1;
  for(unsigned i=0;i<256;i++){c^=save_verify[i];for(unsigned k=0;k<8;k++)c=(c>>1)^((c&1)?0xedb88320u:0);}
 }
 *out=~c;return 0;
}
static int persistence_initialize(uint32_t backup_crc,uint32_t config_crc){
 uint32_t actual;if(persistence_storage_crc(&actual)){save_status=-1;return -1;}
 if(actual!=backup_crc)return -4;
 // Rescan before choosing the sector: a prior commit could have reached flash
 // even if its acknowledgement/readback failed. Never erase that newer copy.
 unsigned active=0,latest=0,have=0;
 for(unsigned sector=SAVE_LO;sector<SAVE_HI;sector+=4096)for(unsigned slot=0;slot<3;slot++){
  unsigned off=sector+slot*SAVE_STRIDE;
  if(save_read(off,save_record,STATE_BYTES)||save_read(off+STATE_BYTES,save_verify,4)){save_status=-1;return -1;}
  if(save_u32(save_verify)!=SAVE_COMMIT||!instrument_valid(save_record,STATE_BYTES))continue;
  unsigned sequence=save_u32(save_record+20);
  if(!have||(int32_t)(sequence-latest)>0){have=1;latest=sequence;active=sector;}
 }
 instrument_export(save_record,0);if(save_u32(save_record+12)!=config_crc)return -4;
 unsigned target=active==SAVE_LO?SAVE_LO+4096:SAVE_LO;
 unsigned next=save_attempt_sequence;if(have&&(int32_t)(latest-next)>0)next=latest;next++;save_attempt_sequence=next;instrument_export(save_record,next);
 if(save_erase(target)||!save_empty(target,4096)){save_status=-1;return -1;}
 if(save_commit_record(target,next))return -1;
 // Only after the replacement is committed may the other sector be erased.
 // A cut here retains that record. A torn unknown sector stays protected and
 // can be reinitialized explicitly using a fresh backup; never auto-erase it.
 unsigned other=target==SAVE_LO?SAVE_LO+4096:SAVE_LO;
 if(save_erase(other)||!save_empty(other,4096)){save_status=-1;return -1;}
 save_status=0;return 0;
}
