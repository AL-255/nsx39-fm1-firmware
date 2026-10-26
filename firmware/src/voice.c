// SPDX-License-Identifier: GPL-3.0-only
// Native recorded-unit vocal renderer. This is a new implementation of the
// documented signal flow, not execution of Yamaha's frontend or a bit-exact port.
#include "nsx.h"
#include "dsp_tables.h"
static const uint8_t *vb;
static uint32_t vb_size;
static uint16_t unit_count, name_count;
static int bank_ok;
static uint32_t r32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint16_t r16(const uint8_t *p){return (uint16_t)(p[0]|p[1]<<8);}
static int bound(uint32_t a,uint32_t n){return a<=vb_size&&n<=vb_size-a;}
static uint32_t bank_crc(const uint8_t *p,unsigned n){uint32_t c=~0u;unsigned offset=0;while(n--){uint8_t b=*p++;if(offset>=12&&offset<16)b=0;offset++;c^=b;for(unsigned k=0;k<8;k++)c=(c>>1)^((c&1)?0xedb88320u:0);}return ~c;}
static int clip(int x,int lo,int hi){return x<lo?lo:x>hi?hi:x;}
static int equal(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static const int16_t ima_step[89]={7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
static const int8_t ima_index[8]={-1,-1,-1,-1,2,4,6,8};
void nsx_decode_ima(const uint8_t *b,int16_t *out){int p=(int16_t)r16(b),idx=clip(b[2],0,88);out[0]=(int16_t)p;for(unsigned i=1;i<128;i++){int c=(b[4+((i-1)>>1)]>>(((i-1)&1)*4))&15,s=ima_step[idx],d=s>>3;if(c&4)d+=s;if(c&2)d+=s>>1;if(c&1)d+=s>>2;p=clip(p+((c&8)?-d:d),-32768,32767);idx=clip(idx+ima_index[c&7],0,88);out[i]=(int16_t)p;}}
static const uint8_t *unit(unsigned id){return vb+64+id*40;}
static const char *name(unsigned id){return (const char *)(vb+r32(vb+24)+id*8);}
static int name_id(const char *s){for(unsigned i=0;i<name_count;i++)if(equal(name(i),s))return (int)i;return -1;}
static int find_unit(unsigned source,unsigned target,unsigned kind,unsigned variant){for(unsigned i=0;i<unit_count;i++){const uint8_t*d=unit(i);if(d[24]==source&&d[25]==target&&d[27]==kind&&(!kind?d[26]==variant:1))return (int)i;}return -1;}
static int16_t cache[2][NSX_MAX_UNIT];
static int16_t steady[36][64];
static int cache_id[2];
static uint8_t syllables[NSX_MAX_SYLLABLES][8],syllable_n[NSX_MAX_SYLLABLES];
static unsigned syllable_count,lyric_cursor,selected,solfege,fixed_syllable=1,playing_syllable;
static uint8_t phrase[8];
static unsigned phrase_len,phrase_pos,current_name,current_slot,current_unit,steady_mode;
static uint32_t cursor_q8,source_speed,blend_age,random_state;
static int steady_a,steady_b;
static int32_t next_grain_q16,period_q16,current_period_q16;
static unsigned current_note,velocity,gate,pedal,release_pending,age;
static int32_t gain_q15,volume=100,expression=127,bend=8192,bend_range=16,tune=8192;
static uint8_t nrpn_msb=127,nrpn_lsb=127,rpn_msb=127,rpn_lsb=127;
static uint8_t controls[16];
static int32_t dc_x,dc_y,last_sample;
static uint32_t vib_phase;
typedef struct {int active,slot,unit,steady;int32_t pos_q16;unsigned elapsed,length;} grain_t;
static grain_t grains[3];

const char *nsx_pronunciation(unsigned i){if(!bank_ok||i>=128)return "";return (const char*)(vb+r32(vb+r32(vb+28)+i*4));}
unsigned nsx_selected(void){return selected;}
const char *nsx_phoneme(unsigned id){return bank_ok&&id<name_count?name(id):"-";}
void nsx_sequence(unsigned on){fixed_syllable=!on;}
void nsx_debug(nsx_debug_t*out){out->selected=playing_syllable;out->note=current_note;out->velocity=velocity;out->active=nsx_active();out->unit=current_unit;out->source=current_unit<unit_count?unit(current_unit)[24]:255;out->target=current_unit<unit_count?unit(current_unit)[25]:255;out->steady=steady_mode;}
unsigned nsx_active(void){return gate||gain_q15>0;}
void nsx_select(unsigned i){selected=i&127u;lyric_cursor=selected;}
void nsx_solge(unsigned on){solfege=on!=0;}
static void parse_lyrics(const char *s,int append){
 unsigned n=append?syllable_count:0,k=0;char token[8];unsigned t=0;uint8_t list[8];
 for(unsigned i=0;i<NSX_LYRIC_BYTES;i++){char c=s[i];if(c==' '||c==','||c==0){if(t){token[t]=0;int id=name_id(token);if(id>=0&&k<8)list[k++]=(uint8_t)id;t=0;}if(c==','||!c){if(n<128){syllable_n[n]=(uint8_t)k;for(unsigned j=0;j<k;j++)syllables[n][j]=list[j];n++;}k=0;}if(!c)break;}else if(t<7)token[t++]=c;}
 syllable_count=n;lyric_cursor=0;
}
static int decode_unit(unsigned id,unsigned slot){const uint8_t*d=unit(id);unsigned len=r32(d+4),blocks=r16(d+22);int16_t tmp[128];if(len>NSX_MAX_UNIT)return 0;for(unsigned i=0;i<blocks;i++){nsx_decode_ima(vb+r32(d)+i*68,tmp);unsigned n=len-i*128;if(n>128)n=128;for(unsigned k=0;k<n;k++)cache[slot][i*128+k]=tmp[k];}cache_id[slot]=(int)id;return 1;}
static void enter_unit(unsigned next){
 int id=find_unit(current_name,next,1,0);if(id<0)id=find_unit((unsigned)name_id("Sil"),next,1,0);
 current_name=next;cursor_q8=0;steady_mode=0;
 if(id>=0){current_slot^=1u;decode_unit((unsigned)id,current_slot);current_unit=(unsigned)id;}
 else {id=find_unit(next,255,0,0);if(id>=0){current_unit=(unsigned)id;steady_mode=1;steady_a=id;steady_b=id;blend_age=0;}else current_unit=0xffffu;}
}
static void next_source(void){
 if(phrase_pos<phrase_len){enter_unit(phrase[phrase_pos++]);return;}
 int id=find_unit(current_name,255,0,0);if(id>=0&&gate){current_unit=(unsigned)id;steady_a=id;steady_b=id;steady_mode=1;cursor_q8=blend_age=0;}
 else {current_unit=0xffffu;gate=0;}
}
static int32_t source_period(void){if(current_unit>=unit_count)return 56<<16;const uint8_t*d=unit(current_unit);unsigned n=r16(d+20),i=(cursor_q8>>8)/128;if(i>=n)i=n-1;int32_t p=(int32_t)r32(vb+r32(d+16)+4*i);if(p<=0)return p;return clip(p,20<<16,149<<16);}
static int16_t source_sample(int slot,unsigned id,int sustain,int pos){
 if(id>=unit_count)return 0;int len=(int)r32(unit(id)+4);
 if(sustain){if(len<=0)return 0;pos%=len;if(pos<0)pos+=len;int a=steady_a,b=steady_b;if(a<0||a>=36)a=(int)id;if(b<0||b>=36)b=a;int la=(int)r32(unit((unsigned)a)+4),lb=(int)r32(unit((unsigned)b)+4);unsigned w=blend_age>1800?1800:blend_age;return (int16_t)((steady[a][pos%la]*(int)(1800-w)+steady[b][pos%lb]*(int)w)/1800);}
 if(pos<0||pos>=len||cache_id[slot]!=(int)id)return 0;return cache[slot][pos];
}
static int32_t get_period(void){
 // cents offset, linearly interpolated semitone period table: bounded integer math.
 int cents=(int)current_note*100+(bend-8192)*bend_range*100/8192+(tune-8192)*100/8192;
 if(controls[1])cents+=(int)(controls[1]-64)/8; // automatic pitch onset gesture
 unsigned delay=(unsigned)controls[7]*173u;
 if(age>delay){int32_t s=sin_q15[(vib_phase>>24)&255];cents+=s*(int)controls[5]*150/(32768*127);}
 if(controls[2])cents+=(random_state&1)?1:-1;
 cents=clip(cents,0,12699);unsigned note=(unsigned)cents/100,frac=(unsigned)cents%100;
 return (int32_t)(note_period[note]+((int32_t)note_period[note+1]-(int32_t)note_period[note])*(int)frac/100);
}
void nsx_note_on(uint8_t note,uint8_t vel){
 if(!bank_ok)return;if(!vel){nsx_note_off(note);return;}
 current_note=note;velocity=vel;gate=1;release_pending=0;age=0;
 if(solfege){static const uint8_t sol[12]={0,0,1,1,2,3,3,4,4,5,5,6};unsigned ix[7]={25,40,86,0,4,104,2};unsigned s=sol[note%12];selected=ix[s];parse_lyrics(nsx_pronunciation(selected),0);}
 unsigned n=syllable_count?(fixed_syllable?selected:lyric_cursor)%syllable_count:0;playing_syllable=n;phrase_len=syllable_n[n];for(unsigned k=0;k<phrase_len;k++)phrase[k]=syllables[n][k];phrase_pos=0;
 lyric_cursor=syllable_count?(lyric_cursor+1)%syllable_count:0;if(!fixed_syllable)selected=n;
 if(!gain_q15)current_name=(unsigned)name_id("Sil");
 next_source();period_q16=get_period();if(!current_period_q16||!controls[8])current_period_q16=period_q16;
 source_speed=(controls[12]>=2)?(unsigned)(128+(127-vel)*3):256;
 next_grain_q16=0;
}
void nsx_note_off(uint8_t note){if(note!=current_note)return;if(pedal){release_pending=1;return;}gate=0;}
void nsx_reset(void){
 gate=pedal=release_pending=0;gain_q15=0;for(unsigned i=0;i<3;i++)grains[i].active=0;
 volume=100;expression=127;bend=8192;bend_range=16;tune=8192;nrpn_msb=nrpn_lsb=rpn_msb=rpn_lsb=127;
 for(unsigned i=0;i<16;i++)controls[i]=0;controls[4]=64;controls[12]=0;selected=lyric_cursor=solfege=0;current_name=(unsigned)name_id("Sil");current_unit=0xffff;fixed_syllable=1;playing_syllable=0;dc_x=dc_y=last_sample=0;age=vib_phase=0;current_period_q16=0;random_state=1;
 if(bank_ok){char all[NSX_LYRIC_BYTES];unsigned p=0;for(unsigned i=0;i<128;i++){const char*s=nsx_pronunciation(i);while(*s&&p<NSX_LYRIC_BYTES-2)all[p++]=*s++;if(i<127)all[p++]=',';}all[p]=0;parse_lyrics(all,0);}
}
void nsx_midi(uint8_t st,uint8_t a,uint8_t b){
 if((st&15)!=0)return;switch(st&0xf0){case 0x90:nsx_note_on(a,b);break;case 0x80:nsx_note_off(a);break;case 0xe0:bend=a+(b<<7);break;
 case 0xb0:switch(a){case 1:controls[5]=b;break;case 7:volume=b;break;case 11:expression=b;break;case 64:pedal=b>=64;if(!pedal&&release_pending){gate=0;release_pending=0;}break;
 case 99:nrpn_msb=b;rpn_msb=rpn_lsb=127;break;case 98:nrpn_lsb=b;break;case 101:rpn_msb=b;nrpn_msb=nrpn_lsb=127;break;case 100:rpn_lsb=b;break;
 case 6:if(nrpn_msb==0x70&&nrpn_lsb<16){controls[nrpn_lsb]=b;if(nrpn_lsb==9)nsx_select(b);}else if(rpn_msb==0&&rpn_lsb==0)bend_range=clip(b,0,24);else if(rpn_msb==0&&rpn_lsb==1)tune=b<<7;break;
 case 38:if(rpn_msb==0&&rpn_lsb==1)tune=(tune&0x3f80)|b;break;
 case 120:gate=0;gain_q15=0;for(unsigned k=0;k<3;k++)grains[k].active=0;break;case 121:nsx_reset();break;case 123:gate=0;break;default:break;}break;default:break;}
}
int nsx_sysex(const uint8_t *p,size_t n){
 if(!bank_ok||n<6||p[0]!=0x43||p[1]!=0x79||p[2]!=9)return 0;
 if(p[3]==0&&p[4]==0x50&&(p[5]==0x10||p[5]==0x11)){
  if(n>NSX_LYRIC_BYTES||p[n-1]!=0)return -1;for(size_t i=6;i<n;i++)if(p[i]>=128)return -1;
  parse_lyrics((const char*)p+6,p[5]==0x11);return 1;
 }
 // Generalplus controller pronunciation selection. Firmware-update commands
 // 70/71 are intentionally never routed to any flash write.
 if(p[3]==0x11&&p[4]==0x0a&&n>=7){nsx_select(p[5]);return 1;}
 return 0;
}
static void grain_start(int32_t sp){
 unsigned k=0;while(k<3&&grains[k].active)k++;if(k==3)k=0;
 int32_t ps=sp>0?sp:56<<16,po=current_period_q16;unsigned length=(unsigned)((2*(ps<po?ps:po))>>16);length=(unsigned)clip((int)length,16,512);
 int center=(int)(cursor_q8>>8);if(sp>0){int r=(ps>>16)/2,best=-32769,where=center;for(int j=-r;j<=r;j++){int x=source_sample((int)current_slot,current_unit,(int)steady_mode,center+j);if(x>best){best=x;where=center+j;}}center=where;}
 grain_t*g=&grains[k];g->active=1;g->slot=(int)current_slot;g->unit=(int)current_unit;g->steady=(int)steady_mode;g->pos_q16=(center-(int)length/2)*65536;g->elapsed=0;g->length=length;
}
void nsx_render(int16_t *out,unsigned count){
 for(unsigned i=0;i<count;i++){
  if(!bank_ok){out[i]=0;continue;}
  if(current_unit<unit_count&&!steady_mode&&((cursor_q8>>8)>=r32(unit(current_unit)+4)))next_source();
  int32_t sp=source_period();period_q16=get_period();if(controls[8])current_period_q16+=(period_q16-current_period_q16)/(1+(int)controls[8]*8);else current_period_q16=period_q16;
  vib_phase+=(uint32_t)(300000u+(unsigned)controls[4]*7000u);age++;random_state=random_state*1664525u+1013904223u;
  int32_t mix=0;
  if(current_unit<unit_count&&(gate||gain_q15)){
   if(sp<=0&&!steady_mode){mix=source_sample((int)current_slot,current_unit,0,(int)(cursor_q8>>8));}
   else {
    if(next_grain_q16<=0){grain_start(sp);next_grain_q16+=current_period_q16;}
    next_grain_q16-=65536;
    for(unsigned j=0;j<3;j++){grain_t*g=&grains[j];if(!g->active)continue;int pos=g->pos_q16>>16;unsigned frac=(unsigned)g->pos_q16&65535u;int a=source_sample(g->slot,(unsigned)g->unit,g->steady,pos),b=source_sample(g->slot,(unsigned)g->unit,g->steady,pos+1);int s=a+((b-a)*(int)frac>>16);unsigned w=g->elapsed*256/g->length;mix+=s*hann_q15[w]>>15;g->pos_q16+=65536;if(++g->elapsed>=g->length)g->active=0;}
   }
   cursor_q8+=source_speed;
   if(steady_mode&&++blend_age>=1800){blend_age=0;steady_a=steady_b;int base=find_unit(current_name,255,0,0);steady_b=base+(int)((random_state>>16)%3);if(steady_b>=36)steady_b=base;cursor_q8%=r32(unit(current_unit)+4)*256;}
  }
  int target=gate?((int)velocity*volume*expression/127):0;
  if(controls[2]&&gate)target=target*(int)(age<2205?age:2205)/2205;
  gain_q15+=clip(target-gain_q15,-64,128);
  int y=mix-dc_x+(dc_y*32723>>15);dc_x=mix;dc_y=clip(y,-65536,65535);
  if(controls[10]&&gate)y+=(int32_t)(random_state>>20)*controls[10]/1024-2*controls[10];
  y=(y*gain_q15)>>14; // Q14 gain, leaves headroom
  last_sample=clip(y,-32768,32767);out[i]=(int16_t)last_sample;
 }
}
int nsx_init(const uint8_t *bank,size_t size){
 bank_ok=0;if(!bank||size<64||bank[0]!='N'||bank[1]!='S'||bank[2]!='X'||bank[3]!='V'||r32(bank+4)!=1)return 0;
 vb=bank;vb_size=r32(vb+8);if(vb_size>size||vb_size>NSX_BANK_CAPACITY||vb_size<64||r32(vb+16)!=449||r32(vb+20)>64||!r32(vb+20)||r32(vb+36)!=22050||r32(vb+40)!=128||r32(vb+44)!=68||bank_crc(vb,vb_size)!=r32(vb+12))return 0;
 unit_count=449;name_count=(uint16_t)r32(vb+20);if(!bound(64,449*40)||!bound(r32(vb+24),name_count*8)||!bound(r32(vb+28),512)||!bound(r32(vb+32),32))return 0;
 for(unsigned i=0;i<name_count;i++)if(name(i)[7]!=0)return 0;
 for(unsigned i=0;i<128;i++){uint32_t p=r32(vb+r32(vb+28)+i*4);if(!bound(p,1))return 0;unsigned n=0;while(bound(p+n,1)&&vb[p+n]&&n<63)n++;if(!bound(p+n,1)||n>=63)return 0;}
 for(unsigned i=0;i<unit_count;i++){const uint8_t*d=unit(i);unsigned len=r32(d+4),blocks=r16(d+22),n=r16(d+20);if(!len||len>NSX_MAX_UNIT||blocks!=(len+127)/128||!n||!bound(r32(d),d[27]?blocks*68:len*2)||!bound(r32(d+16),n*4)||!bound(r32(d+28),n*4)||d[24]>=name_count||(d[25]!=255&&d[25]>=name_count)||d[27]>1||d[36]!=(d[27]?1:0)||r32(d+8)>len+1)return 0;
  if(i<36&&(d[27]||len>64))return 0;
  if(d[27])for(unsigned j=0;j<blocks;j++){const uint8_t*b=vb+r32(d)+j*68;if(b[2]>88||b[3])return 0;}
 }
 for(unsigned i=0;i<36;i++){for(unsigned k=0;k<r32(unit(i)+4);k++)steady[i][k]=(int16_t)r16(vb+r32(unit(i))+k*2);}
 cache_id[0]=cache_id[1]=-1;bank_ok=1;nsx_reset();return 1;
}
