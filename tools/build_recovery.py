#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build the small, source-only, SRAM recovery stub. No proprietary input."""
from pathlib import Path
import os,subprocess,hashlib
R=Path(__file__).resolve().parents[1];out=R/'build/recovery';out.mkdir(parents=True,exist_ok=True)
tc=Path(os.environ.get('JIELI_TOOLCHAIN',str(R/'.cache/toolchain')))
def run(tool,*args):return subprocess.check_output([str(tc/tool),*map(str,args)],cwd=R,text=True)
for src,name in [('entry.S','entry'),('boot_state.c','boot_state')]:
 run('pi32v2/bin/clang','-target','pi32v2','-Os','-ffunction-sections','-fdata-sections','-fno-builtin','-Ivendor/felucca/firmware/hal','-c',R/'firmware/recovery'/src,'-o',out/(name+'.o'))
elf=out/'recovery.elf';run('pi32v2/bin/ld','--gc-sections','-e','_start','-T',R/'firmware/recovery/ram.ld',out/'entry.o',out/'boot_state.o','-o',elf)
run('common/bin/objcopy','-O','binary','-j','.text',elf,out/'raw.bin')
b=(out/'raw.bin').read_bytes()
if not b or len(b)>512:raise SystemExit('Unexpected recovery code size')
b+=bytes(512-len(b));(out/'boot-state.bin').write_bytes(b)
(out/'recovery.dis').write_text(run('common/bin/objdump','-d',elf))
from recover_boot_state import PAYLOAD_SHA
if hashlib.sha256(b).hexdigest()!=PAYLOAD_SHA:raise SystemExit('Recovery code changed; audit before updating the helper hash')
print('512-byte SRAM recovery stub SHA-256:',hashlib.sha256(b).hexdigest())
