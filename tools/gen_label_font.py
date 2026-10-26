#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Readable syllable labels: a compact 2-bit Noto Sans CJK JP glyph subset.

The labels are UI annotations, not source-voice data. No NSX39 input is read.
Noto font license: SIL Open Font License 1.1, vendor/Noto-CJK-LICENSE.txt.
"""
import json,os
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
R=Path(__file__).resolve().parents[1]
def katakana(s):return ''.join(chr(ord(c)+0x60) if 0x3041<=ord(c)<=0x3096 else c for c in s)
def generate():
 rows=json.loads((R/'web/syllables.json').read_text());assert len(rows)==128
 labels=[[h,katakana(h),r] for r,h in rows]
 cached=R/'.cache/NotoSansCJK-Bold.ttc'
 path=os.environ.get('NSX39_LABEL_FONT',str(cached) if cached.is_file() else '/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc')
 font=ImageFont.truetype(path,40,index=0)
 atlas=bytearray();glyphs=[]
 chars=sorted(set(''.join(''.join(row) for row in labels)+'?—'))
 for c in chars:
  x0,y0,x1,y1=font.getbbox(c,anchor='ls');w,h=x1-x0,y1-y0
  assert 0<=w<=48 and 0<=h<=48
  im=Image.new('L',(max(1,w),max(1,h)));ImageDraw.Draw(im).text((-x0,-y0),c,font=font,fill=255,anchor='ls')
  offset=len(atlas);packed=bytearray((w*h+3)//4);levels=[]
  for y in range(h):
   for x in range(w):
    q=min(3,(im.getpixel((x,y))+42)//85);levels.append(q);packed[(y*w+x)//4]|=q<<(((y*w+x)%4)*2)
  if c!=' ' and not any(packed):raise ValueError('Empty glyph: '+c)
  rle=bytearray();i=0
  while i<len(levels):
   end=i+1
   while end<len(levels) and levels[end]==levels[i] and end-i<64:end+=1
   rle.append((levels[i]<<6)|(end-i-1));i=end
  codec=int(len(rle)<len(packed));atlas+=rle if codec else packed
  glyphs.append((offset,ord(c),x0,y0,w,h,round(font.getlength(c)),codec))
 widths=[sum(round(font.getlength(c)) for c in text) for row in labels for text in row]
 if max(widths)>220:raise ValueError('Label does not fit the display')
 out=R/'build/gen';out.mkdir(parents=True,exist_ok=True)
 lines=['// Generated from Noto Sans CJK JP Bold; SIL OFL 1.1.\n', 'typedef struct {uint32_t offset;uint16_t codepoint;int8_t left,top;uint8_t width,height,advance,codec;} label_glyph_t;\n','static const label_glyph_t label_glyphs[]={\n']
 lines+=['{'+','.join(map(str,g))+'},\n' for g in glyphs];lines+=['};\nstatic const uint8_t label_pixels[]={\n']
 for i in range(0,len(atlas),96):lines.append(','.join(map(str,atlas[i:i+96]))+',\n')
 lines+=['};\nstatic const char *const syllable_labels[128][3]={\n']
 lines+=['{'+','.join(json.dumps(s,ensure_ascii=False) for s in row)+'},\n' for row in labels];lines+=['};\n']
 (out/'label_font.h').write_text(''.join(lines));(out/'label_font.json').write_text(json.dumps({'font':font.getname(),'pixels':40,'glyphs':len(glyphs),'atlasBytes':len(atlas),'maxLabelWidth':max(widths)},indent=2)+'\n')
 print((out/'label_font.json').read_text())
if __name__=='__main__':generate()
