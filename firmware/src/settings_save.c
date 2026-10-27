// SPDX-License-Identifier: GPL-3.0-only
// Included by main.c. Keep the external codec clocked during idle saves.
#ifndef SETTINGS_LOCK
#define SETTINGS_LOCK() __asm__ volatile("cli":::"memory")
#define SETTINGS_UNLOCK() __asm__ volatile("sti":::"memory")
#endif
static int settings_flush(void){
 // Serialize admission with note handling. Leave I2S/DMA clocking throughout
 // flash access: stopping it can pop the external codec, including on retries.
 SETTINGS_LOCK();
 if(nsx_active()||fm1_in.notes||mi_r!=mi_w||updating){SETTINGS_UNLOCK();return -3;}
 updating=1;uint32_t start=frame_counter;SETTINGS_UNLOCK();
 // The audio IRQ writes silence to the free half while updating is set. Wait
 // for both halves; never overwrite a DMA half that may still be playing.
 uint32_t began=fm1_ticks();
 while(frame_counter-start<2u){
  fm1_wdt_feed();
  if(fm1_ticks()-began>=24u*20000u){updating=0;return -3;}
 }
 // With the complete ring silent, DMA can repeat it autonomously even while
 // the existing RAM flash driver masks IRQs and temporarily suspends XIP.
 int result=persistence_flush();
 if(!result){instrument_export(save_record,0);settings_crc=save_u32(save_record+12);settings_dirty=0;}
 updating=0;return result;
}
static void settings_poll(void){
 if(fm1_ms-settings_checked<250)return;settings_checked=fm1_ms;
 SETTINGS_LOCK();instrument_export(save_record,0);SETTINGS_UNLOCK();
 uint32_t current=save_u32(save_record+12);if(current!=settings_crc){settings_crc=current;settings_changed=fm1_ms;settings_dirty=1;}
 if(save_uncertain&&!settings_dirty){settings_dirty=1;settings_changed=fm1_ms;}
 if(settings_dirty&&fm1_ms-settings_changed>=2000&&!nsx_active()&&!fm1_in.notes&&!usb.ota_req&&!updating){settings_flush();settings_changed=fm1_ms;}
}
