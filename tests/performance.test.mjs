// SPDX-License-Identifier: GPL-3.0-only
import test from 'node:test';import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';import {existsSync} from 'node:fs';import vm from 'node:vm';
import {fakeLyrics,synthetic} from './fixture.mjs';import {validateFactoryLyrics,crc32} from '../web/lib/bank.js';
const built=existsSync('build/voice-engine.wasm');
async function engine(bank=synthetic(fakeLyrics())){const {instance}=await WebAssembly.instantiate(await readFile('build/voice-engine.wasm'),{});const e=instance.exports;new Uint8Array(e.memory.buffer,e.web_bank_ptr(),bank.length).set(bank);assert.equal(e.web_init(),1);return e;}
test('manual selection, phrase stepping, wrapping, rewind and reserved direction',{skip:!built},async()=>{
 const e=await engine();assert.equal(e.web_last(),128);assert.equal(e.web_next(),0);
 e.performance_manual(39);e.performance_trigger(65,127);assert.equal(e.web_last(),39);assert.equal(e.web_next(),39);
 e.performance_select(-1);assert.equal(e.web_next(),38);assert.equal(e.web_last(),39);
 e.performance_mode(8);assert.equal(e.web_last(),39);assert.equal(e.web_next(),30);assert.equal(e.web_position(),0);
 for(const [id,next]of [[30,31],[31,32],[32,30]]){e.performance_trigger(69,127);assert.equal(e.web_last(),id);assert.equal(e.web_next(),next);}
 e.performance_select(1);assert.equal(e.web_next(),30);e.performance_trigger(70,127);assert.equal(e.web_next(),31);
 e.performance_revoice(65,127);assert.equal(e.web_next(),31);assert.equal(e.web_last(),30);
 e.performance_select(-8);assert.equal(e.web_next(),30);assert.equal(e.web_last(),30);
 e.performance_mode(0);e.performance_midi(0xb0,11,100);assert.equal(e.web_next(),38);e.performance_select(128);assert.equal(e.web_next(),38);
 e.performance_rotate(-1);assert.equal(e.web_next(),75);e.performance_rotate(1);assert.equal(e.web_next(),38);
 e.performance_revoice(65,127);e.performance_midi(0xe0,0,64);assert.equal(e.web_next(),38);
});
test('only positive channel-1 note-on advances a lyric; controls do not retrigger',{skip:!built},async()=>{
 const e=await engine();e.performance_mode(4);e.performance_select(-1);e.performance_select(1);assert.equal(e.nsx_active(),0);
 e.performance_midi(0x91,65,127);e.performance_midi(0x90,65,0);e.performance_midi(0x80,65,0);e.performance_midi(0xe0,0,64);assert.equal(e.web_position(),0);assert.equal(e.web_last(),128);
 e.performance_midi(0x90,65,127);assert.equal(e.web_last(),10);assert.equal(e.web_next(),11);e.performance_midi(0x90,65,0);assert.equal(e.web_next(),11);
 e.performance_midi(0xb0,120,0);assert.equal(e.nsx_active(),0);assert.equal(e.web_last(),10);assert.equal(e.web_next(),11);
 e.performance_midi(0x90,77,127);assert.equal(e.web_last(),11);assert.equal(e.web_next(),12);
});
test('factory table requires bounded syllables and a return marker; malformed banks fail closed',{skip:!built},async()=>{
 for(const mutate of [b=>b.fill(0,0,64),b=>b[0]=128,b=>b[3]=0xc1,b=>b[4]=3]){const table=fakeLyrics();mutate(table);assert.throws(()=>validateFactoryLyrics(table));}
 const bank=synthetic(fakeLyrics()),v=new DataView(bank.buffer),offset=v.getUint32(48,true);const e=await engine(bank);const target=new Uint8Array(e.memory.buffer,e.web_bank_ptr(),bank.length);
 for(const mutate of [b=>new DataView(b.buffer).setUint32(48,0xfffffff0,true),b=>new DataView(b.buffer).setUint32(52,1023,true),b=>b[offset+3]=0xc1,b=>b.fill(0,offset,offset+64)]){const b=bank.slice();mutate(b);const d=new DataView(b.buffer);d.setUint32(12,0,true);d.setUint32(12,crc32(b.subarray(0,d.getUint32(8,true))),true);target.set(b);assert.equal(e.web_init(),0);e.performance_mode(8);assert.equal(e.web_next(),0);}
});
test('AudioWorklet shares the firmware sequencer and reports played/upcoming state',{skip:!built},async()=>{
 let Processor;const replies=[];class Base{constructor(){this.port={postMessage:d=>replies.push(d)};}}
 vm.runInNewContext(await readFile('web/voice-worklet.js','utf8'),{AudioWorkletProcessor:Base,registerProcessor:(_,p)=>Processor=p,WebAssembly,Uint8Array,Int16Array,Float32Array,Int32Array,sampleRate:48000});
 const p=new Processor(),send=d=>p.port.onmessage({data:d});await send({init:true,wasm:await readFile('build/voice-engine.wasm'),bank:synthetic(fakeLyrics())});assert(replies.at(-1).ready);
 await send({adsr:[0,250]});assert.equal(replies.at(-1).articulation[0],250);await send({button:5});assert(replies.at(-1).articulation[9]>0);
 await send({lyric:8});assert.equal(replies.at(-1).next,30);await send({note:65});assert.equal(replies.at(-1).last,30);assert.equal(replies.at(-1).next,31);
 await send({note:69,revoice:true});assert.equal(replies.at(-1).last,30);assert.equal(replies.at(-1).next,31);
 const channels=[new Float32Array(128),new Float32Array(128)];let energy=0;for(let k=0;k<40;k++){assert.equal(p.process([], [channels]),true);energy+=channels[0].reduce((n,x)=>n+x*x,0);}assert(energy>0);
 await send({rewind:true});assert.equal(replies.at(-1).next,30);assert.equal(replies.at(-1).last,30);
});
test('DoReMi follows pitch and ALGORITHM exposes all sixteen memories',{skip:!built},async()=>{
 const e=await engine();e.performance_manual(39);e.performance_rotate(1);const ids=[50,50,114,114,101,95,95,25,25,111,111,32];
 for(const octave of [36,60,84])for(let i=0;i<12;i++){e.performance_trigger(octave+i,127);assert.equal(e.web_last(),ids[i]);e.performance_select(-1);e.performance_select(1);assert.equal(e.web_next(),ids[i]);}
 e.performance_trigger(60,0);assert.equal(e.web_last(),32);e.performance_revoice(60,127);assert.equal(e.web_last(),50);
 for(let bank=0;bank<16;bank++){e.performance_rotate(1);assert.equal(e.web_next(),bank*5);e.performance_trigger(65,127);assert.equal(e.web_last(),bank*5);}
 e.performance_rotate(1);assert.equal(e.web_next(),39);
});
