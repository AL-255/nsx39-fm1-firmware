// SPDX-License-Identifier: GPL-3.0-only
// Included by main.c; canvas is 240 x 240, with 12-pixel diagnostic type.
static void number(char*out,unsigned v){char b[12];unsigned n=0;do{b[n++]=(char)('0'+v%10);v/=10;}while(v);unsigned i=0;while(n)out[i++]=b[--n];out[i]=0;}
static void put_text(unsigned x,unsigned y,const char*s,uint16_t color,unsigned scale){
 while(*s&&x+8*scale<=240){unsigned c=(unsigned char)*s++;if(c<32||c>127)c='?';for(unsigned row=0;row<14;row++)for(unsigned bit=0;bit<8;bit++)if(ui_font[c-32][row]&(1u<<bit))for(unsigned a=0;a<scale;a++)for(unsigned b=0;b<scale;b++)if(y+row*scale+b<240)canvas[(y+row*scale+b)*240+x+bit*scale+a]=color;x+=8*scale;}
}
static void label_num(unsigned x,unsigned y,const char*label,unsigned n){char b[12];put_text(x,y,label,0xad55,1);number(b,n);put_text(x+str_len(label)*8,y,b,0xffff,1);}
static void ui_line(int x,int y,int xx,int yy,uint16_t color){int dx=xx>x?xx-x:x-xx,dy=yy>y?yy-y:y-yy,sx=x<xx?1:-1,sy=y<yy?1:-1,error=dx-dy;for(;;){if(x>=0&&x<240&&y>=0&&y<240)canvas[y*240+x]=color;if(x==xx&&y==yy)break;int e=error*2;if(e>-dy){error-=dy;x+=sx;}if(e<dx){error+=dx;y+=sy;}}}
static const char*const settings_names[]={"Octave","Labels","Mode","Syllable","Attack ms","Decay ms","Sustain %","Release ms","Auto pitch","Auto dynamics","Vibrato type","Vibrato rate","Vibrato depth","Vib delay ms","Glide","Noise","Connection","Hold","Reset all"};
static const char*const types[]={"EXTREME","FAST","NORMAL","SLIGHT"};
static const char*const scripts[]={"HIRAGANA","KATAKANA","ROMAJI"};
static const char*const connections[]={"50ms","MIN","VEL"};
static void settings_screen(void){
 put_text(8,4,"SETTINGS",UI_ACCENT,1);put_text(136,4,"OCT-/+: back",0xad55,1);
 unsigned first=settings_item>4?settings_item-4:0;if(first+9>SET_COUNT)first=SET_COUNT>9?SET_COUNT-9:0;
 for(unsigned row=0;row<9&&first+row<SET_COUNT;row++){
  unsigned item=first+row,y=29+row*20;uint16_t color=item==settings_item?UI_ACCENT:0xad55;
  if(item==settings_item)put_text(0,y,">",UI_ACCENT,1);put_text(12,y,settings_names[item],color,1);
  const char*text=0;unsigned value=instrument_value(item);
  char mode_text[12];if(item==SET_LYRIC){if(value==0)text="SELECT";else if(value==1)text="DOREMI";else{mode_text[0]='L';mode_text[1]='0'+(value-2)/10;mode_text[2]='0'+(value-2)%10;mode_text[3]=0;text=mode_text;}}
  if(item==SET_SCRIPT)text=scripts[value];if(item==SET_VIBTYPE)text=types[value];if(item==SET_JOIN)text=connections[value];
  if(item==SET_PITCH||item==SET_DYNAMICS||item==SET_HOLD)text=value?"ON":"OFF";
  if(item==SET_RESET)text=reset_armed?"AGAIN >":"TURN >";
  if(text)put_text(152,y,text,color,1);else label_num(176,y,"",value);
 }
 put_text(8,212,"1: select   2: change",0xad55,1);
 put_text(8,227,save_status<0?(save_status==-2?"Storage occupied":"Save failed"):settings_dirty?(settings_pending?"Saves when keys released":"Not saved"):"Saved",save_status<0?0xfca0:UI_ACCENT,1);
}
static void screen(void){
 nsx_debug_t state;nsx_art_debug_t art;performance_state_t song;int16_t wave[102];
 __asm__ volatile("cli":::"memory");nsx_debug(&state);nsx_art_debug(&art);performance_snapshot(&song);unsigned w=scope_w;for(unsigned i=0;i<102;i++)wave[i]=scope[(w-102+i)&511];__asm__ volatile("sti":::"memory");
 for(unsigned i=0;i<240*240;i++)canvas[i]=0x0000;
 if(settings_open)settings_screen();
 else if(!voice_ready){put_text(8,40,"NO VALID VOICE BANK",0xfca0,1);put_text(8,64,"Generate with your dump",0xffff,1);}
 else{
  unsigned script=instrument_script();put_text(8,5,"Last played",0xad55,1);put_text(128,5,"Next",0xad55,1);
  const char*last=song.has_last?ui_syllable(song.last,script):"—";unsigned div=ui_label_width(last,1)>112?2:1;ui_label(canvas,8,66,last,div,UI_ACCENT);
  if(song.mode==1)put_text(128,43,"By pitch",UI_ACCENT,1);
  unsigned x=128;for(unsigned n=0;n<(song.mode==1?0u:song.mode>=2?32u:1u);n++){const char*s=ui_syllable(performance_upcoming(n),script);unsigned width=ui_label_width(s,2);if(!width||x+width>236)break;ui_label(canvas,(int)x,57,s,2,UI_ACCENT);x+=width+2;}
  if(song.mode<2)put_text(8,77,song.mode?"DOREMI":"SELECT",UI_ACCENT,1);else{char mode_label[]={'L',(char)('0'+(song.mode-2)/10),(char)('0'+(song.mode-2)%10),0};put_text(8,77,mode_label,UI_ACCENT,1);}if(song.mode>=2)label_num(48,77,"STEP",song.cursor+1);if(song.mode>=2)label_num(96,77,"/",song.length);label_num(136,77,"N",state.note);label_num(176,77,"Oct",instrument_octave());
  put_text(8,98,"VIB",0xad55,1);put_text(40,98,art.vibrato_depth?types[art.vibrato_type]:"OFF",UI_ACCENT,1);label_num(112,98,"D",art.vibrato_depth);label_num(176,98,"R",art.vibrato_rate);
  label_num(8,116,"Delay ",art.vibrato_delay);label_num(136,116,"Glide ",art.portamento);
  put_text(8,134,art.auto_pitch?"Pitch ON":"Pitch OFF",UI_ACCENT,1);put_text(96,134,art.auto_dynamics?"Dyn ON":"Dyn OFF",UI_ACCENT,1);
  int db=((int)art.noise-64)*24/(art.noise<=64?64:63);put_text(168,134,db<0?"N-":"N+",0xad55,1);label_num(184,134,"",(unsigned)(db<0?-db:db));
  put_text(8,152,"Join",0xad55,1);put_text(48,152,connections[art.connection],UI_ACCENT,1);put_text(96,152,art.hold?"Hold ON":"Hold OFF",UI_ACCENT,1);if(art.release_unit&&art.stage==NSX_ENV_RELEASE)put_text(176,152,"Tail",UI_ACCENT,1);
  label_num(8,170,"A",art.adsr[0]);label_num(64,170,"D",art.adsr[1]);label_num(120,170,"S",art.adsr[2]);label_num(176,170,"R",art.adsr[3]);
  static const char*const stages[]={"IDLE","ATTACK","DECAY","SUSTAIN","RELEASE"};put_text(8,188,stages[art.stage],UI_ACCENT,1);put_text(130,188,"WAVE",0xad55,1);
  unsigned total=art.adsr[0]+art.adsr[1]+500+art.adsr[3],ax=13+art.adsr[0]*85/total,dx=ax+3+art.adsr[1]*85/total,sx=dx+3+500*85/total,sy=234-art.adsr[2]*30/100;
  ui_line(10,234,(int)ax,204,art.stage==1?UI_ACCENT:0x5b6c);ui_line((int)ax,204,(int)dx,(int)sy,art.stage==2?UI_ACCENT:0x5b6c);ui_line((int)dx,(int)sy,(int)sx,(int)sy,art.stage==3?UI_ACCENT:0x5b6c);ui_line((int)sx,(int)sy,110,234,art.stage==4?UI_ACCENT:0x5b6c);
  unsigned px=110;if(art.stage==1)px=10+(ax-10)*art.progress/32768;else if(art.stage==2)px=ax+(dx-ax)*art.progress/32768;else if(art.stage==3)px=(dx+sx)/2;else if(art.stage==4)px=sx+(110-sx)*art.progress/32768;
  int py=234-(int)(art.level*30/32768);if(art.stage)for(int y=py-1;y<=py+1;y++)for(unsigned a=px-1;a<=px+1;a++)if(y>=202&&y<237)canvas[(unsigned)y*240+a]=0xffff;
  int low=wave[0],high=wave[0];for(unsigned i=1;i<102;i++){if(wave[i]<low)low=wave[i];if(wave[i]>high)high=wave[i];}
  int previous=high-low>=64?235-((int)wave[0]-low)*32/(high-low):219;
  for(unsigned i=0;i<102;i++){int y=high-low>=64?235-((int)wave[i]-low)*32/(high-low):219;ui_line((int)i+130,previous,(int)i+130,y,UI_ACCENT);previous=y;}
 }
 fm1_led_key(2,art.noise!=64);fm1_led_key(3,art.auto_pitch);fm1_led_key(4,art.auto_dynamics);fm1_led_key(5,art.vibrato_depth!=0);fm1_led_key(6,art.vibrato_type!=2);fm1_led_key(7,art.vibrato_rate!=64);fm1_led_key(8,art.vibrato_delay!=0);fm1_led_key(10,art.connection!=1);fm1_led_key(11,art.portamento!=0);fm1_led_key(12,art.hold);fm1_led_key(13,art.vibrato_depth!=0);
 lcd_sync();for(unsigned i=0;i<240*240;i++)canvas[i]=(uint16_t)((canvas[i]>>8)|(canvas[i]<<8));lcd_blit(0,0,240,240,canvas);lcd_sync();
}
