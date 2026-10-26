// SPDX-License-Identifier: GPL-3.0-only
// Generated tones and invented syllable order only; no NSX39 data.
import {packBank} from '../web/lib/bank.js';
export function fakeLyrics(){const b=new Uint8Array(1024).fill(0x80);for(let i=0;i<16;i++)b.set([i*5,i*5+1,i*5+2],i*64);return b;}
export function synthetic(factoryLyrics){const units=Array.from({length:449},(_,i)=>({source:i<36||i===448?'a':'Sil',target:i<36?'':i===448?'Sil':'a',variant:i<36?i%3:0,kind:i<36?0:1,anchor:28,period:56<<16,post:28,pcm:Int16Array.from({length:56},(_,k)=>Math.round(12000*Math.sin(k*2*Math.PI/56))),pitch:[56<<16,56<<16],stretch:[56,56]}));return packBank({units,pronunciations:Array(128).fill('a'),curves:[[0,100<<16,0,-100<<16,0],[0,50<<16,0,-50<<16,0],[0,25<<16,0,-25<<16,0],[0,10<<16,0,-10<<16,0]],factoryLyrics}).bytes;}
