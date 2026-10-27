#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Allowlist publication. NEVER copy local/, dumps, recordings or extracted banks."""
import argparse,json,shutil,hashlib,tarfile,gzip,io,re
from pathlib import Path
R=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('output',type=Path,nargs='?',default=R/'dist');a=ap.parse_args();out=a.output.resolve()
if out.exists() and any(out.iterdir()):raise SystemExit('Publication destination must be empty')
manifest=json.loads((R/'build/manifest.json').read_text())
template=(R/'build/template.fwsc').read_bytes()
if hashlib.sha256(template).hexdigest()!=manifest['templateSha256']:raise SystemExit('Template hash mismatch')
# Prove the compiled reserve is empty using the package parser in a separate test.
out.mkdir(parents=True,exist_ok=True)
for rel in ['index.html','style.css','site.js','memory-editor.js','settings-editor.js','app.js','worker.js','voice-worklet.js','syllables.json','lib/bank.js','lib/generator.js','lib/fm1pkg.js','lib/fm1ota.js','lib/install-progress.js','lib/memory.js','lib/device-memory.js','lib/device-session.js']:
 (out/rel).parent.mkdir(parents=True,exist_ok=True)
 shutil.copyfile(R/'web'/rel,out/rel)
# Version UI imports and entry points to avoid mixing cached protocol callbacks
# with a new progress UI (GitHub Pages caches static files for ten minutes).
revisions={}
def version_module(path):
 if path in revisions:return revisions[path]
 text=path.read_text()
 def dependency(match):
  relative=match[2];dependency_path=(path.parent/relative).resolve()
  if not dependency_path.is_relative_to(out) or not dependency_path.is_file():raise SystemExit('Invalid local module import: '+relative)
  return match[1]+relative+'?v='+version_module(dependency_path)+match[1]
 text=re.sub(r'''(["'])(\./[^"'\n]+\.js)\1''',dependency,text)
 for worker in ['worker.js','voice-worklet.js']:
  if path.name==worker:continue
  for quote in ['"',"'"]:
   if quote+worker+quote in text:
    text=text.replace(quote+worker+quote,quote+worker+'?v='+version_module((out/worker).resolve())+quote)
 path.write_text(text);revisions[path]=hashlib.sha256(path.read_bytes()).hexdigest()[:16]
 return revisions[path]
for rel in ['site.js','app.js','worker.js','voice-worklet.js']:version_module((out/rel).resolve())
html=(out/'index.html').read_text()
for rel in ['site.js','style.css']:
 revision=hashlib.sha256((out/rel).read_bytes()).hexdigest()[:16]
 html=html.replace('"'+rel+'"','"'+rel+'?v='+revision+'"')
(out/'index.html').write_text(html)
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
