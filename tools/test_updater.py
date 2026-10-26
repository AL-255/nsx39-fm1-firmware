#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run real updater cores against public, synthetic packages. No device access."""
from pathlib import Path
import os,subprocess,sys
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'vendor/felucca/tools'))
import fm1pkg_make as pkg
pkg.SDK=Path(os.environ.get('AC79_SDK',str(R/'.cache/ac79-sdk')))
app=bytearray((R/'build/voice-app.bin').read_bytes());app[-1]^=0x55
# This is a mutation test fixture, NOT a firmware to install on hardware.
new=pkg.ufw(pkg.flash_image(app,pkg.KEY),(R/'build/loader/ota.bin').read_bytes(),'FM-1_939')
(R/'build/test-update.fwsc').write_bytes(new)
for test in ['ota','ldr']:
 subprocess.run(['cc','-O2',f'vendor/felucca/tests/{test}_test.c','-o',f'build/{test}_test'],cwd=R,check=True)
 args=['build/template.fwsc'] if test=='ota' else ['build/template.fwsc','build/test-update.fwsc']
 subprocess.run([f'build/{test}_test',*args],cwd=R,check=True)
