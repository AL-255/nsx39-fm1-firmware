// SPDX-License-Identifier: GPL-3.0-only
#include <stdint.h>
#include "nsx.h"
#define FELUCCA_OTA 1
#define FELUCCA_CDC 0
#define FELUCCA_UAC 0
#define FELUCCA_ID "FM-1_942"
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
// Application can write only the five OTA staging sectors. No musical setting
// can erase/program flash, and no Yamaha firmware-update command is forwarded.
#define FL_RANGE_OK(off,n) FL_IN(off,n,0xe0000u,0xe5000u)
#include "fm1_flash.h"
#include "../../vendor/felucca/firmware/src/libc.c"
#include "../../vendor/felucca/firmware/src/usb.c"
#include "../../vendor/felucca/firmware/src/lcd.c"
#include "voice.c"
#include "performance.c"
const uint8_t voice_bank[NSX_BANK_CAPACITY] __attribute__((section(".voice_bank"),used))={0};
static int32_t dma_audio[512] __attribute__((aligned(4)));
static volatile uint32_t audio_busy,render_us,render_max_us,audio_late,frame_counter;
static volatile uint32_t master_level;
static int voice_ready;
static unsigned script_mode;
static unsigned octave=1,panel_note=255,panel_key=255,held_note_mask;
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
 unsigned notes=fm1_in.notes,pressed=notes&~held_note_mask;held_note_mask=notes;
 int revoice=0;if(!pressed&&notes&&panel_key<27&&!(notes&(1u<<panel_key))){pressed=notes;revoice=1;}
 if(pressed){unsigned k=0;while(pressed>>1){pressed>>=1;k++;}panel_key=k;panel_note=53+12*octave+k;if(revoice)performance_revoice((uint8_t)panel_note,127);else performance_trigger((uint8_t)panel_note,127);}
 if(panel_note!=255&&!notes){nsx_note_off((uint8_t)panel_note);panel_note=255;panel_key=255;}
 if(midi_in_overflow){nsx_midi(0xb0,120,0);mi_r=mi_w;midi_in_overflow=0;}
 unsigned budget=MQ;while(mi_r!=mi_w&&budget--){uint32_t p=midi_in_q[mi_r++%MQ];performance_midi((uint8_t)(p>>8),(uint8_t)(p>>16),(uint8_t)(p>>24));}
}
void fm1_alnk0_irq(void){
 uint32_t t=fm1_ticks();uint8_t p=fm1_audio_pending();audio_busy=1;fm1_audio_ack_aux(p);
 if(p&FM1_AUDIO_HALF){int32_t*out=dma_audio+fm1_audio_free_half()*256;panel_audio();int16_t mono[64];if(!updating)nsx_render(mono,64);else for(unsigned i=0;i<64;i++)mono[i]=0;
  // 22.05 -> 44.1 kHz linear interpolator, continuous between halves.
  static int32_t prev;for(unsigned i=0;i<64;i++){int32_t s=mono[i];if(nsx_active())scope[scope_w++&511]=(int16_t)(s*(int32_t)master_level/1024);int32_t mid=(prev+s)/2,g=(int32_t)master_level;out[i*4]=out[i*4+1]=(mid*g/1024)*128;out[i*4+2]=out[i*4+3]=(s*g/1024)*128;prev=s;}
  fm1_audio_ack_half();frame_counter++;
 }
 render_us=(fm1_ticks()-t)/24;if(render_us>render_max_us)render_max_us=render_us;if(render_us>2900)audio_late++;audio_busy=0;
}
static void text_line(unsigned y,const char*s,uint16_t color){
 // Small bitmap font rasterized from DejaVu Sans Mono; license in vendor/.
 extern const uint8_t ui_font[96][8];unsigned x=12;lcd_fill(0,y,240,18,0x0841);
 while(*s&&x<228){unsigned c=(unsigned char)*s++;if(c<32||c>127)c='?';for(unsigned row=0;row<8;row++)for(unsigned bit=0;bit<6;bit++)if(ui_font[c-32][row]&(1u<<bit))lcd_fill(x+bit*2,y+row*2,2,2,color);x+=12;}
}
#include "ui_font.h"
#include "label.c"
static void number(char*out,unsigned v){char b[12];unsigned n=0;do{b[n++]=(char)('0'+v%10);v/=10;}while(v);unsigned i=0;while(n)out[i++]=b[--n];out[i]=0;}
static void put_text(unsigned x,unsigned y,const char*s,uint16_t color,unsigned scale){
 extern const uint8_t ui_font[96][8];while(*s&&x+6*scale<240){unsigned c=(unsigned char)*s++;if(c<32||c>127)c='?';for(unsigned row=0;row<8;row++)for(unsigned bit=0;bit<6;bit++)if(ui_font[c-32][row]&(1u<<bit))for(unsigned a=0;a<scale;a++)for(unsigned b=0;b<scale;b++)if(y+row*scale+b<240)canvas[(y+row*scale+b)*240+x+bit*scale+a]=color;x+=6*scale;}
}
static void label_num(unsigned x,unsigned y,const char*label,unsigned n){char b[12];put_text(x,y,label,0xad55,1);number(b,n);put_text(x+str_len(label)*6,y,b,0xffff,1);}
static void ui_line(int x,int y,int xx,int yy,uint16_t color){int dx=xx>x?xx-x:x-xx,dy=yy>y?yy-y:y-yy,sx=x<xx?1:-1,sy=y<yy?1:-1,error=dx-dy;for(;;){if(x>=0&&x<240&&y>=0&&y<240)canvas[y*240+x]=color;if(x==xx&&y==yy)break;int e=error*2;if(e>-dy){error-=dy;x+=sx;}if(e<dx){error+=dx;y+=sy;}}}
static void screen(void){
 nsx_debug_t state;nsx_art_debug_t art;performance_state_t song;int16_t wave[240];unsigned w;
 __asm__ volatile("cli":::"memory");nsx_debug(&state);nsx_art_debug(&art);performance_snapshot(&song);w=scope_w;for(unsigned i=0;i<240;i++)wave[i]=scope[(w-240+i)&511];__asm__ volatile("sti":::"memory");
 for(unsigned i=0;i<240*240;i++)canvas[i]=0x0841;
 static const char*const scripts[]={"HIRAGANA","KATAKANA","ROMAJI"};
 put_text(10,8,"NSX39",UI_ACCENT,1);put_text(112,8,scripts[script_mode],UI_ACCENT,1);
 if(!voice_ready){put_text(10,40,"NO VALID VOICE BANK",0xfca0,1);put_text(10,64,"Generate with your dump",0xffff,1);}
 else{
  label_num(10,25,"LYRIC ",song.mode);put_text(76,25,song.mode?"ORIGINAL":"MANUAL / SELECT",0xad55,1);
  put_text(10,32,"LAST PLAYED",0xad55,1);if(song.has_last)label_num(150,32,"ID ",song.last);
  ui_label(canvas,10,77,song.has_last?ui_syllable(song.last,script_mode):"—",1,UI_ACCENT);
  put_text(10,91,"NEXT",0xad55,1);ui_label(canvas,46,106,ui_syllable(song.next,script_mode),2,UI_ACCENT);
  if(song.mode){label_num(124,91,"STEP ",song.cursor+1);label_num(184,91,"/",song.length);}else label_num(152,91,"ID ",song.next);
  static const char*const types[]={"EXTREME","FAST","NORMAL","SLIGHT"};
  put_text(10,114,"VIB",0xad55,1);put_text(34,114,art.vibrato_depth?types[art.vibrato_type]:"OFF",UI_ACCENT,1);label_num(94,114,"DEP ",art.vibrato_depth);label_num(166,114,"RATE ",art.vibrato_rate);
  label_num(10,125,"DELAY ms ",art.vibrato_delay);label_num(132,125,"GLIDE ",art.portamento);
  put_text(10,136,art.auto_pitch?"AP ON":"AP OFF",art.auto_pitch?UI_ACCENT:0xad55,1);put_text(58,136,art.auto_dynamics?"DYN ON":"DYN OFF",art.auto_dynamics?UI_ACCENT:0xad55,1);
  put_text(118,136,"NOISE",0xad55,1);int db=((int)art.noise-64)*24/(art.noise<=64?64:63);put_text(154,136,db<0?"-":"+",UI_ACCENT,1);label_num(160,136,"",(unsigned)(db<0?-db:db));put_text(178,136,"dB",0xad55,1);
  static const char*const connections[]={"50ms","MIN","VEL"};
  put_text(10,147,"JOIN",0xad55,1);put_text(40,147,connections[art.connection],UI_ACCENT,1);
  put_text(82,147,art.hold?"HOLD ON":"HOLD OFF",art.hold?UI_ACCENT:0xad55,1);put_text(148,147,art.release_unit&&art.stage==NSX_ENV_RELEASE?"TAIL":"",UI_ACCENT,1);
  label_num(10,159,"A",art.adsr[0]);label_num(68,159,"D",art.adsr[1]);label_num(126,159,"S%",art.adsr[2]);label_num(184,159,"R",art.adsr[3]);
 }
 static const char*const stages[]={"IDLE","A","D","S","R"};
 put_text(10,172,"ADSR ms/%",0xad55,1);put_text(76,172,stages[art.stage],UI_ACCENT,1);put_text(132,172,"WAVE",0xad55,1);
 unsigned total=art.adsr[0]+art.adsr[1]+500+art.adsr[3];
 unsigned ax=13+art.adsr[0]*85/total,dx=ax+3+art.adsr[1]*85/total,sx=dx+3+500*85/total;
 unsigned sy=213-art.adsr[2]*30/100;ui_line(10,213,(int)ax,183,art.stage==NSX_ENV_ATTACK?UI_ACCENT:0x5b6c);
 ui_line((int)ax,183,(int)dx,(int)sy,art.stage==NSX_ENV_DECAY?UI_ACCENT:0x5b6c);ui_line((int)dx,(int)sy,(int)sx,(int)sy,art.stage==NSX_ENV_SUSTAIN?UI_ACCENT:0x5b6c);ui_line((int)sx,(int)sy,110,213,art.stage==NSX_ENV_RELEASE?UI_ACCENT:0x5b6c);
 unsigned px=110;if(art.stage==NSX_ENV_ATTACK)px=10+(ax-10)*art.progress/32768;else if(art.stage==NSX_ENV_DECAY)px=ax+(dx-ax)*art.progress/32768;else if(art.stage==NSX_ENV_SUSTAIN)px=(dx+sx)/2;else if(art.stage==NSX_ENV_RELEASE)px=sx+(110-sx)*art.progress/32768;
 int py=213-(int)(art.level*30/32768);if(art.stage)for(int y=py-1;y<=py+1;y++)for(unsigned x=px-1;x<=px+1;x++)if(y>=181&&y<216)canvas[(unsigned)y*240+x]=0xffff;
 for(unsigned x=130;x<232;x++)canvas[198*240+x]=0x2965;
 int last_y=198;for(unsigned x=0;x<102;x++){int y=clip(198-wave[x+138]*15/32768,182,214);int a=last_y<y?last_y:y,b=last_y>y?last_y:y;for(int q=a;q<=b;q++)canvas[q*240+130+x]=UI_ACCENT;last_y=y;}
 label_num(10,220,"NOTE ",state.note);label_num(82,220,"OCT ",octave);label_num(148,220,"LATE ",audio_late);
 put_text(10,232,"KNOBS 1-4: A D S R",0xad55,1);label_num(148,232,"MAX us ",render_max_us);
 // Reflect live state, including external MIDI changes, in the panel LEDs.
 fm1_led_key(2,art.noise!=64);fm1_led_key(3,art.auto_pitch);fm1_led_key(4,art.auto_dynamics);fm1_led_key(5,art.vibrato_depth!=0);
 fm1_led_key(6,art.vibrato_type!=2);fm1_led_key(7,art.vibrato_rate!=64);fm1_led_key(8,art.vibrato_delay!=0);fm1_led_key(10,art.connection!=1);fm1_led_key(11,art.portamento!=0);fm1_led_key(12,art.hold);fm1_led_key(13,art.vibrato_depth!=0);
 lcd_sync(); // DMA must finish before its source buffer can be reused.
 // lcd_blit reads the buffer until the next lcd_sync, so finish before return.
 for(unsigned i=0;i<240*240;i++)canvas[i]=(uint16_t)((canvas[i]>>8)|(canvas[i]<<8));
 lcd_blit(0,0,240,240,canvas);lcd_sync();
}
static void fm1_fault(const fm1_crash_t*c){(void)c;fm1_audio_stop();fm1_wdt_stop();lcd_init();text_line(90,"FAULT - RESTART",0xf800);for(;;);}
#include "../../vendor/felucca/firmware/src/ota.c"
static uint32_t ota_now_ms(void){return fm1_ms;}
static void ota_idle(void){fm1_wdt_feed();}
static int ota_erase(uint32_t off){uint32_t t;if(!FL_IN(off,4096,OTA_AREA,OTA_AREA+OTA_AREA_LEN)||(off&4095))return -8;return fl_erase4k(off,&t);}
static int ota_prog(uint32_t off,const void*p,uint32_t n){if(!FL_IN(off,n,OTA_AREA,OTA_AREA+OTA_AREA_LEN))return -8;return fl_write(off,p,n);}
static int ota_fread(uint32_t off,void*p,uint32_t n){uint32_t f=irq_save();int rc=FL_FAR(fl_read_ram)(off,p,n);irq_restore(f);return rc;}
static void ota_show(uint32_t step,int32_t code){(void)step;text_line(90,code<0?"UPDATE FAILED":"UPDATING - WAIT",0xfca0);}
static void ota_commit(const uint8_t*parm){usb_detach();fm1_delay_ms(30);fm1_enter_update(parm);}
static void musical_sysex(void){if(sx_ready&&sx_frame_len>=5&&sx_frame[0]==0x43&&sx_frame[1]==0x79){__asm__ volatile("cli":::"memory");performance_sysex(sx_frame,sx_frame_len);sx_ready=0;__asm__ volatile("csync\nsti":::"memory");}}
static void panel_controls(void){
 uint32_t buttons=fm1_input_edges(0);if(buttons&1)octave=octave?octave-1:0;if(buttons&2)octave=octave<2?octave+1:2;
 // Musical controls change RAM only. The updater and recovery controls are separate.
 for(unsigned button=2;button<14;button++)if(buttons&(1u<<button)){__asm__ volatile("cli":::"memory");nsx_panel_button(button);__asm__ volatile("sti":::"memory");}
 for(unsigned knob=0;knob<4;knob++){int step=fm1_enc_take(knob+2);if(step){__asm__ volatile("cli":::"memory");nsx_adsr_knob(knob,step);__asm__ volatile("sti":::"memory");}}
 int delta=fm1_enc_take(0);if(delta){__asm__ volatile("cli":::"memory");performance_select(delta);__asm__ volatile("sti":::"memory");}
 delta=fm1_enc_take(1);if(delta){__asm__ volatile("cli":::"memory");performance_rotate(delta);__asm__ volatile("sti":::"memory");}
 delta=fm1_enc_take(6);if(delta)script_mode=(unsigned)((int)script_mode+delta%3+3)%3;
}
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
 voice_ready=nsx_init(voice_bank,sizeof voice_bank);performance_init(nsx_factory_lyrics());fm1_wdt_feed();
 usb_start();fm1_uart1_midi_init(uart_ring,128);fm1_audio_init(dma_audio,256,isr_alnk0,3);fm1_timer5_start(isr_timer5,4);fm1_guard_lock_top();fm1_irq_enable_all();screen();
 uint32_t shown=0,both_start=0;
 for(;;){fm1_wdt_feed();usb_retry(fm1_ms);musical_sysex();ota_service();panel_controls();int a=fm1_adc_read(FM1_ADC_MASTER);if(a>=0)master_level=(master_level*7+(unsigned)a)/8;
  if(fm1_ms>30000)boot_attempts=0;
  if((fm1_in.buttons&3)!=3)both_start=fm1_ms;else if(fm1_ms-both_start>5000){fm1_audio_stop();usb_detach();fm1_enter_uboot();}
  if(usb.ota_req){usb.ota_req=0;updating=1;uint32_t f=irq_save();uint32_t id=FL_FAR(fl_jedec_ram)();irq_restore(f);if(id==0x856014u)ota_session();else text_line(90,"UNKNOWN FLASH",0xf800);updating=0;}
  if(usb.uboot_req){fm1_audio_stop();usb_detach();fm1_enter_uboot();}
  if(fm1_ms-shown>80){shown=fm1_ms;screen();}
 }
}
