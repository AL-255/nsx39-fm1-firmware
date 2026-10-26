// SPDX-License-Identifier: GPL-3.0-only
// All Yamaha data is read from the user's dump. This file contains format knowledge only.
export const DUMP_SHA = '036c666d47cc3b9a7b51914fd0b9883fc3cda8ba7e79581abbae1a9c6c9e50a6';
export const FLASH_SHA = '4e9b8eb35198f2b809fa49f9c704a8982ec6b5fa6cd610c4bde807d8fb02a9e4';
export const CAPACITY = 0x78000;
export const BLOCK_SAMPLES = 128;
export const BLOCK_BYTES = 68; // first PCM16, IMA index, reserved, 127 nibbles + padding
export const STEP = [7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767];
export const INDEX = [-1,-1,-1,-1,2,4,6,8];
const clamp = (x,a,b) => Math.max(a,Math.min(b,x));
export async function sha256(b) {
  return [...new Uint8Array(await crypto.subtle.digest('SHA-256',b))].map(x=>x.toString(16).padStart(2,'0')).join('');
}
export function crc32(bytes) {
  let crc=0xffffffff;
  for (const b of bytes) { crc ^= b; for(let k=0;k<8;k++) crc=(crc>>>1)^((crc&1)?0xedb88320:0); }
  return (crc^0xffffffff)>>>0;
}
export function decodeYamaha(data) {
  let p=0,s=127; const scale=[230,230,230,230,307,409,512,614],out=new Int16Array(data.length*2);
  for(let i=0;i<out.length;i++) {
    const c=(data[i>>1]>>((i&1)?0:4))&15;
    let d=s>>3; if(c&4)d+=s;if(c&2)d+=s>>1;if(c&1)d+=s>>2;
    p=clamp(p+((c&8)?-d:d),-32768,32767);s=clamp((s*scale[c&7])>>8,127,24576);out[i]=p;
  }
  return out;
}
function imaCode(target,p,index) {
  let d=target-p,c=0,s=STEP[index],v=s>>3;
  if(d<0){c=8;d=-d;} if(d>=s){c|=4;d-=s;v+=s;}s>>=1;
  if(d>=s){c|=2;d-=s;v+=s;}s>>=1;if(d>=s){c|=1;v+=s;}
  return [c,clamp(p+((c&8)?-v:v),-32768,32767),clamp(index+INDEX[c&7],0,88)];
}
export function encodeBlock(pcm) {
  if(!pcm.length||pcm.length>128)throw Error('Invalid block size');
  // Choose the initial step by a short error search, independently for each block.
  let best=0,cost=Infinity;
  for(let idx=0;idx<=88;idx++) {
    let p=pcm[0],q=idx,e=0;
    for(let i=1;i<Math.min(pcm.length,25);i++){const r=imaCode(pcm[i],p,q);p=r[1];q=r[2];e+=(pcm[i]-p)**2;}
    if(e<cost){best=idx;cost=e;}
  }
  const out=new Uint8Array(68),v=new DataView(out.buffer);v.setInt16(0,pcm[0],true);out[2]=best;
  let p=pcm[0],idx=best;
  for(let i=1;i<128;i++) { const r=imaCode(pcm[Math.min(i,pcm.length-1)],p,idx);p=r[1];idx=r[2];out[4+((i-1)>>1)]|=r[0]<<(((i-1)&1)*4); }
  return out;
}
export function decodeBlock(data) {
  if(data.length!==68||data[2]>88||data[3])throw Error('Invalid IMA block');
  const out=new Int16Array(128);let p=new DataView(data.buffer,data.byteOffset,data.byteLength).getInt16(0,true),idx=data[2];out[0]=p;
  for(let i=1;i<128;i++){const c=(data[4+((i-1)>>1)]>>(((i-1)&1)*4))&15,s=STEP[idx];let d=s>>3;if(c&4)d+=s;if(c&2)d+=s>>1;if(c&1)d+=s>>2;p=clamp(p+((c&8)?-d:d),-32768,32767);idx=clamp(idx+INDEX[c&7],0,88);out[i]=p;}
  return out;
}
export async function extractDump(input) {
  const b=new Uint8Array(input); if(![0x200000,0x200110].includes(b.length))throw Error('Expected a 2 MiB NSX39 SPI dump, optionally with its 272-byte programmer trailer.');
  // Hash the flash separately: user lyric/calibration changes are deliberately not silently accepted.
  const hash=await sha256(b.subarray(0,0x200000));if(hash!==FLASH_SHA)throw Error('Unknown NSX39 firmware revision or modified dump. SHA-256 does not match the supported flash.');
  const image=b.slice(0x40000,0x1fe180);for(let i=0x80;i<image.length;i++)image[i]^=0xd9;
  const v=new DataView(image.buffer),u16=o=>v.getUint16(o),u32=o=>v.getUint32(o),str=(o,n)=>new TextDecoder('ascii').decode(image.subarray(o,o+n)).split('\0')[0];
  if(str(0,4)!=='NSXP'||u32(4)!==0x80||u32(8)!==0x04000000||u32(12)!==image.length)throw Error('Unsupported NSXP layout');
  let pos=0x57be0;const leaves=[];
  function take(n){const o=pos;pos+=n;if(pos>0x5bfbc)throw Error('Database exceeds its boundary');return o;}
  const root=take(72),rootCount=u32(root+68);if(rootCount>16)throw Error('Invalid database root');
  function branch(depth,path){const o=take(8),count=u32(o+4);path=[...path,image[o]];if(count>512)throw Error('Invalid group count');
    if(depth<2){for(let i=0;i<count;i++)branch(depth+1,path);return;}
    for(let i=0;i<count;i++){const g=take(16),source=str(g,8),n=u32(g+12);if(n>449)throw Error('Invalid unit count');
      for(let j=0;j<n;j++){const a=take(36);if(path[0]!==1)leaves.push({source,target:str(a+8,8),variant:j,kind:path[1]?1:0,anchor:u32(a),period:u32(a+4),post:u32(a+20),pitchOffset:0x97bd8+u32(a+24),stretchOffset:0x97bd8+u32(a+28),audioOffset:0x97bd8+u32(a+32)});}
    }
  }
  for(let i=0;i<rootCount;i++)branch(0,[]);if(pos!==0x5bfbc||leaves.length!==449)throw Error('Incomplete vocal database');
  const byAudio=new Map(leaves.map(x=>[x.audioOffset,x]));let a=0xa5648;const units=[];
  while(a<0x1bdecc){const fmt=u16(a),rate=u16(a+2),size=u32(a+4);if(![0x310,0x204].includes(fmt)||rate!==22050||!size||a+8+size>0x1bdecc)throw Error('Invalid audio record');
    const unit=byAudio.get(a);if(!unit)throw Error('Audio metadata link missing');const data=image.subarray(a+8,a+8+size);
    unit.pcm=fmt===0x204?decodeYamaha(data):Int16Array.from({length:size/2},(_,i)=>v.getInt16(a+8+i*2));
    if(unit.anchor+unit.post!==unit.pcm.length&&!(fmt===0x204&&unit.anchor+unit.post===unit.pcm.length+1))throw Error('Invalid unit extent');
    function control(o,lo,hi){if(o<lo||o+8>hi||u16(o)!==22050||u16(o+2)!==128)throw Error('Invalid control stream');const n=u32(o+4);if(n%4||!n||o+8+n>hi)throw Error('Invalid control extent');return Array.from({length:n/4},(_,i)=>v.getInt32(o+8+i*4));}
    unit.pitch=control(unit.pitchOffset,0x97bd8,0x9e910);unit.stretch=control(unit.stretchOffset,0x9e910,0xa5648);
    if(unit.pitch.length!==unit.stretch.length)throw Error('Mismatched control streams');
    units.push(unit);a=(a+8+size+3)&~3;
  }
  if(a!==0x1bdecc||units.length!==449||byAudio.size!==449)throw Error('Incomplete audio bank');
  const pronunciations=[];
  for(const off of [0x1db44,0x1dbac,0x1dc14,0x1dc7c,0x1dce0,0x1dd44]) {
    const prefix=[0xf0,0x43,0x79,9,0,0x50];if(prefix.some((x,i)=>b[off+i]!==x))throw Error('Pronunciation packet missing');
    let e=off+7;while(e<off+128&&b[e])e++;if(b[e]!==0||b[e+1]!==0xf7)throw Error('Invalid pronunciation packet');
    pronunciations.push(...new TextDecoder().decode(b.subarray(off+7,e)).split(','));
  }
  if(pronunciations.length!==128)throw Error('Expected 128 pronunciations');
  const curves=[];for(let o=0x1bdecc;o<0x1be180;){const n=u32(o+4);if(n%4||o+8+n>0x1be180)throw Error('Invalid vibrato curve');curves.push(Array.from({length:n/4},(_,i)=>v.getInt32(o+8+i*4)));o+=8+n;}
  // Factory defaults (controller address 0x2001de40). Bank 0 is replaced by
  // manual selection on the FM-1. Preserve banks 1..15, including user copies.
  const factoryLyrics=b.slice(0x1de40,0x1e240);
  validateFactoryLyrics(factoryLyrics);
  return {units,pronunciations,curves,factoryLyrics,sha256:hash};
}
export function validateFactoryLyrics(bytes) {
  if(bytes.length!==1024)throw Error('Invalid factory lyric bank size');
  for(let bank=0;bank<16;bank++){
    let end=0;while(end<64&&bytes[bank*64+end]<128)end++;
    if(!end||end===64||bytes.subarray(bank*64+end,(bank+1)*64).some(x=>x!==0x80))throw Error('Unsupported factory lyric return marker');
  }
}
export function packBank({units,pronunciations,curves,factoryLyrics}) {
  const bytes=new Uint8Array(CAPACITY),v=new DataView(bytes.buffer),w16=(o,x)=>v.setUint16(o,x,true),w32=(o,x)=>v.setUint32(o,x>>>0,true);
  const names=[...new Set(units.flatMap(u=>[u.source,u.target]).filter(Boolean))];
  let p=64+units.length*40;const nameOff=p;
  for(const s of names){bytes.set(new TextEncoder().encode(s),p);p+=8;}
  const pronOff=p;p+=128*4;
  for(let i=0;i<128;i++){w32(pronOff+i*4,p);const s=new TextEncoder().encode(pronunciations[i]);bytes.set(s,p);p+=s.length+1;}
  p=(p+3)&~3;const curveOff=p;p+=4*8;
  for(let i=0;i<4;i++){w32(curveOff+i*8,p);w32(curveOff+i*8+4,curves[i].length);for(const x of curves[i]){w32(p,x);p+=4;}}
  let squared=0,signal=0,total=0;
  for(let i=0;i<units.length;i++){
    const u=units[i],d=64+i*40,blocks=Math.ceil(u.pcm.length/128),audioBytes=u.kind?blocks*68:u.pcm.length*2;p=(p+3)&~3;
    w32(d,p);w32(d+4,u.pcm.length);w32(d+8,u.anchor);w32(d+12,u.period);w32(d+16,p+audioBytes);w16(d+20,u.pitch.length);w16(d+22,blocks);
    bytes[d+24]=names.indexOf(u.source);bytes[d+25]=u.target?names.indexOf(u.target):255;bytes[d+26]=u.variant;bytes[d+27]=u.kind;
    w32(d+28,p+audioBytes+u.pitch.length*4);w32(d+32,u.post);bytes[d+36]=u.kind?1:0;
    if(!u.kind){for(const x of u.pcm){w16(p,x&65535);p+=2;signal+=x*x;total++;}}
    else for(let b=0;b<blocks;b++){const pcm=u.pcm.subarray(b*128,Math.min(u.pcm.length,b*128+128)),packed=encodeBlock(pcm);bytes.set(packed,p);p+=68;const decoded=decodeBlock(packed);for(let k=0;k<pcm.length;k++){squared+=(decoded[k]-pcm[k])**2;signal+=pcm[k]**2;total++;}}
    for(const x of u.pitch){w32(p,x);p+=4;}for(const x of u.stretch){w32(p,x);p+=4;}
  }
  if(factoryLyrics){validateFactoryLyrics(factoryLyrics);p=(p+3)&~3;w32(48,p);w32(52,1024);bytes.set(factoryLyrics,p);p+=1024;}
  if(p>CAPACITY)throw Error(`Voice bank ${p} exceeds the ${CAPACITY}-byte slot`);
  bytes.set(new TextEncoder().encode('NSXV'),0);w32(4,2);w32(8,p);w32(16,units.length);w32(20,names.length);w32(24,nameOff);w32(28,pronOff);w32(32,curveOff);w32(36,22050);w32(40,128);w32(44,68);
  w32(12,crc32(bytes.subarray(0,p)));
  return {bytes,used:p,samples:total,snrDB:10*Math.log10(signal/Math.max(1,squared)),names};
}
