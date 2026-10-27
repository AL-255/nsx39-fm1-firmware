// SPDX-License-Identifier: GPL-3.0-only
import {DeviceMemory} from './device-memory.js';
export async function withDevice(owner,action){
 if(!navigator.requestMIDIAccess)throw Error('Use Chrome or Edge with SysEx MIDI access.');
 document.dispatchEvent(new CustomEvent('fm1-midi-busy',{detail:{owner,busy:true}}));let device;
 try{device=await DeviceMemory.connect(await navigator.requestMIDIAccess({sysex:true}));return await action(device);}
 finally{await device?.close();document.dispatchEvent(new CustomEvent('fm1-midi-busy',{detail:{owner,busy:false}}));}
}
