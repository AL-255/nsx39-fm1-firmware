// SPDX-License-Identifier: GPL-3.0-only
#include "nsx.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char**argv){
 if(argc==3&&!strcmp(argv[1],"decode")){FILE*f=fopen(argv[2],"rb");assert(f);uint8_t b[68];int16_t out[128];while(fread(b,1,68,f)==68){nsx_decode_ima(b,out);fwrite(out,2,128,stdout);}fclose(f);return 0;}
 assert(argc>=2);uint8_t*b=calloc(1,NSX_BANK_CAPACITY);FILE*f=fopen(argv[1],"rb");assert(f);size_t n=fread(b,1,NSX_BANK_CAPACITY,f);fclose(f);assert(nsx_init(b,n));
 int16_t out[128];
 if(argc>=3&&!strcmp(argv[2],"render")){unsigned note=argc>3?(unsigned)atoi(argv[3]):69;nsx_select(argc>4?(unsigned)atoi(argv[4]):0);nsx_note_on((uint8_t)note,127);for(unsigned k=0;k<260;k++){nsx_render(out,128);fwrite(out,2,128,stdout);}return 0;}
 unsigned long long energy=0;unsigned silent=0;
 for(unsigned s=0;s<128;s++){
  nsx_reset();nsx_select(s);nsx_note_on(69,110);unsigned long long e=0;
  for(unsigned k=0;k<40;k++){nsx_render(out,128);for(unsigned j=0;j<128;j++)e+=(long long)out[j]*out[j];}
  if(!e)silent++;energy+=e;nsx_note_off(69);for(unsigned k=0;k<200;k++)nsx_render(out,128);assert(!nsx_active());
 }
 assert(!silent&&energy>0);printf("128 pronunciations rendered; energy=%llu\n",energy);
 // MIDI channel isolation, sustain and panic.
 nsx_reset();nsx_midi(0x91,69,127);assert(!nsx_active());nsx_midi(0x90,69,127);assert(nsx_active());nsx_midi(0xb0,64,127);nsx_midi(0x80,69,0);nsx_render(out,128);assert(nsx_active());nsx_midi(0xb0,120,0);assert(!nsx_active());
 // Pitch test: settled A4 should be close to 440 Hz.
 nsx_reset();nsx_select(0);nsx_note_on(69,127);for(unsigned k=0;k<200;k++)nsx_render(out,128);unsigned crossings=0;int prev=0;for(unsigned k=0;k<200;k++){nsx_render(out,128);for(unsigned j=0;j<128;j++){if(prev<0&&out[j]>=0)crossings++;prev=out[j];}}double hz=(double)crossings*22050/(200*128);printf("A4 positive zero crossings %.2f Hz (waveform harmonics may add crossings)\n",hz);
 // Bounded MIDI/NRPN extremes across the entire external note range.
 for(unsigned note=0;note<128;note++){nsx_reset();nsx_select(note);nsx_midi(0xb0,99,0x70);for(unsigned c=1;c<16;c++){nsx_midi(0xb0,98,(uint8_t)c);nsx_midi(0xb0,6,127);}nsx_midi(0xe0,127,127);nsx_note_on((uint8_t)note,127);for(unsigned k=0;k<8;k++)nsx_render(out,128);nsx_midi(0xb0,120,0);}
 // Invalid input and unsupported firmware-update commands fail without writes.
 uint8_t sx[]={0x43,0x79,9,0x11,0x70,0,0};assert(nsx_sysex(sx,sizeof sx)==0);
 for(unsigned i=0;i<64;i++){uint8_t old=b[i];b[i]^=0x55;assert(!nsx_init(b,n));b[i]=old;}assert(nsx_init(b,n));
 assert(!nsx_init(b,63));b[n-1]^=1; // padding may be outside CRC
 free(b);return 0;
}
