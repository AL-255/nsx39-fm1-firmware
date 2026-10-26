#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Execute the real app with nonzero initial SRAM. No hardware or NSX39 input.

The pinned simulator normally zeroes SRAM; this diagnostic-only extension fills
it before it creates SPL parameters. A separate untouched RAM word proves the
fill was actually applied. Tests use the public template's empty voice reserve.
"""
import argparse,os,re,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[1]
REV='eef6d957aa94d3dfa74551688d707b984b792922'
URL='https://github.com/joakimeriksson/fm1-emu.git'
ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('--emulator-source',type=Path,default=R/'.cache/fm1-emu')
ap.add_argument('--fwsc',type=Path,default=R/'build/template.fwsc')
ap.add_argument('--fills',default='0,165,255')
a=ap.parse_args();source=a.emulator_source.resolve()
def run(args,**kw):return subprocess.run(list(map(str,args)),check=True,**kw)
if not (source/'.git').is_dir():
 source.parent.mkdir(parents=True,exist_ok=True)
 run(['git','init',source]);run(['git','-C',source,'remote','add','origin',URL]);run(['git','-C',source,'fetch','--depth','1','origin',REV]);run(['git','-C',source,'checkout','--detach','FETCH_HEAD'])
head=subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()
if head!=REV:raise SystemExit('Unexpected emulator revision')
p=source/'crates/fm1-emu/src/main.rs';s=p.read_text()
if 'FM1_TEST_SRAM_FILL' not in s:
 needle='    let mut soc = Soc::new(args.ram, image);\n'
 if s.count(needle)!=1:raise SystemExit('Emulator source layout changed; inspect before applying RAM fixture')
 s=s.replace(needle,needle+'''    // Diagnostic-only extension: model SRAM left by ROM/previous firmware.
    if let Ok(fill) = std::env::var("FM1_TEST_SRAM_FILL") {
        let byte = fill.parse::<u8>().expect("SRAM fill is a byte") as u32;
        for offset in (0..args.ram).step_by(4) {
            soc.write32(RAM_BASE + offset as u32, byte * 0x01010101);
        }
    }
''');p.write_text(s)
run(['cargo','build','--release','-p','fm1-emu','--manifest-path',source/'Cargo.toml'])
exe=source/'target/release/fm1-emu'
for fill in map(int,a.fills.split(',')):
 if not 0<=fill<=255:raise SystemExit('Invalid fill byte')
 env=dict(os.environ,FM1_TEST_SRAM_FILL=str(fill))
 result=run([exe,'--fwsc',a.fwsc.resolve(),'--ms','550','--dump','0x01c6f000','1'],env=env,cwd=R,capture_output=True,text=True)
 log=result.stdout;(R/f'build/boot-sram-{fill:02x}.log').write_text(log)
 if f'0x01c6f000: {fill*0x01010101:08x}' not in log:raise SystemExit('RAM fixture did not apply')
 frames=re.search(r'lcd frames=(\d+).*display_on=true',log);audio=re.search(r'(\d+) audio halves',log)
 if not frames or int(frames[1])==0 or not audio or int(audio[1])==0:raise SystemExit(f'Boot failed with SRAM fill {fill:#04x}; inspect build/boot-sram-{fill:02x}.log')
 print(f'SRAM {fill:#04x}: LCD on, {frames[1]} frames, {audio[1]} audio halves',flush=True)
