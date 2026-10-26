// SPDX-License-Identifier: GPL-3.0-only
class Voice extends AudioWorkletProcessor {
 constructor(){super();this.ready=false;this.index=128;this.phase=0;this.a=0;this.b=0;this.scope=new Float32Array(240);this.scopeIndex=0;this.ticks=0;this.port.onmessage=async({data:d})=>{
  if(d.init){try{this.e=(await WebAssembly.instantiate(d.wasm,{})).instance.exports;new Uint8Array(this.e.memory.buffer,this.e.web_bank_ptr(),d.bank.length).set(d.bank);if(!this.e.web_init())throw Error('Voice bank rejected');this.buffer=new Int16Array(this.e.memory.buffer,this.e.web_audio_ptr(),128);this.ready=true;this.port.postMessage({ready:true});}catch(e){this.port.postMessage({error:e.message});}}
  if(!this.ready)return;
  if(d.select!==undefined)this.e.nsx_select(d.select);
  if(d.note!==undefined){this.e.nsx_note_on(d.note,127);this.port.postMessage({note:d.note,playing:d.selected});}
  if(d.off!==undefined)this.e.nsx_note_off(d.off);
  if(d.panic)this.e.nsx_midi(0xb0,120,0);
 };}
 next(){if(this.index>=128){this.e.web_render();this.index=0;}return this.buffer[this.index++]/32768;}
 process(_inputs,outputs){const out=outputs[0];if(!this.ready)return true;for(let i=0;i<out[0].length;i++){this.phase+=22050/sampleRate;if(this.phase>=1){this.phase-=1;this.a=this.b;this.b=this.next();}const s=(this.a+(this.b-this.a)*this.phase)*0.6;for(const ch of out)ch[i]=s;this.scope[this.scopeIndex++%240]=s;}
 if(++this.ticks%32===0)this.port.postMessage({wave:this.scope,scopeIndex:this.scopeIndex});return true;}
}
registerProcessor('nsx-voice',Voice);
