// SPDX-License-Identifier: GPL-3.0-only
// Optional local-only tests. Requires a user-owned NSX39 dump; never used in public CI.
import {readFile,writeFile} from 'node:fs/promises';import assert from 'node:assert/strict';import {execFileSync} from 'node:child_process';
import {generate} from '../web/lib/generator.js';import {Package} from '../web/lib/fm1pkg.js';import {extractDump,packBank} from '../web/lib/bank.js';
const dump=await readFile(process.argv[2]);const manifest=JSON.parse(await readFile('build/manifest.json','utf8'));const template=await readFile('build/template.fwsc');
const pkg=new Package(template);assert(pkg.dec.subarray(pkg.appOff+manifest.bankOffset,pkg.appOff+manifest.bankOffset+manifest.bankCapacity).every(x=>x===0));
const result=await generate(dump,template,manifest);assert.equal(result.pronunciations.length,128);assert(result.bankBytes<=manifest.bankCapacity);
const input=await extractDump(dump);const trimmed=await extractDump(dump.subarray(0,0x200000));assert.equal(trimmed.sha256,input.sha256);assert.deepEqual(input.factoryLyrics,new Uint8Array(dump.subarray(0x1de40,0x1e240)));
for(let i=0;i<36;i++){const d=new DataView(result.bank.buffer);const offset=d.getUint32(64+i*40,true);for(let k=0;k<input.units[i].pcm.length;k++)assert.equal(d.getInt16(offset+2*k,true),input.units[i].pcm[k]);}
assert.deepEqual(result.bytes,(await generate(dump,template,manifest)).bytes);await writeFile('local/voice.fwsc',result.bytes);await writeFile('local/voice.bank',result.bank);
const {instance}=await WebAssembly.instantiate(await readFile('build/voice-engine.wasm'),{});const e=instance.exports;new Uint8Array(e.memory.buffer,e.web_bank_ptr(),result.bank.length).set(result.bank);assert.equal(e.web_init(),1);
for(const note of [53,65,69,77,91,103]){e.nsx_reset();e.nsx_select(0);e.nsx_note_on(note,127);const actual=[];for(let k=0;k<260;k++){e.web_render();actual.push(Buffer.from(new Uint8Array(e.memory.buffer,e.web_audio_ptr(),256)));}const expected=execFileSync('build/core_test',['local/voice.bank','render',String(note),'0']);assert.deepEqual(Buffer.concat(actual),expected);}
// Play every extracted factory bank through the same controller as the panel.
assert.deepEqual(new Uint8Array(result.bank.buffer,new DataView(result.bank.buffer).getUint32(48,true),1024),input.factoryLyrics);
e.web_init();
for(let mode=1;mode<16;mode++){e.performance_mode(mode);const ids=[...input.factoryLyrics.slice(mode*64,(mode+1)*64)].filter(x=>x<128);assert(ids.length);for(let lap=0;lap<2;lap++)for(let i=0;i<ids.length;i++){assert.equal(e.web_next(),ids[i]);e.performance_trigger(65+i%27,127);assert.equal(e.web_last(),ids[i]);assert.equal(e.web_next(),ids[(i+1)%ids.length]);}e.performance_select(-1);assert.equal(e.web_next(),ids[0]);}
const modified=template.slice();modified[modified.length-1]^=1;await assert.rejects(generate(dump,modified,manifest),/hash mismatch/);
console.log(JSON.stringify({units:449,pronunciations:128,factoryBanks:15,bankBytes:result.bankBytes,losslessCycles:36,wasmMatchesNativeNotes:[53,65,69,77,91,103],packageSha256:result.sha256,snrDB:result.snrDB},null,2));
