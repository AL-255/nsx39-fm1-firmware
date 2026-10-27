// SPDX-License-Identifier: GPL-3.0-only
// Two-bit antialiasing, with per-glyph RLE where it saves flash space.
#include "nsx.h"
#include "label_font.h"
static uint8_t glyph_mask[48*48];
const char *ui_syllable(unsigned id,unsigned script){return syllable_labels[id&127u][script%3];}
static unsigned utf8_next(const char **p){const uint8_t*s=(const uint8_t*)*p;unsigned c=*s++;if(c>=0xe0){c=((c&15)<<12)|((s[0]&63)<<6)|(s[1]&63);s+=2;}else if(c>=0xc0){c=((c&31)<<6)|(s[0]&63);s++;}*p=(const char*)s;return c;}
unsigned ui_label_width(const char*text,unsigned divisor){unsigned width=0;while(*text){unsigned c=utf8_next(&text);for(unsigned i=0;i<sizeof label_glyphs/sizeof label_glyphs[0];i++)if(label_glyphs[i].codepoint==c){width+=label_glyphs[i].advance/divisor;break;}}return width;}
static uint16_t label_blend(uint16_t fg,unsigned q){unsigned bg=0x0000;return (uint16_t)(((((fg>>11)*q+(bg>>11)*(3-q))/3)<<11)|(((((fg>>5)&63)*q+((bg>>5)&63)*(3-q))/3)<<5)|(((fg&31)*q+(bg&31)*(3-q))/3));}
void ui_label(uint16_t*canvas,int x,int baseline,const char*text,unsigned divisor,uint16_t color){
 uint16_t colors[4];for(unsigned i=0;i<4;i++)colors[i]=label_blend(color,i);
 while(*text){unsigned c=utf8_next(&text),lo=0,hi=sizeof label_glyphs/sizeof label_glyphs[0];
  while(lo<hi){unsigned m=(lo+hi)/2;if(label_glyphs[m].codepoint<c)lo=m+1;else hi=m;}
  if(lo>=sizeof label_glyphs/sizeof label_glyphs[0]||label_glyphs[lo].codepoint!=c)continue;
  const label_glyph_t*g=&label_glyphs[lo];const uint8_t*p=label_pixels+g->offset;unsigned n=g->width*g->height;
  if(g->codec){unsigned i=0;while(i<n){unsigned b=*p++,run=(b&63)+1;while(run--&&i<n)glyph_mask[i++]=(uint8_t)(b>>6);}}
  else for(unsigned i=0;i<n;i++)glyph_mask[i]=(p[i>>2]>>((i&3)*2))&3;
  unsigned w=(g->width+divisor-1)/divisor,h=(g->height+divisor-1)/divisor;
  for(unsigned y=0;y<h;y++)for(unsigned a=0;a<w;a++){
   unsigned q=0;for(unsigned dy=0;dy<divisor;dy++)for(unsigned dx=0;dx<divisor;dx++){unsigned sx=a*divisor+dx,sy=y*divisor+dy;if(sx<g->width&&sy<g->height)q+=glyph_mask[sy*g->width+sx];}
   q=(q+divisor*divisor/2)/(divisor*divisor);int xx=x+g->left/(int)divisor+(int)a,yy=baseline+g->top/(int)divisor+(int)y;
   if(q&&xx>=0&&xx<240&&yy>=0&&yy<240)canvas[yy*240+xx]=colors[q];
  }
  x+=g->advance/divisor;
 }
}
