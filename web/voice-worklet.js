// SPDX-License-Identifier: GPL-3.0-only
class Voice extends AudioWorkletProcessor {
 constructor(){super();this.ready=false;this.index=128;this.phase=0;this.a=0;this.b=0;this.scope=new Float32Array(240);this.scopeIndex=0;this.ticks=0;this.port.onmessage=async({data:d})=>{
  if(d.init){try{this.e=(await WebAssembly.instantiate(d.wasm,{})).instance.exports;if(typeof this.e.web_art_ptr!=='function'||typeof this.e.web_state_apply!=='function')throw Error('Preview engine version mismatch. Reload this page.');new Uint8Array(this.e.memory.buffer,this.e.web_bank_ptr(),d.bank.length).set(d.bank);if(!this.e.web_init())throw Error('Voice bank rejected');this.buffer=new Int16Array(this.e.memory.buffer,this.e.web_audio_ptr(),128);this.ready=true;this.port.postMessage({ready:true});}catch(e){this.port.postMessage({error:e.message});}}
  if(!this.ready)return;
  let changed=false,persistent=false;
  if(d.memory){const p=this.e.web_state_ptr();new Uint8Array(this.e.memory.buffer,p,1120).set(d.memory);if(!this.e.web_state_apply()){this.port.postMessage({error:'Invalid settings record'});return;}changed=persistent=true;}
  if(d.select!==undefined){this.e.performance_manual(d.select);changed=persistent=true;}
  if(d.lyric!==undefined){this.e.performance_mode(d.lyric);changed=persistent=true;}
  if(d.rewind){this.e.performance_select(-1);changed=true;}
  if(d.adsr){this.e.nsx_adsr(d.adsr[0],d.adsr[1]);changed=persistent=true;}
  if(d.button!==undefined){this.e.nsx_panel_button(d.button);changed=persistent=true;}
  if(d.note!==undefined){if(d.revoice)this.e.performance_revoice(d.note,127);else this.e.performance_trigger(d.note,127);changed=true;}
  if(changed)this.port.postMessage({state:true,last:this.e.web_last(),next:this.e.web_next(),upcoming:Array.from({length:this.e.instrument_value(2)>=2?32:1},(_,i)=>this.e.performance_upcoming(i)),position:this.e.web_position(),articulation:this.articulation(),memory:persistent?Array.from(new Uint8Array(this.e.memory.buffer,this.e.web_state_ptr(),1120)):undefined});
  if(d.off!==undefined)this.e.nsx_note_off(d.off);
  if(d.panic){const hold=d.keepHold&&this.articulation()[16];this.e.nsx_midi(0xb0,120,0);if(hold)this.e.nsx_midi(0xb0,64,127);}
 };}
 articulation(){return Array.from(new Int32Array(this.e.memory.buffer,this.e.web_art_ptr(),19));}
 next(){if(this.index>=128){this.e.web_render();this.index=0;}return this.buffer[this.index++]/32768;}
 process(_inputs,outputs){const out=outputs[0];if(!this.ready)return true;for(let i=0;i<out[0].length;i++){this.phase+=22050/sampleRate;if(this.phase>=1){this.phase-=1;this.a=this.b;this.b=this.next();}const s=(this.a+(this.b-this.a)*this.phase)*0.6;for(const ch of out)ch[i]=s;this.scope[this.scopeIndex++%240]=s;}
 if(++this.ticks%32===0)this.port.postMessage({wave:this.scope,scopeIndex:this.scopeIndex,articulation:this.articulation()});return true;}
}
registerProcessor('nsx-voice',Voice);
