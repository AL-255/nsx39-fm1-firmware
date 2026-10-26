#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Allowlist publication. NEVER copy local/, dumps, recordings or extracted banks."""
import argparse,json,shutil,hashlib,tarfile,gzip,io
from pathlib import Path
R=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('output',type=Path,nargs='?',default=R/'dist');a=ap.parse_args();out=a.output.resolve()
if out.exists() and any(out.iterdir()):raise SystemExit('Publication destination must be empty')
manifest=json.loads((R/'build/manifest.json').read_text())
template=(R/'build/template.fwsc').read_bytes()
if hashlib.sha256(template).hexdigest()!=manifest['templateSha256']:raise SystemExit('Template hash mismatch')
# Prove the compiled reserve is empty using the package parser in a separate test.
out.mkdir(parents=True,exist_ok=True)
for rel in ['index.html','style.css','app.js','worker.js','voice-worklet.js','syllables.json','lib/bank.js','lib/generator.js','lib/fm1pkg.js','lib/fm1ota.js']:
 (out/rel).parent.mkdir(parents=True,exist_ok=True)
 shutil.copyfile(R/'web'/rel,out/rel)
(out/'assets').mkdir(exist_ok=True);(out/'licenses').mkdir(exist_ok=True)
for source,dest in [('build/template.fwsc','assets/template.fwsc'),('build/manifest.json','assets/manifest.json'),('build/voice-engine.wasm','assets/voice-engine.wasm'),('LICENSE','licenses/GPL-3.0.txt'),('vendor/Apache-2.0.txt','licenses/Apache-2.0.txt'),('vendor/DejaVu-LICENSE.txt','licenses/DejaVu-LICENSE.txt'),('vendor/Noto-CJK-LICENSE.txt','licenses/Noto-CJK-LICENSE.txt')]:shutil.copyfile(R/source,out/dest)
# Public binaries have accessible corresponding source even if the GitHub repo is private.
source_files=[R/'LICENSE',R/'README.md',R/'package.json',R/'.gitignore',R/'web/syllables.json']
allowed={'.c','.h','.S','.ld','.mjs','.py','.sh','.js','.css','.html','.md','.yml','.txt'}
for directory in ['firmware','vendor','web','tools','tests','docs','.github']:
 for p in sorted((R/directory).rglob('*')):
  if p.is_file() and not p.is_symlink() and (p.suffix in allowed or p.name=='LICENSE'):
   source_files.append(p)
with (out/'source.tar.gz').open('wb') as raw:
 with gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0) as zipped:
  with tarfile.open(fileobj=zipped,mode='w') as tar:
   for p in sorted(set(source_files)):
    data=p.read_bytes();info=tarfile.TarInfo('nsx39-fm1-firmware/'+str(p.relative_to(R)))
    info.size=len(data);info.mode=0o644;info.mtime=0;tar.addfile(info,io.BytesIO(data))
(out/'.nojekyll').write_text('')
print(f'Published-file tree prepared at {out}; no user inputs were read.')
