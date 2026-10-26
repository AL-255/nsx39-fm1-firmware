// SPDX-License-Identifier: GPL-3.0-only
// Included by voice.c after the bank readers and DSP state declarations.
// Control math follows observed NSX39 behavior. Curves are always read from the
// user's bank. The additional ADSR is an FM-1 performance control, not a Yamaha NRPN.
#include "nsx.h"
typedef struct {unsigned stage,elapsed,length,level,start,target,step,remainder,error;} envelope_t;
static envelope_t envelope;
static unsigned adsr_ms[4]={0,0,100,100},adsr_knobs[4]={0,0,127,20};
static unsigned vibrato_delay_ms,vibrato_position,vibrato_reverse,vibrato_ramp,vibrato_active,vibrato_start_frame,tail_vibrato;
static int32_t articulation_vibrato_q16;
static unsigned articulation_frames,articulation_samples;
static const uint8_t *art_curves[4];
static unsigned art_curve_lengths[4];
static int32_t art_multiply(int32_t a,int32_t b){return (int32_t)(((int64_t)a*b)>>16);}
// Original MIDI scalar: round(value * 0x10102 / 65536) * 512.
static unsigned art_scalar(unsigned value){return ((value*0x10102u+0x8000u)>>16)*512u;}
static void envelope_begin(unsigned stage,unsigned target,unsigned length){
 envelope.stage=stage;envelope.elapsed=0;envelope.length=length;envelope.start=envelope.level;envelope.target=target;envelope.error=0;
 unsigned distance=target>envelope.level?target-envelope.level:envelope.level-target;
 envelope.step=length?distance/length:distance;envelope.remainder=length?distance%length:0;
}
static void envelope_next(void){
 if(envelope.stage==NSX_ENV_ATTACK)envelope_begin(NSX_ENV_DECAY,adsr_ms[2]*32768u/100u,adsr_ms[1]*NSX_RATE/1000u);
 else if(envelope.stage==NSX_ENV_DECAY){envelope.stage=NSX_ENV_SUSTAIN;envelope.level=adsr_ms[2]*32768u/100u;}
 else {envelope.stage=NSX_ENV_IDLE;envelope.level=0;}
}
static unsigned envelope_sample(void){
 for(unsigned guard=0;guard<3;guard++){
  if(envelope.stage==NSX_ENV_IDLE||envelope.stage==NSX_ENV_SUSTAIN)return envelope.level;
  if(!envelope.length){envelope.level=envelope.target;envelope_next();continue;}
  unsigned step=envelope.step;envelope.error+=envelope.remainder;if(envelope.error>=envelope.length){envelope.error-=envelope.length;step++;}
  if(envelope.target>=envelope.start)envelope.level+=step;else envelope.level-=step;
  if(++envelope.elapsed>=envelope.length){envelope.level=envelope.target;envelope_next();}
  break;
 }
 return envelope.level;
}
static void articulation_note_on(unsigned legato){
 envelope_begin(NSX_ENV_ATTACK,32768,adsr_ms[0]*NSX_RATE/1000u);
 tail_vibrato=0;
 if(!legato){articulation_frames=articulation_samples=vibrato_start_frame=0;vibrato_active=vibrato_ramp=vibrato_position=vibrato_reverse=0;articulation_vibrato_q16=0;}
}
static void articulation_note_off(void){tail_vibrato=vibrato_active;envelope_begin(NSX_ENV_RELEASE,0,adsr_ms[3]*NSX_RATE/1000u);}
static void articulation_panic(void){envelope.level=0;envelope.stage=NSX_ENV_IDLE;articulation_vibrato_q16=0;vibrato_active=tail_vibrato=0;}
static void articulation_reset(void){
 articulation_panic();articulation_frames=articulation_samples=vibrato_position=vibrato_reverse=vibrato_ramp=vibrato_delay_ms=vibrato_start_frame=0;
 adsr_ms[0]=adsr_ms[1]=0;adsr_ms[2]=100;adsr_ms[3]=100;
 adsr_knobs[0]=adsr_knobs[1]=0;adsr_knobs[2]=127;adsr_knobs[3]=20;
}
static void articulation_vibrato_tick(unsigned enabled,unsigned type,unsigned rate,unsigned depth){
 unsigned delay_frames=vibrato_delay_ms*NSX_RATE/(128u*1000u);
 unsigned rate_q16=art_scalar(rate),depth_q16=art_scalar(depth);
 if(!enabled||!rate_q16||!depth_q16||articulation_frames<delay_frames||!art_curve_lengths[type]){
  vibrato_active=vibrato_position=vibrato_reverse=vibrato_ramp=0;vibrato_start_frame=articulation_frames;articulation_vibrato_q16=0;return;
 }
 if(!vibrato_active){vibrato_active=1;vibrato_position=vibrato_reverse=vibrato_ramp=0;}
 // Original 0409330e: fractional interpolation, 60 control-frame onset ramp,
 // mirrored endpoint traversal and the piecewise rate transform around 0.5.
 unsigned n=art_curve_lengths[type],index=vibrato_position>>16;
 int neighbor=vibrato_reverse?(int)index-1:(int)index+1;
 if(neighbor<0)neighbor=0;if((unsigned)neighbor>=n)neighbor=(int)n-1;
 int32_t a=(int32_t)r32(art_curves[type]+index*4),b=(int32_t)r32(art_curves[type]+(unsigned)neighbor*4);
 int32_t slope=vibrato_reverse?a-b:b-a;
 int32_t value=a+art_multiply(slope,(int32_t)(vibrato_position&65535u));
 vibrato_ramp=articulation_frames-vibrato_start_frame;
 unsigned ramp=vibrato_ramp<60?vibrato_ramp*0x444u:65536u;
 articulation_vibrato_q16=art_multiply(art_multiply(value,(int32_t)ramp),(int32_t)(depth_q16*2));
 unsigned step=rate_q16<32768?rate_q16+32768:rate_q16*2,limit=(n-1)*65536u;
 if(!vibrato_reverse){if(vibrato_position+step>=limit){vibrato_position=limit;vibrato_reverse=1;}else vibrato_position+=step;}
 else {vibrato_position=vibrato_position>step?vibrato_position-step:0;if((vibrato_position>>16)==0)vibrato_reverse=0;}
}
void nsx_adsr(unsigned parameter,unsigned value){
 if(parameter>3)return;unsigned max=parameter==2?100:parameter==3?4000:2000;if(value>max)value=max;adsr_ms[parameter]=value;
 if(parameter==0&&envelope.stage==NSX_ENV_ATTACK)envelope_begin(NSX_ENV_ATTACK,32768,value*NSX_RATE/1000u);
 if(parameter==1&&envelope.stage==NSX_ENV_DECAY)envelope_begin(NSX_ENV_DECAY,adsr_ms[2]*32768u/100u,value*NSX_RATE/1000u);
 if(parameter==2&&(envelope.stage==NSX_ENV_DECAY||envelope.stage==NSX_ENV_SUSTAIN))envelope_begin(NSX_ENV_DECAY,value*32768u/100u,128);
 if(parameter==3&&envelope.stage==NSX_ENV_RELEASE){
  envelope_begin(NSX_ENV_RELEASE,0,value*NSX_RATE/1000u);
  if(!value){articulation_panic();gain_q15=0;for(unsigned i=0;i<3;i++)grains[i].active=0;}
  else if(release_unit&&current_unit<unit_count){unsigned end=r32(unit(current_unit)+4)*256u,remaining=end>cursor_q8?end-cursor_q8:0;source_speed=remaining/(value*NSX_RATE/1000u);if(!source_speed)source_speed=1;}
 }
}
void nsx_adsr_knob(unsigned parameter,int delta){
 if(parameter>3)return;unsigned value=(unsigned)clip((int)adsr_knobs[parameter]+delta,0,127);adsr_knobs[parameter]=value;
 nsx_adsr(parameter,parameter==2?(value*100u+63)/127u:value*value*(parameter==3?4000u:2000u)/(127u*127u));
}
