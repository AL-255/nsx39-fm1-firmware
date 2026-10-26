#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
import os,shutil,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[1]
clang=os.environ.get('NSX39_CLANG') or shutil.which('clang-18') or shutil.which('clang') or '/tmp/nsx39-wasm-toolchain/root/usr/lib/llvm-18/bin/clang'
ld=os.environ.get('NSX39_WASM_LD') or shutil.which('wasm-ld-18') or shutil.which('wasm-ld') or '/tmp/nsx39-wasm-toolchain/root/usr/lib/llvm-18/bin/wasm-ld'
subprocess.run([clang,'--target=wasm32','-O2','-nostdlib','-fno-builtin','-Ifirmware/include','-Ibuild/gen','firmware/src/voice.c','firmware/src/web.c','-fuse-ld='+ld,'-Wl,--no-entry','-Wl,--export-all','-Wl,--initial-memory=2097152','-Wl,--max-memory=2097152','-Wl,-z,stack-size=131072','-o','build/voice-engine.wasm'],cwd=R,check=True)
