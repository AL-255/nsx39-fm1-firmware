// SPDX-License-Identifier: GPL-3.0-only
import test from 'node:test';import assert from 'node:assert/strict';
import {encodeBlock,decodeBlock,decodeYamaha,extractDump,crc32} from '../web/lib/bank.js';
import {writeFile,mkdir,readFile} from 'node:fs/promises';import {execFileSync} from 'node:child_process';
test('CRC32 standard check vector',()=>assert.equal(crc32(new TextEncoder().encode('123456789')),0xcbf43926));
test('Yamaha decoding preserves separate integer shifts and clamps',()=>{assert.deepEqual([...decodeYamaha(Uint8Array.of(0x01,0x78,0xff))],[15,61,297,259,-252,-1477]);});
test('IMA independently seeded blocks, bounded reconstruction',()=>{
 for(const f of [()=>0,i=>i%2?32767:-32768,i=>Math.round(22000*Math.sin(i/8)),i=>i*200-10000]){const pcm=Int16Array.from({length:128},(_,i)=>f(i));const p=encodeBlock(pcm),out=decodeBlock(p);assert.equal(p.length,68);assert.equal(out[0],pcm[0]);assert.equal(out.length,128);assert(p[2]<=88);}
 assert.throws(()=>encodeBlock(new Int16Array()));assert.throws(()=>decodeBlock(new Uint8Array(4)));
});
test('C decoder exactly matches JavaScript for every IMA code and state',async()=>{
 await mkdir('build',{recursive:true});const blocks=[];let seed=1;
 for(let idx=0;idx<89;idx++){const b=new Uint8Array(68);b[0]=idx;b[1]=idx*3;b[2]=idx;for(let k=4;k<68;k++){seed=(Math.imul(seed,1664525)+1013904223)>>>0;b[k]=seed>>>24;}blocks.push(b);}
 await writeFile('build/codec-vectors',Buffer.concat(blocks));execFileSync('python3',['tools/gen_tables.py']);execFileSync('cc',['-O2','-std=c11','-Ifirmware/include','-Ibuild/gen','tests/core.c','firmware/src/voice.c','-o','build/core_test']);
 const out=execFileSync('build/core_test',['decode','build/codec-vectors']);const expected=Buffer.concat(blocks.map(b=>Buffer.from(decodeBlock(b).buffer)));assert.deepEqual(out,expected);
});
test('unknown dumps and wrong lengths rejected before parsing',async()=>{
 await assert.rejects(extractDump(new Uint8Array(10)),/2 MiB/);await assert.rejects(extractDump(new Uint8Array(0x200000)),/Unknown NSX39/);
});
