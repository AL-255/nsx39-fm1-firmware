// SPDX-License-Identifier: GPL-3.0-only
// Model only timing/DMA/flash I/O; exercise the actual autosave policy.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t frame_counter,fm1_ms,ticks,settings_crc,settings_checked,settings_changed;
static unsigned updating,settings_dirty,settings_pending,save_uncertain,active,mi_r,mi_w,watchdog,locks,ring_half,stall;
static struct{unsigned notes;}fm1_in;
static struct{unsigned ota_req;}usb;
static uint8_t save_record[1120];
static int32_t dma[2][256];
static unsigned half_writes,flushes,config_crc=1234;
static int result,save_status;
#define SETTINGS_LOCK() (locks++)
#define SETTINGS_UNLOCK() do{assert(locks);locks--;}while(0)
static unsigned nsx_active(void){return active;}
static void fm1_wdt_feed(void){watchdog++;}
static uint32_t fm1_ticks(void){
 ticks+=24000u;fm1_ms++;
 if(!stall&&fm1_ms%3==0){assert(!locks);if(updating){memset(dma[ring_half],0,sizeof dma[0]);ring_half^=1;half_writes++;}frame_counter++;}
 return ticks;
}
static uint32_t save_u32(const uint8_t*p){uint32_t value;memcpy(&value,p,4);return value;}
static void instrument_export(uint8_t*p,unsigned sequence){(void)sequence;memset(p,0,1120);memcpy(p+12,&config_crc,4);}
static int persistence_flush(void){
 assert(updating);assert(!locks);assert(half_writes>=2);
 for(unsigned i=0;i<2;i++)for(unsigned j=0;j<256;j++)assert(dma[i][j]==0);
 flushes++;save_status=result;return result;
}
static int persistence_initialize(uint32_t a,uint32_t b){(void)a;(void)b;return persistence_flush();}
#include "../firmware/src/settings_save.c"
static void fresh(void){frame_counter=fm1_ms=ticks=updating=settings_dirty=settings_pending=save_uncertain=active=mi_r=mi_w=watchdog=locks=ring_half=stall=half_writes=flushes=0;settings_crc=settings_checked=settings_changed=0;fm1_in.notes=usb.ota_req=0;result=save_status=0;config_crc=1234;memset(dma,0x35,sizeof dma);}
int main(void){
 fresh();settings_pending=1;active=1;assert(settings_flush()==-3&&settings_pending);active=0;fm1_in.notes=1;assert(settings_flush()==-3);fm1_in.notes=0;mi_w=1;assert(settings_flush()==-3);mi_w=0;updating=1;assert(settings_flush()==-3);updating=0;assert(!flushes&&!half_writes&&!locks);
 frame_counter=UINT32_MAX;assert(!settings_flush());assert(frame_counter==1);assert(half_writes==2&&watchdog&&flushes==1&&!updating);assert(settings_crc==config_crc&&!settings_dirty);
 fresh();stall=1;assert(settings_flush()==-3);assert(!flushes&&!updating&&watchdog&&fm1_ms<=21);
 // Debounce a burst of edits; a held/releasing voice postpones the attempt.
 fresh();active=1;
 for(unsigned t=250;t<=1500;t+=250){fm1_ms=t;config_crc++;settings_poll();}
 for(unsigned t=1750;t<=5000;t+=250){fm1_ms=t;settings_poll();}
 assert(!flushes&&settings_dirty&&settings_pending);active=0;result=-2;
 fm1_ms=5250;settings_poll();assert(flushes==1&&settings_dirty&&!settings_pending&&save_status==-2);
 // A failed or uncertain attempt must never become periodic flash traffic.
 save_uncertain=1;
 for(unsigned t=5500;t<=65000;t+=250){fm1_ms=t;settings_poll();}
 assert(flushes==1&&settings_dirty&&!settings_pending&&save_status==-2);
 // A later actual edit allows exactly one new attempt after two seconds.
 config_crc++;fm1_ms=65250;settings_poll();assert(settings_pending&&save_status==0);
 fm1_ms=67000;settings_poll();assert(flushes==1);
 fm1_ms=67250;settings_poll();assert(flushes==2&&settings_dirty&&!settings_pending);
 // Explicit saves are allowed after failure; success clears the pending edit.
 result=0;save_uncertain=0;assert(!settings_flush());assert(flushes==3&&!settings_dirty&&!settings_pending&&settings_crc==config_crc);
 for(unsigned t=67500;t<=120000;t+=250){fm1_ms=t;settings_poll();}assert(flushes==3);
 // An explicit save before the debounce expires consumes its automatic attempt.
 config_crc++;fm1_ms=120250;settings_poll();assert(settings_pending);assert(!settings_flush());unsigned n=flushes;
 for(unsigned t=120500;t<=125000;t+=250){fm1_ms=t;settings_poll();}assert(flushes==n);
 // A failed explicit save also consumes the pending automatic attempt.
 config_crc++;fm1_ms=125250;settings_poll();assert(settings_pending);result=-1;save_uncertain=1;assert(settings_flush()==-1);n=flushes;
 for(unsigned t=125500;t<=180000;t+=250){fm1_ms=t;settings_poll();}assert(flushes==n&&settings_dirty&&!settings_pending);
 // A stalled audio ring also consumes the attempt without repeatedly waiting.
 fresh();stall=1;fm1_ms=250;settings_poll();fm1_ms=2250;settings_poll();assert(!settings_pending&&settings_dirty&&!flushes);n=watchdog;
 for(unsigned t=2500;t<=10000;t+=250){fm1_ms=t;settings_poll();}assert(watchdog==n&&!flushes);
 puts("Autosave passed: silent DMA admission, timeout/wrap, debounce, one attempt per change, no idle/uncertain retries and explicit save");
}
