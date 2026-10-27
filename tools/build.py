#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build only redistributable code, with an EMPTY voice slot. No NSX39 input."""
import os,sys,json,hashlib,subprocess,re,struct,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build';OUT.mkdir(exist_ok=True)
V=ROOT/'vendor/felucca'
sys.path.insert(0,str(V/'tools'))
import fm1pkg_make as pkg
import lz4blk
TC=Path(os.environ.get('JIELI_TOOLCHAIN',str(ROOT/'.cache/toolchain'))).resolve()
SDK=Path(os.environ.get('AC79_SDK',str(ROOT/'.cache/ac79-sdk'))).resolve()
def run(args):
 r=subprocess.run(list(map(str,args)),cwd=ROOT,capture_output=True,text=True)
 if r.returncode:sys.exit(r.stdout+r.stderr)
 if r.stderr:print(r.stderr,end='')
 return r.stdout
def tool(name,*args):return run([TC/name,*args])
def cc(src,out):tool('pi32v2/bin/clang','-target','pi32v2','-Os','-ffunction-sections','-fdata-sections','-fno-builtin','-Wall','-Wno-unused-function','-Ifirmware/include','-Ivendor/felucca/firmware/hal','-Ibuild/gen','-c',src,'-o',out)
run([sys.executable,'tools/gen_tables.py'])
run([sys.executable,'tools/gen_label_font.py'])
# Rasterize a redistributable font from the OS; shipped as source data with notice.
font=Path('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf')
from PIL import Image,ImageFont,ImageDraw
f=ImageFont.truetype(str(font),12)
rows=[]
for c in range(32,128):
 im=Image.new('1',(8,14));ImageDraw.Draw(im).text((0,-2),chr(c),font=f,fill=1)
 rows.append([sum((1<<x) for x in range(8) if im.getpixel((x,y))) for y in range(14)])
(OUT/'gen/ui_font.h').write_text('// DejaVu Sans Mono rasterization. See vendor/DejaVu-LICENSE.txt\nconst uint8_t ui_font[96][14]={'+','.join('{'+','.join(map(str,r))+'}' for r in rows)+'};\n')
# Validate every SDK component, not merely its filename.
for rel,want in {'uboot.boot':'4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4','cfg_tool.bin':'276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b','cfg/eq_cfg_hw.bin':'41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d'}.items():
 p=SDK/'cpu/wl82/tools'/rel
 if hashlib.sha256(p.read_bytes()).hexdigest()!=want:sys.exit('SDK SHA-256 mismatch: '+rel)
objects=[]
for src,name in [('firmware/crt0.S','crt0'),('vendor/felucca/firmware/hal/fm1_vec.S','vec'),('vendor/felucca/firmware/hal/fm1_isr.S','isr'),('firmware/src/main.c','main')]:
 o=OUT/(name+'.o');cc(src,o);objects.append(o)
elf=OUT/'voice.elf'
tool('pi32v2/bin/ld','--gc-sections','-e','_start','-T','firmware/app.ld',*objects,'-o',elf)
syms=tool('common/bin/objdump','-t',elf);dis=tool('common/bin/objdump','-d',elf)
(OUT/'voice.dis').write_text(dis)
def sym(n):
 m=re.search(r'^([0-9a-f]+) .*\s'+n+r'$',syms,re.M)
 if not m:sys.exit('Missing symbol: '+n)
 return int(m[1],16)
app=bytearray()
for section,load in [('.text',0x02000120),('.ram_text',sym('_rt_load')),('.data',sym('_data_load'))]:
 p=OUT/(section[1:]+'.bin');tool('common/bin/objcopy','-O','binary','-j',section,elf,p)
 b=p.read_bytes();off=load-0x02000120
 if b:
  if off<len(app):sys.exit('Overlapping load sections')
  app+=b'\xff'*(off-len(app));app+=b
app+=b'\xff'*(-len(app)%4)
if app[:4]!=bytes.fromhex('04818000') or len(app)>pkg.APP_SLOT:sys.exit('Invalid app entry or oversized image')
bank_offset=sym('voice_bank')-0x02000120
if any(app[bank_offset:bank_offset+0x78000]):sys.exit('Public template must have an empty voice slot')
# Verify that everything executing during flash writes stays in SRAM.
ram_dis=tool('common/bin/objdump','-d','-j','.ram_text',elf)
for line in ram_dis.splitlines():
 if re.search(r'\bcall\s+0x0?2[0-9a-f]{6}',line):sys.exit('RAM flash code calls XIP: '+line)
(OUT/'voice-app.bin').write_bytes(app)
ldr=OUT/'loader';ldr.mkdir(exist_ok=True)
for src,name in [('crt0_ldr.S','start'),('loader.c','loader')]:cc(V/'firmware/loader'/src,ldr/(name+'.o'))
lelf=ldr/'loader.elf';tool('pi32v2/bin/ld','--gc-sections','-e','_start','-T',V/'firmware/loader/loader.ld',ldr/'start.o',ldr/'loader.o','-o',lelf)
tool('common/bin/objcopy','-O','binary','-j','.text',lelf,ldr/'raw.bin')
raw=(ldr/'raw.bin').read_bytes()
if not raw or len(raw)>0x14000:sys.exit('Invalid loader size')
blocks=bytearray()
for i in range(0,len(raw),4096):
 b=raw[i:i+4096];c=lz4blk.compress(b)
 if len(c)>4096 or lz4blk.decompress(c)!=b:sys.exit('Loader compression failed')
 blocks+=struct.pack('<II',len(c),len(b))+c
def head(crc,a,b,attr,name):
 body=struct.pack('<HIIBBH16s',crc,a,b,attr,0,0,name);return struct.pack('<H',pkg.crc16(body))+body
body=head(pkg.crc16(raw),len(raw),0x01c0a800,0,b'usb_hid_ota.bin')+blocks
ota=head(pkg.crc16(body),32,len(body),0x41,b'usb_hid_ota.bin')+body
(ldr/'ota.bin').write_bytes(ota)
pkg.SDK=SDK
template=pkg.ufw(pkg.flash_image(app,pkg.KEY),ota,'FM-1_946')
(OUT/'template.fwsc').write_bytes(template)
manifest={'format':1,'product':'FM-1_946','version':'0.4.3-debug','bankOffset':bank_offset,'bankCapacity':0x78000,'appBytes':len(app),'templateSha256':hashlib.sha256(template).hexdigest(),'hardwareValidated':False,'compatibility':'experimental-native-vocals','loaderSource':'hugelton/Felucca@440e62ce8a33d9bb285ab6bbca9a0fd50e459648','ramBytes':sym('_bss_end')-0x1c08000,'poolBytes':sym('_pool_end')-sym('_pool_start')}
if manifest['ramBytes']>72*1024-4096:sys.exit('Insufficient RAM headroom')
(OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest,indent=2))
