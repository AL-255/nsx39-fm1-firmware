// SPDX-License-Identifier: GPL-3.0-only
import test from 'node:test';import assert from 'node:assert/strict';import{readFile}from'node:fs/promises';import{execFileSync}from'node:child_process';
import{encodeMemory,decodeMemory,parseLyrics,lyricArrays,DEFAULT_VALUES,loadMemory,saveMemory}from'../web/lib/memory.js';
import{DeviceMemory,memoryPacket,memoryResponse}from'../web/lib/device-memory.js';
import{pack7,unpack7}from'../web/lib/fm1ota.js';import{fakeLyrics,synthetic}from'./fixture.mjs';
import{crc32}from'../web/lib/bank.js';
const config=()=>({values:[...DEFAULT_VALUES],lyrics:lyricArrays(fakeLyrics())});
test('kana, katakana, romaji and exact IDs produce the same bounded lyric sequence',async()=>{
 const labels=JSON.parse(await readFile('web/syllables.json'));
 for(const text of ['はつねみく','ハツネミク','ha tsu ne mi ku','hatsunemiku','は、つ。ね みく'])assert.deepEqual(parseLyrics(text,labels),[71,60,66,101,7]);
 assert.deepEqual(parseLyrics('#124 #127',labels),[124,127]);assert.deepEqual(parseLyrics("o'ozora o’ozora",labels),[4,4,30,111,4,4,30,111]);assert.deepEqual(parseLyrics("n a n'ya n (m)",labels),[123,0,123,108,124]);for(const text of ['', 'hello!', '#128','あ'.repeat(64)])assert.throws(()=>parseLyrics(text,labels));
});
test('configuration round-trips through browser storage and C, rejects corruption and preserves lyric position',async()=>{
 const c=config();c.values[0]=250;c.values[16]=2;c.values[17]=0;c.lyrics[6]=[5,6,7,8];const b=encodeMemory(c);assert.deepEqual(decodeMemory(b),c);
 const storage={getItem(){return this.value},setItem(k,v){this.value=v}};saveMemory(storage,c);assert.deepEqual(loadMemory(storage),c);const bad=b.slice();bad[102]^=1;assert.throws(()=>decodeMemory(bad),/checksum/);
 const{instance}=await WebAssembly.instantiate(await readFile('build/voice-engine.wasm'),{}),e=instance.exports,bank=synthetic(fakeLyrics());new Uint8Array(e.memory.buffer,e.web_bank_ptr(),bank.length).set(bank);assert.equal(e.web_init(),1);assert.equal(e.instrument_octave(),0);
 new Uint8Array(e.memory.buffer,e.web_state_ptr(),1120).set(b);assert.equal(e.web_state_apply(),1);assert.equal(e.instrument_script(),2);e.performance_mode(8);e.performance_trigger(53,127);assert.equal(e.web_next(),6);
 const current=decodeMemory(new Uint8Array(e.memory.buffer,e.web_state_ptr(),1120));current.values[1]=400;new Uint8Array(e.memory.buffer,e.web_state_ptr(),1120).set(encodeMemory(current));assert.equal(e.web_state_apply(),1);assert.equal(e.web_next(),6);
 assert.deepEqual(decodeMemory(new Uint8Array(e.memory.buffer,e.web_state_ptr(),1120)),current);
});
test('flash journal survives byte-level power interruptions without touching updater or unknown data',()=>{
 execFileSync('cc',['-O1','-g','-fsanitize=address,undefined','-Ifirmware/include','-Ibuild/gen','tests/persistence.c','-o','build/persistence_test']);console.log(execFileSync('build/persistence_test',{encoding:'utf8',env:{...process.env,UBSAN_OPTIONS:'halt_on_error=1'}}).trim());
});
test('generated settings seed is applied at startup while original lyrics remain available for reset',async()=>{
 const c=config();c.lyrics[6]=[71,60,66,101,7];c.values[0]=350;c.values[14]=8;c.values[16]=2;
 const bank=synthetic(fakeLyrics(),encodeMemory(c)),{instance}=await WebAssembly.instantiate(await readFile('build/voice-engine.wasm'),{}),e=instance.exports;
 const target=new Uint8Array(e.memory.buffer,e.web_bank_ptr(),bank.length);target.set(bank);assert.equal(e.web_init(),1);assert.equal(e.web_next(),71);assert.equal(e.instrument_value(4),350);assert.equal(e.instrument_script(),2);
 e.performance_trigger(53,127);assert.equal(e.web_next(),60);e.instrument_defaults();assert.equal(e.instrument_value(4),0);e.performance_mode(8);assert.equal(e.web_next(),30);
 const v=new DataView(bank.buffer),offset=v.getUint32(56,true);bank[offset+100]^=1;v.setUint32(12,0,true);v.setUint32(12,crc32(bank.subarray(0,v.getUint32(8,true))),true);target.set(bank);assert.equal(e.web_init(),0);
});
test('memory SysEx validates routing, checksums, complete transfer, durable acknowledgement and readback',async()=>{
 const c=config();let stored=encodeMemory(c),staged=new Uint8Array(1120),mask=0,q=[];const link={send(packet){const p=packet.slice(1,-1),cmd=p[5],id=p[6],chunk=p[7];let data;
  if(cmd===1)data=[chunk,...pack7(stored.slice(chunk*128,Math.min(1120,(chunk+1)*128)))];
  if(cmd===2){const bits=p.slice(8,-1);let acc=0,n=0,out=[];for(const b of bits){acc|=b<<n;n+=7;while(n>=8){out.push(acc&255);acc>>>=8;n-=8;}}staged.set(out,chunk*128);mask|=1<<chunk;data=[0];}
  if(cmd===3){assert.equal(mask,511);decodeMemory(staged);stored=staged.slice();data=[0];}q.push(memoryPacket(cmd|64,id,data));return true;},async read(){return q.shift();},close(){}};
 const device=new DeviceMemory({link});assert.deepEqual(await device.read(),c);c.lyrics[3]=[1,2,3];assert.deepEqual(await device.write(c),c);
 const packet=memoryPacket(65,7,[0,1,2]);assert.deepEqual(memoryResponse(packet,1,7),[0,1,2]);packet[8]^=1;assert.equal(memoryResponse(packet,1,7),null);
});
test('independent lyric/settings updates read fresh device state and preserve unrelated changes',async()=>{
 let current=config(),reads=0,writes=0;const device=new DeviceMemory({link:{}});
 device.read=async()=>{reads++;return structuredClone(current);};device.write=async c=>{writes++;encodeMemory(c);current=structuredClone(c);return current;};
 current.values[0]=415;current.values[17]=2;
 await device.update({lyric:{slot:6,ids:[71,60,66,101,7]}});assert.equal(current.values[0],415);assert.equal(current.values[17],2);
 current.lyrics[7]=[3,4,5];await device.update({values:{0:425}});assert.deepEqual(current.lyrics[6],[71,60,66,101,7]);assert.deepEqual(current.lyrics[7],[3,4,5]);assert.equal(current.values[17],2);
 assert.equal(reads,2);assert.equal(writes,2);
 for(const patch of [{lyric:{slot:16,ids:[1]}},{values:{18:1}},{values:{0:2001}},{lyric:{slot:6,ids:[]}}])await assert.rejects(device.update(patch));assert.equal(writes,2);
});
test('legacy state migrates lyric selections once and refuses unsupported old-device modes',()=>{
 const c=config();c.values[14]=8;const old=encodeMemory(c,0,0,1);assert.equal(new DataView(old.buffer).getUint16(60,true),6);assert.deepEqual(decodeMemory(old),c);assert.deepEqual(decodeMemory(encodeMemory(decodeMemory(old))),c);
 for(const mode of [1,2]){c.values[14]=mode;assert.throws(()=>encodeMemory(c,0,0,1),/944/);assert.deepEqual(decodeMemory(encodeMemory(c)),c);}
 c.values[14]=0;assert.deepEqual(decodeMemory(encodeMemory(c,0,0,1)),c);
});
test('FM-1_943 wire uses its legacy format and unsupported modes send no staging commands',async()=>{
 const c=config();c.values[14]=8;const device=new DeviceMemory({id:{version:943},link:{}});let calls=0,staged=new Uint8Array(1120);
 device.request=async(cmd,p=[])=>{calls++;if(cmd===2){staged.set(unpack7(p.slice(1)),p[0]*128);return[0];}if(cmd===3)return[0];throw Error('Unexpected command');};device.read=async()=>decodeMemory(staged);
 await device.write(c);assert.equal(calls,10);assert.equal(new DataView(staged.buffer).getUint32(4,true),1);assert.equal(new DataView(staged.buffer).getUint16(60,true),6);
 for(const mode of[1,2]){c.values[14]=mode;await assert.rejects(device.write(c),/944/);}assert.equal(calls,10);
});
test('storage repair requires a complete checked backup and sends only a fixed proof',async()=>{
 const c=config(),state=encodeMemory(c),raw=Uint8Array.from({length:8192},(_,i)=>(i*17+31)&255);let initialized=0;
 const device=new DeviceMemory({id:{version:947,text:'FM-1_947'},link:{}});device.readBytes=async()=>state.slice();device.request=async(cmd,data)=>{
  if(cmd===4){assert(data[0]>=0&&data[0]<64);return[data[0],...pack7(raw.slice(data[0]*128,(data[0]+1)*128))];}
  assert.equal(cmd,5);const proof=new Uint8Array(unpack7(data)),v=new DataView(proof.buffer);assert.equal(proof.length,12);assert.equal(v.getUint32(0,true),crc32(raw));assert.equal(v.getUint32(4,true),new DataView(state.buffer).getUint32(12,true));assert.deepEqual([...proof.slice(8)],[73,78,73,84]);initialized++;return[0];
 };
 const backup=await device.backupStorage();assert.deepEqual(backup.storage,[...raw]);assert.deepEqual(await device.initializeStorage(backup),c);assert.equal(initialized,1);
 for(const mutate of[b=>b.storage[0]^=1,b=>b.offset=0,b=>b.device='FM-1_946',b=>b.storage.pop()]){const bad=structuredClone(backup);mutate(bad);await assert.rejects(device.initializeStorage(bad));}assert.equal(initialized,1);
 device.request=async()=>[7];await assert.rejects(device.initializeStorage(backup),/changed since/);
 device.device.id.version=946;await assert.rejects(device.backupStorage(),/947/);await assert.rejects(device.initializeStorage(backup),/947/);
});
