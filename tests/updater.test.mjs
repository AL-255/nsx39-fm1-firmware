// SPDX-License-Identifier: GPL-3.0-only
import test from 'node:test';import assert from 'node:assert/strict';
import {Updater,pack7,unpack7} from '../web/lib/fm1ota.js';
function request(addr,len=8){const b=[0,0x59,0x30,8,0,0,0,addr&255,(addr>>>8)&255,(addr>>>16)&255,(addr>>>24)&255,len&255,(len>>>8)&255,0];b.push(~b.slice(6).reduce((a,b)=>a+b,0)&255);return [0xf0,...pack7(b),0xf7];}
function link(packets,send=true){return {lost:false,sent:[],read:async()=>packets.shift(),send(p){this.sent.push(p);return send;}};}
test('pack7 round trip every byte',()=>{const b=Array.from({length:256},(_,i)=>i);assert.deepEqual(unpack7(pack7(b)),b);});
test('serve refuses empty, oversized and out-of-image reads',async()=>{for(const [a,n]of[[0,0],[0,513],[100,20]]){const l=link([request(a,n)]);await assert.rejects(new Updater({}).serve(l,new Uint8Array(110),0xf0000000,1),/bad request/);assert.equal(l.sent.length,0);}});
test('phase-specific completion gate requires a prior read',async()=>{
 for(const packets of [[request(0xf0000000)],[request(0,8),request(0xe0000000)],[request(0,8),request(0xf0000000,7)]]){const l=link(packets);await assert.rejects(new Updater({}).serve(l,new Uint8Array(16),0xf0000000,1),/completion/);assert(l.sent.length<=1);}
});
test('completion response send failure cannot report success',async()=>{const l=link([request(0,8),request(0xf0000000)]);const old=l.send;l.send=function(p){old.call(this,p);return this.sent.length<2;};const r=await new Updater({}).serve(l,new Uint8Array(16),0xf0000000,1);assert.equal(r.finished,false);assert.equal(r.lost,true);});
test('correct read then finish succeeds with 8-byte success response',async()=>{const l=link([request(0,8),request(0xf0000000)]);const r=await new Updater({}).serve(l,new Uint8Array(16),0xf0000000,1);assert.equal(r.finished,true);assert.equal(r.served,1);const data=unpack7(l.sent[1].slice(1,-1));assert.deepEqual(data.slice(14,22),[...new TextEncoder().encode('success'),0]);});
