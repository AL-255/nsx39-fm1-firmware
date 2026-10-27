// SPDX-License-Identifier: GPL-3.0-only
import {crc32,validateFactoryLyrics} from './bank.js';
export const STATE_BYTES=1120;
export const DEFAULT_VALUES=[0,0,100,100,2,64,0,0,1,0,0,64,1,0,0,0,0,0];
export const VALUE_LIMITS=[2000,2000,100,4000,3,127,127,16383,1,1,127,127,2,1,17,127,2,2];
export const STORAGE_KEY='nsx39.fm1.memories.v1';
export function lyricArrays(table){validateFactoryLyrics(table);return Array.from({length:16},(_,i)=>[...table.slice(i*64,(i+1)*64)].filter(v=>v<128));}
export function lyricTable(lyrics){
 if(!Array.isArray(lyrics)||lyrics.length!==16)throw Error('Expected 16 lyric memories');
 const table=new Uint8Array(1024).fill(128);
 lyrics.forEach((ids,slot)=>{if(!Array.isArray(ids)||ids.length<1||ids.length>63||ids.some(id=>!Number.isInteger(id)||id<0||id>127))throw Error(`Memory ${slot}: use 1–63 syllables`);table.set(ids,slot*64);});return table;
}
export function encodeMemory(config,seed=0,sequence=0,version=2){
 const values=config.values||DEFAULT_VALUES;if(values.length!==18||values.some((v,i)=>!Number.isInteger(v)||v<0||v>VALUE_LIMITS[i]))throw Error('Invalid parameter settings');
 if(version!==1&&version!==2)throw Error('Unsupported settings record');
 if(version===1&&(values[14]===1||values[14]===2))throw Error('DoReMi and Lyrics 00 require FM-1_944 or newer');
 const wire=[...values];if(version===1&&wire[14])wire[14]-=2;
 const bytes=new Uint8Array(STATE_BYTES),v=new DataView(bytes.buffer);bytes.set([78,83,88,83]);v.setUint32(4,version,true);v.setUint32(8,STATE_BYTES,true);v.setUint32(16,seed,true);v.setUint32(20,sequence,true);
 wire.forEach((n,i)=>v.setUint16(32+i*2,n,true));bytes.set(lyricTable(config.lyrics),96);v.setUint32(12,crc32(bytes),true);return bytes;
}
export function decodeMemory(input){
 const b=new Uint8Array(input),v=new DataView(b.buffer,b.byteOffset,b.byteLength);if(b.length!==STATE_BYTES||v.getUint32(0,true)!==0x5358534e||![1,2].includes(v.getUint32(4,true))||v.getUint32(8,true)!==STATE_BYTES)throw Error('Unsupported settings record');
 const copy=b.slice(),d=new DataView(copy.buffer),sum=d.getUint32(12,true);d.setUint32(12,0,true);if(crc32(copy)!==sum)throw Error('Settings checksum mismatch');
 if(b.slice(24,32).some(Boolean)||b.slice(68,96).some(Boolean))throw Error('Unsupported settings fields');
 const config={values:Array.from({length:18},(_,i)=>v.getUint16(32+i*2,true)),lyrics:lyricArrays(b.slice(96))};if(v.getUint32(4,true)===1){if(config.values[14]>15)throw Error('Invalid legacy mode');if(config.values[14])config.values[14]+=2;}encodeMemory(config);return config;
}
export function parseLyrics(text,labels){
 const normalize=s=>s.normalize('NFKC').toLowerCase().replace(/[ァ-ヶ]/g,c=>String.fromCharCode(c.charCodeAt(0)-0x60));
 const map=new Map();labels.forEach(([roman,kana],id)=>{for(const s of [roman,kana]){const key=normalize(s).replace(/\s/g,'');if(!map.has(key))map.set(key,id);}});map.set('を',labels.findIndex(([s])=>s==='wo'));
 const keys=[...map.keys()].sort((a,b)=>b.length-a.length),ids=[];
 const parts=normalize(text).replace(/\bn\s+\((m|ng|ny|n)\)/g,'n($1)').split(/[\s'’ʼ,、。・.!?！？]+/).filter(Boolean);
 for(const s of parts){let p=0;while(p<s.length){if(s[p]==='#'){const m=/^#(\d{1,3})/.exec(s.slice(p));if(!m||Number(m[1])>127)throw Error(`Invalid syllable ID near ${s.slice(p,p+8)}`);ids.push(Number(m[1]));p+=m[0].length;}else{const k=keys.find(k=>s.startsWith(k,p));if(!k)throw Error(`Unknown syllable near “${s.slice(p,p+8)}”`);ids.push(map.get(k));p+=k.length;}}}
 if(ids.length<1||ids.length>63)throw Error('Use 1–63 syllables per memory');return ids;
}
export function loadMemory(storage){const raw=storage.getItem(STORAGE_KEY);if(!raw)return null;const data=JSON.parse(raw);if(![1,2].includes(data.version))throw Error('Unsupported saved memory version');if(data.version===1&&data.values[14])data.values[14]+=2;encodeMemory(data);return {values:data.values,lyrics:data.lyrics};}
export function saveMemory(storage,config){encodeMemory(config);storage.setItem(STORAGE_KEY,JSON.stringify({version:2,...config}));}
