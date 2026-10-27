// SPDX-License-Identifier: GPL-3.0-only
// Included by main.c. Keep the external codec clocked during idle saves.
#ifndef SETTINGS_LOCK
#define SETTINGS_LOCK() __asm__ volatile("cli":::"memory")
#define SETTINGS_UNLOCK() __asm__ volatile("sti":::"memory")
#endif
static int settings_quiet(void){
 // Serialize admission with note handling. Leave I2S/DMA clocking throughout
 // flash access: stopping it can pop the external codec, including after a failed save.
 SETTINGS_LOCK();
 if(nsx_active()||fm1_in.notes||mi_r!=mi_w||updating){SETTINGS_UNLOCK();return -3;}
 settings_pending=0;updating=1;uint32_t start=frame_counter;SETTINGS_UNLOCK();
 // The audio IRQ writes silence to the free half while updating is set. Wait
 // for both halves; never overwrite a DMA half that may still be playing.
 uint32_t began=fm1_ticks();
 while(frame_counter-start<2u){
  fm1_wdt_feed();
  if(fm1_ticks()-began>=24u*20000u){updating=0;return -3;}
 }
 return 0;
}
static int settings_finish(int result){
 if(!result){instrument_export(save_record,0);settings_crc=save_u32(save_record+12);settings_dirty=0;}
 updating=0;return result;
}
static int settings_flush(void){
 int rc=settings_quiet();if(rc)return rc;
 // DMA repeats silence autonomously while the RAM flash driver masks IRQs.
 return settings_finish(persistence_flush());
}
static int settings_initialize(uint32_t backup_crc,uint32_t config_crc){
 int rc=settings_quiet();if(rc)return rc;
 return settings_finish(persistence_initialize(backup_crc,config_crc));
}
static void settings_poll(void){
 if(fm1_ms-settings_checked<250)return;settings_checked=fm1_ms;
 SETTINGS_LOCK();instrument_export(save_record,0);SETTINGS_UNLOCK();
 uint32_t current=save_u32(save_record+12);if(current!=settings_crc){settings_crc=current;settings_changed=fm1_ms;settings_dirty=settings_pending=1;save_status=0;}
 // One attempt per settled change. A failure stays visible, without polling
 // flash again; another change or an explicit MIDI save permits a new attempt.
 if(settings_pending&&settings_dirty&&fm1_ms-settings_changed>=2000&&!nsx_active()&&!fm1_in.notes&&mi_r==mi_w&&!usb.ota_req&&!updating){settings_flush();}
}
