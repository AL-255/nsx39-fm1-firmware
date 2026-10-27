// SPDX-License-Identifier: GPL-3.0-only
#include <stdint.h>
#include "nsx.h"
#define FELUCCA_OTA 1
#define FELUCCA_CDC 0
#define FELUCCA_UAC 0
#define FELUCCA_ID "FM-1_947"
// Miku cyan #39C5BB, quantized to the LCD's RGB565 format.
#define UI_ACCENT 0x3e37u
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#include "fm1_time.h"
// Always halt a possibly running second core inherited from previous firmware.
#define FM1_CPU1_USED 1
#include "fm1_sys.h"
#undef FM1_CPU1_USED
#define FM1_CPU1_USED 0
#include "fm1_irq.h"
#include "fm1_guard.h"
#include "fm1_input.h"
#include "fm1_timer.h"
#include "fm1_audio.h"
#include "fm1_adc.h"
#include "fm1_lcd_hw.h"
#include "fm1_uart.h"
// OTA staging and the separate two-sector settings journal only.
#define FL_RANGE_OK(off,n) FL_IN(off,n,0xe0000u,0xe7000u)
#include "fm1_flash.h"
#include "../../vendor/felucca/firmware/src/libc.c"
#include "../../vendor/felucca/firmware/src/usb.c"
#include "../../vendor/felucca/firmware/src/lcd.c"
#include "voice.c"
#include "performance.c"
#include "instrument.c"
#include "persistence.c"
const uint8_t voice_bank[NSX_BANK_CAPACITY] __attribute__((section(".voice_bank"),used))={0};
static int32_t dma_audio[512] __attribute__((aligned(4)));
static volatile uint32_t audio_busy,render_us,render_max_us,audio_late,frame_counter;
static volatile uint32_t master_level;
static int voice_ready;
static unsigned settings_open,settings_item,reset_armed,settings_dirty,settings_pending,runtime_ready,save_supported;
static uint32_t settings_crc,settings_changed,settings_checked;
static unsigned panel_note=255,panel_key=255,held_note_mask;
static volatile int16_t scope[512];
static volatile unsigned scope_w;
static uint16_t canvas[240*240] __attribute__((section(".pool")));
static volatile uint8_t updating;
static uint8_t uart_ring[128] __attribute__((aligned(16)));
static uint32_t uart_pos;
static uint8_t uart_status,uart_got,uart_first;
static void uart_poll(void){unsigned n=fm1_uart1_rx_take();if(n>128){n=128;uart_status=uart_got=0;midi_in_overflow=1;}while(n--){uint8_t b=uart_ring[uart_pos++&127];if(b>=0xf8)continue;if(b&128){uart_status=b<0xf0?b:0;uart_got=0;continue;}if(!uart_status)continue;if((uart_status&0xe0)==0xc0){midi_enqueue((uart_status>>4)|uart_status<<8|(uint32_t)b<<16,2);continue;}if(!uart_got){uart_first=b;uart_got=1;}else{midi_enqueue((uart_status>>4)|uart_status<<8|(uint32_t)uart_first<<16|(uint32_t)b<<24,2);uart_got=0;}}}
void fm1_timer5_irq(void){static uint32_t sub,last,acc;fm1_timer5_ack();fm1_input_tick();uint32_t t=fm1_ticks();acc+=t-last;last=t;fm1_ms+=acc/24000u;acc%=24000u;if((++sub%5)==0&&!audio_busy){usb_poll();uart_poll();}}
extern void isr_timer5(void),isr_alnk0(void);
static void panel_audio(void){
 if(updating||!voice_ready)return;
 unsigned notes=fm1_in.notes,pressed=notes&~held_note_mask;held_note_mask=notes;
 int revoice=0;if(!pressed&&notes&&panel_key<27&&!(notes&(1u<<panel_key))){pressed=notes;revoice=1;}
 if(pressed){unsigned k=0;while(pressed>>1){pressed>>=1;k++;}panel_key=k;panel_note=53+12*instrument_octave()+k;if(revoice)performance_revoice((uint8_t)panel_note,127);else performance_trigger((uint8_t)panel_note,127);}
 if(panel_note!=255&&!notes){nsx_note_off((uint8_t)panel_note);panel_note=255;panel_key=255;}
 if(midi_in_overflow){nsx_midi(0xb0,120,0);mi_r=mi_w;midi_in_overflow=0;}
 unsigned budget=MQ;while(mi_r!=mi_w&&budget--){uint32_t p=midi_in_q[mi_r++%MQ];performance_midi((uint8_t)(p>>8),(uint8_t)(p>>16),(uint8_t)(p>>24));}
}
void fm1_alnk0_irq(void){
 uint32_t t=fm1_ticks();uint8_t p=fm1_audio_pending();audio_busy=1;fm1_audio_ack_aux(p);
 if(p&FM1_AUDIO_HALF){int32_t*out=dma_audio+fm1_audio_free_half()*256;panel_audio();int16_t mono[64];if(!updating)nsx_render(mono,64);else for(unsigned i=0;i<64;i++)mono[i]=0;
  // 22.05 -> 44.1 kHz linear interpolator, continuous between halves.
  static int32_t prev;if(updating)prev=0;for(unsigned i=0;i<64;i++){int32_t s=mono[i];scope[scope_w++&511]=(int16_t)(s*(int32_t)master_level/1024);int32_t mid=(prev+s)/2,g=(int32_t)master_level;out[i*4]=out[i*4+1]=(mid*g/1024)*128;out[i*4+2]=out[i*4+3]=(s*g/1024)*128;prev=s;}
  fm1_audio_ack_half();frame_counter++;
 }
 render_us=(fm1_ticks()-t)/24;if(render_us>render_max_us)render_max_us=render_us;if(render_us>2900)audio_late++;audio_busy=0;
}
static void text_line(unsigned y,const char*s,uint16_t color){
 // Small bitmap font rasterized from DejaVu Sans Mono; license in vendor/.
 extern const uint8_t ui_font[96][14];unsigned x=12;lcd_fill(0,y,240,28,0x0000);
 while(*s&&x<232){unsigned c=(unsigned char)*s++;if(c<32||c>127)c='?';for(unsigned row=0;row<14;row++)for(unsigned bit=0;bit<8;bit++)if(ui_font[c-32][row]&(1u<<bit))lcd_fill(x+bit,y+row,1,1,color);x+=8;}
}
#include "ui_font.h"
#include "label.c"
#include "display.c"
static void fm1_fault(const fm1_crash_t*c){(void)c;fm1_audio_stop();fm1_wdt_stop();lcd_init();text_line(90,"FAULT - RESTART",0xf800);for(;;);}
#include "../../vendor/felucca/firmware/src/ota.c"
static uint32_t ota_now_ms(void){return fm1_ms;}
static void ota_idle(void){fm1_wdt_feed();}
static int ota_erase(uint32_t off){uint32_t t;if(!FL_IN(off,4096,OTA_AREA,OTA_AREA+OTA_AREA_LEN)||(off&4095))return -8;return fl_erase4k(off,&t);}
static int ota_prog(uint32_t off,const void*p,uint32_t n){if(!FL_IN(off,n,OTA_AREA,OTA_AREA+OTA_AREA_LEN))return -8;return fl_write(off,p,n);}
static int ota_fread(uint32_t off,void*p,uint32_t n){uint32_t f=irq_save();int rc=FL_FAR(fl_read_ram)(off,p,n);irq_restore(f);return rc;}
static void ota_show(uint32_t step,int32_t code){(void)step;text_line(90,code<0?"UPDATE FAILED":"UPDATING - WAIT",0xfca0);}
static void ota_commit(const uint8_t*parm){usb_detach();fm1_delay_ms(30);fm1_enter_update(parm);}
static int memory_sysex(const uint8_t*p,unsigned n);
static void musical_sysex(void){if(sx_ready){if(memory_sysex(sx_frame,sx_frame_len))sx_ready=0;else if(sx_frame_len>=5&&sx_frame[0]==0x43&&sx_frame[1]==0x79){__asm__ volatile("cli":::"memory");performance_sysex(sx_frame,sx_frame_len);sx_ready=0;__asm__ volatile("csync\nsti":::"memory");}}}
static void panel_controls(void){
 static unsigned oct_pending,oct_chord;static uint32_t chord_time;
 uint32_t buttons=fm1_input_edges(0),oct=fm1_in.buttons&3u;
 oct_pending|=oct;if(oct==3&&!oct_chord){oct_chord=1;chord_time=fm1_ms;}
 if(!oct&&oct_pending){
  if(oct_chord&&fm1_ms-chord_time<1000){settings_open=!settings_open;reset_armed=0;}
  else if(!oct_chord){unsigned n=instrument_octave();if(oct_pending==1)instrument_octave_set(n?n-1:0);if(oct_pending==2)instrument_octave_set(n<2?n+1:2);}
  oct_pending=oct_chord=0;
 }
 for(unsigned button=2;button<14;button++)if(buttons&(1u<<button)){__asm__ volatile("cli":::"memory");nsx_panel_button(button);__asm__ volatile("sti":::"memory");}
 for(unsigned knob=0;knob<4;knob++){
  int step=fm1_enc_take(knob+2);if(!step)continue;
  __asm__ volatile("cli":::"memory");
  if(settings_open){
   if(knob==0){settings_item=(unsigned)clip((int)settings_item+step,0,SET_COUNT-1);reset_armed=0;}
   if(knob==1){if(settings_item==SET_RESET){if(step>0){if(reset_armed){instrument_defaults();reset_armed=0;}else reset_armed=1;}else reset_armed=0;}else instrument_change(settings_item,step);}
  }else nsx_adsr_knob(knob,step);
  __asm__ volatile("sti":::"memory");
 }
 int delta=fm1_enc_take(0);if(delta){__asm__ volatile("cli":::"memory");performance_select(delta);__asm__ volatile("sti":::"memory");}
 delta=fm1_enc_take(1);if(delta){__asm__ volatile("cli":::"memory");performance_rotate(delta);__asm__ volatile("sti":::"memory");}
 delta=fm1_enc_take(6);if(delta)instrument_script_set((unsigned)((int)instrument_script()+delta%3+3));
}
static int save_read(uint32_t off,void*p,unsigned n){if(!FL_IN(off,n,SAVE_LO,SAVE_HI))return -1;__asm__ volatile("cli":::"memory");int rc=FL_FAR(fl_read_ram)(off,p,n);if(runtime_ready)__asm__ volatile("csync\nsti":::"memory");return rc;}
static int save_write(uint32_t off,const void*p,unsigned n){if(!save_supported||!FL_IN(off,n,SAVE_LO,SAVE_HI))return -1;fm1_wdt_feed();return fl_write(off,p,n);}
static int save_erase(uint32_t off){if(!save_supported||!FL_IN(off,4096,SAVE_LO,SAVE_HI)||(off&4095))return -1;uint32_t took;fm1_wdt_feed();return fl_erase4k(off,&took);}
#include "settings_save.c"
#include "memory_midi.c"
extern uint32_t _data_start[],_data_end[],_data_load[],_bss_start[],_bss_end[],_pool_start[],_pool_end[],_rt_start[],_rt_end[],_rt_load[];
// Retained SRAM is outside the C zero-initialization contract. Volatile forces
// a real 32-bit read/compare: the pi32v2 compiler otherwise folds static
// boot_magic into a one-byte Boolean and trusts arbitrary retained RAM.
static volatile uint32_t boot_attempts __attribute__((section(".noinit"))),boot_magic __attribute__((section(".noinit")));
void fm1_cstart(void){
 fm1_cpu1_halt();fm1_time_init();fm1_reset_reason();fm1_wdt_arm(0xd);
 if(boot_magic!=0x4e535842){boot_magic=0x4e535842;boot_attempts=0;}if(++boot_attempts>2){boot_attempts=0;fm1_enter_uboot();}
 for(uint32_t*d=_bss_start;d<_bss_end;d++)*d=0;for(uint32_t*d=_pool_start;d<_pool_end;d++)*d=0;
 for(uint32_t*s=_data_load,*d=_data_start;d<_data_end;s++,d++)*d=*s;
 for(uint32_t*s=_rt_load,*d=_rt_start;d<_rt_end;s++,d++)*d=*s;
 fm1_irq_init();fm1_mailbox_clear();fm1_guard_enable(FM1_GUARD_STACK|FM1_GUARD_WRITE|FM1_GUARD_BUS|FM1_GUARD_PC);
 lcd_init();fm1_wdt_feed();fm1_input_init();fm1_adc_init();int a=fm1_adc_read(FM1_ADC_MASTER);master_level=a<0?0:(unsigned)a;
 voice_ready=nsx_init(voice_bank,sizeof voice_bank);performance_init(nsx_factory_lyrics());unsigned seed_size;const uint8_t*seed=nsx_seed_state(&seed_size);if(!instrument_init(seed,seed_size))voice_ready=0;save_supported=FL_FAR(fl_jedec_ram)()==0x856014u;persistence_load();instrument_export(save_record,0);settings_crc=save_u32(save_record+12);fm1_wdt_feed();
 usb_start();fm1_uart1_midi_init(uart_ring,128);fm1_audio_init(dma_audio,256,isr_alnk0,3);fm1_timer5_start(isr_timer5,4);fm1_guard_lock_top();runtime_ready=1;fm1_irq_enable_all();screen();
 uint32_t shown=0,both_start=0;
 for(;;){fm1_wdt_feed();usb_retry(fm1_ms);musical_sysex();ota_service();panel_controls();settings_poll();int a=fm1_adc_read(FM1_ADC_MASTER);if(a>=0)master_level=(master_level*7+(unsigned)a)/8;
  if(fm1_ms>30000)boot_attempts=0;
  if((fm1_in.buttons&3)!=3)both_start=fm1_ms;else if(fm1_ms-both_start>5000){fm1_audio_stop();usb_detach();fm1_enter_uboot();}
  if(usb.ota_req){usb.ota_req=0;updating=1;uint32_t f=irq_save();uint32_t id=FL_FAR(fl_jedec_ram)();irq_restore(f);if(id==0x856014u)ota_session();else text_line(90,"UNKNOWN FLASH",0xf800);updating=0;}
  if(usb.uboot_req){fm1_audio_stop();usb_detach();fm1_enter_uboot();}
  if(fm1_ms-shown>80){shown=fm1_ms;screen();}
 }
}
