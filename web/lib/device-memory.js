// SPDX-License-Identifier: GPL-3.0-only
import {crc32,sha256} from './bank.js';
import {pack7,unpack7,Updater} from './fm1ota.js';
import {encodeMemory,decodeMemory,STATE_BYTES,VALUE_LIMITS} from './memory.js';
const HEADER=[0x7d,78,83,88,1];
export function memoryPacket(command,id,payload=[]){const b=[...HEADER,command,id,...payload];b.push((-b.reduce((s,v)=>s+v,0))&127);return [0xf0,...b,0xf7];}
export function memoryResponse(packet,command,id){
 const b=Array.from(packet);if(b.length<10||b[0]!==240||b.at(-1)!==247)return null;const p=b.slice(1,-1);
 if(HEADER.some((v,i)=>p[i]!==v)||p[5]!==(command|64)||p[6]!==id||p.some(v=>v>127)||p.reduce((a,b)=>a+b,0)&127)return null;return p.slice(7,-1);
}
const messages={2:'Settings data was incomplete or invalid',3:'Release all keys and turn hold off before saving',4:'Unsupported memory command',5:'The FM-1 could not verify its settings write',6:'Settings storage contains another format; it was left intact. Open Device settings → Storage repair to back up and initialize it',7:'Storage or settings changed since the backup. Prepare a fresh backup before initializing'};
export class DeviceMemory{
 constructor(device){this.device=device;this.id=0;}
 static async connect(access){const u=new Updater(access),dev=await u.connect(id=>id.model==='FM-1');if(!dev)throw Error('FM-1 not found');const memory=new DeviceMemory(dev);if(dev.id.version<943){await memory.close();throw Error('Install FM-1_943 or newer before editing device memories or settings');}return memory;}
 async request(command,payload=[]){const id=this.id++%128,link=this.device.link;if(!link.send(memoryPacket(command,id,payload)))throw Error('The FM-1 MIDI port disconnected');const end=Date.now()+5000;
  while(Date.now()<end){const p=await link.read(Math.max(0,end-Date.now()));if(!p)break;const data=memoryResponse(p,command,id);if(data)return data;}throw Error('The FM-1 did not answer the memory request');
 }
 async readBytes(){const state=new Uint8Array(STATE_BYTES);for(let chunk=0;chunk<9;chunk++){const data=await this.request(1,[chunk]);const n=Math.min(128,STATE_BYTES-chunk*128),decoded=unpack7(data.slice(1));if(data[0]!==chunk||decoded.length!==n)throw Error('Invalid memory response');state.set(decoded,chunk*128);}decodeMemory(state);return state;}
 async read(){return decodeMemory(await this.readBytes());}
 async write(config){const state=encodeMemory(config,0,0,(this.device.id?.version??944)>=944?2:1);for(let chunk=0;chunk<9;chunk++){const reply=await this.request(2,[chunk,...pack7(state.slice(chunk*128,Math.min((chunk+1)*128,STATE_BYTES)))]);if(reply.length!==1||reply[0])throw Error(messages[reply[0]]||'Memory transfer rejected');}
  const reply=await this.request(3);if(reply.length!==1||reply[0])throw Error(messages[reply[0]]||'Memory save rejected');const actual=await this.read();if(JSON.stringify(actual.values)!==JSON.stringify(config.values)||JSON.stringify(actual.lyrics)!==JSON.stringify(config.lyrics))throw Error('Saved memory readback differs');return actual;
 }
 requireStorageRepair(){if((this.device.id?.version??0)<947)throw Error('Install FM-1_947 or newer to back up and initialize settings storage');}
 async backupStorage(){
  this.requireStorageRepair();const state=await this.readBytes(),storage=new Uint8Array(8192);
  for(let chunk=0;chunk<64;chunk++){const data=await this.request(4,[chunk]);if(data[0]===127)throw Error(messages[data[1]]||'Storage backup failed');const raw=unpack7(data.slice(1));if(data[0]!==chunk||raw.length!==128)throw Error('Invalid storage backup response');storage.set(raw,chunk*128);}
  const after=await this.readBytes();if(!after.every((v,i)=>v===state[i]))throw Error('Device settings changed during backup. Release keys and prepare a fresh backup');
  return {format:1,type:'nsx39-fm1-settings-backup',device:this.device.id.text,offset:0xe5000,storage:Array.from(storage),state:Array.from(state),storageSha256:await sha256(storage)};
 }
 async initializeStorage(backup){
  this.requireStorageRepair();
  if(backup?.format!==1||backup.type!=='nsx39-fm1-settings-backup'||backup.device!==this.device.id.text||backup.offset!==0xe5000||!Array.isArray(backup.storage)||backup.storage.length!==8192||!Array.isArray(backup.state)||backup.state.length!==1120||[...backup.storage,...backup.state].some(n=>!Number.isInteger(n)||n<0||n>255))throw Error('Invalid storage backup');
  const storage=new Uint8Array(backup.storage),state=new Uint8Array(backup.state);decodeMemory(state);if(await sha256(storage)!==backup.storageSha256)throw Error('Storage backup checksum mismatch');
  const proof=new Uint8Array(12),v=new DataView(proof.buffer);v.setUint32(0,crc32(storage),true);v.setUint32(4,new DataView(state.buffer).getUint32(12,true),true);proof.set([73,78,73,84],8);
  const reply=await this.request(5,pack7(proof));if(reply.length!==1||reply[0])throw Error(messages[reply[0]]||'Storage initialization failed');
  const actual=await this.readBytes();if(!actual.every((b,i)=>b===state[i]))throw Error('Initialized settings readback differs');return decodeMemory(actual);
 }
 async update({lyric,values}={}){
  const current=await this.read();
  if(lyric){if(!Number.isInteger(lyric.slot)||lyric.slot<0||lyric.slot>15)throw Error('Choose memory 00–15');current.lyrics[lyric.slot]=[...lyric.ids];}
  if(values)for(const [key,value]of Object.entries(values)){const i=Number(key);if(!Number.isInteger(i)||i<0||i>=18||!Number.isInteger(value)||value<0||value>VALUE_LIMITS[i])throw Error('Invalid device setting');current.values[i]=value;}
  encodeMemory(current);return this.write(current);
 }
 close(){this.device.link.close();return Promise.allSettled([this.device.link.input,this.device.link.output].filter(Boolean).map(p=>Promise.resolve().then(()=>p.close())));}
}
