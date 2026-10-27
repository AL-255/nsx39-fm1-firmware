// SPDX-License-Identifier: GPL-3.0-only
// Model only timing/DMA/flash I/O; exercise the actual autosave policy.
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t frame_counter,fm1_ms,ticks,settings_crc,settings_checked,settings_changed;
static unsigned updating,settings_dirty,save_uncertain,active,mi_r,mi_w,watchdog,locks,ring_half,stall;
static struct{unsigned notes;}fm1_in;
static struct{unsigned ota_req;}usb;
static uint8_t save_record[1120];
static int32_t dma[2][256];
static unsigned half_writes,flushes,config_crc=1234;
static int result;
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
 flushes++;return result;
}
#include "../firmware/src/settings_save.c"
static void fresh(void){frame_counter=fm1_ms=ticks=updating=settings_dirty=save_uncertain=active=mi_r=mi_w=watchdog=locks=ring_half=stall=half_writes=flushes=0;settings_crc=settings_checked=settings_changed=0;fm1_in.notes=usb.ota_req=0;result=0;memset(dma,0x35,sizeof dma);}
int main(void){
 fresh();active=1;assert(settings_flush()==-3);active=0;fm1_in.notes=1;assert(settings_flush()==-3);fm1_in.notes=0;mi_w=1;assert(settings_flush()==-3);mi_w=0;updating=1;assert(settings_flush()==-3);updating=0;assert(!flushes&&!half_writes&&!locks);
 frame_counter=UINT32_MAX;assert(!settings_flush());assert(frame_counter==1);assert(half_writes==2&&watchdog&&flushes==1&&!updating);assert(settings_crc==config_crc&&!settings_dirty);
 fresh();stall=1;assert(settings_flush()==-3);assert(!flushes&&!updating&&watchdog&&fm1_ms<=21);
 // A long active release defers saving. Failed saves can retry while idle,
 // without any audio-stop/start or direct writes to a live DMA half.
 fresh();active=1;for(unsigned t=250;t<=5000;t+=250){fm1_ms=t;settings_poll();}assert(!flushes&&settings_dirty);active=0;result=-2;
 for(unsigned t=5250;t<=12250;t+=250){fm1_ms=t;settings_poll();}assert(flushes>=3&&settings_dirty&&!updating);
 result=0;fm1_ms=15000;settings_poll();assert(!settings_dirty&&settings_crc==config_crc);unsigned n=flushes;
 for(unsigned t=15250;t<=20000;t+=250){fm1_ms=t;settings_poll();}assert(flushes==n);
 // Readback uncertainty retries even if the parameter snapshot is unchanged.
 save_uncertain=1;fm1_ms=21000;settings_poll();assert(settings_dirty);save_uncertain=0;fm1_ms=23500;settings_poll();assert(!settings_dirty&&flushes==n+1);
 puts("Autosave passed: two silent DMA halves, admission, timeout/wrap, deferred release, retries and stable idle");
}
