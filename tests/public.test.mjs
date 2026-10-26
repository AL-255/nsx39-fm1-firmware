// SPDX-License-Identifier: GPL-3.0-only
import test from 'node:test';import assert from 'node:assert/strict';import {readFile,writeFile} from 'node:fs/promises';import {existsSync} from 'node:fs';import {execFileSync} from 'node:child_process';
import {Package} from '../web/lib/fm1pkg.js';import {sha256,crc32} from '../web/lib/bank.js';
const built=existsSync('build/template.fwsc')&&existsSync('build/voice-engine.wasm');
import {synthetic} from './fixture.mjs';
test('public template reserve and WASM bank contain no voice data',{skip:!built},async()=>{
 const m=JSON.parse(await readFile('build/manifest.json','utf8')),b=await readFile('build/template.fwsc'),p=new Package(b);assert.equal(await sha256(b),m.templateSha256);assert(p.dec.subarray(p.appOff+m.bankOffset,p.appOff+m.bankOffset+m.bankCapacity).every(x=>x===0));
 const {instance}=await WebAssembly.instantiate(await readFile('build/voice-engine.wasm'),{});const e=instance.exports;assert(new Uint8Array(e.memory.buffer,e.web_bank_ptr(),m.bankCapacity).every(x=>x===0));assert.equal(e.web_init(),0);
});
test('synthetic bank renders and forged offsets are rejected even with valid CRC',{skip:!built},async()=>{
 const bank=synthetic();await writeFile('build/synthetic.bank',bank);const {instance}=await WebAssembly.instantiate(await readFile('build/voice-engine.wasm'),{});const e=instance.exports;const target=new Uint8Array(e.memory.buffer,e.web_bank_ptr(),bank.length);target.set(bank);assert.equal(e.web_init(),1);e.nsx_note_on(69,127);e.web_render();assert(new Int16Array(e.memory.buffer,e.web_audio_ptr(),128).some(x=>x!==0));
 for(const [offset,value]of [[24,0xfffffff0],[28,0xfffffffc],[64,0xfffffff0],[64+16,0xfffffff0],[64+28,0xfffffff0],[64+4,9000]]){const b=bank.slice(),v=new DataView(b.buffer);v.setUint32(offset,value,true);v.setUint32(12,0,true);v.setUint32(12,crc32(b.subarray(0,v.getUint32(8,true))),true);target.set(b);assert.equal(e.web_init(),0);}
 execFileSync('cc',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Ifirmware/include','-Ibuild/gen','tests/core.c','firmware/src/voice.c','-o','build/core_asan']);const report=execFileSync('build/core_asan',['build/synthetic.bank'],{encoding:'utf8'});assert.match(report,/128 pronunciations rendered/);
});
