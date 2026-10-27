// SPDX-License-Identifier: GPL-3.0-only
import test from 'node:test';import assert from 'node:assert/strict';import{readFile}from'node:fs/promises';import{existsSync}from'node:fs';
import{synthetic}from'./fixture.mjs';
const built=existsSync('build/voice-engine.wasm');
async function engine(){const{instance}=await WebAssembly.instantiate(await readFile('build/voice-engine.wasm'),{}),e=instance.exports,b=synthetic();new Uint8Array(e.memory.buffer,e.web_bank_ptr(),b.length).set(b);assert.equal(e.web_init(),1);return e;}
function state(e){return [...new Int32Array(e.memory.buffer,e.web_art_ptr(),19)];}
function render(e,blocks){while(blocks--)e.web_render();}
function nrpn(e,id,high,low){e.nsx_midi(0xb0,99,0x70);e.nsx_midi(0xb0,98,id);e.nsx_midi(0xb0,6,high);if(low!==undefined)e.nsx_midi(0xb0,38,low);}
test('ADSR has timed attack/decay, live sustain, bounded release and immediate panic',{skip:!built},async()=>{
 const e=await engine();e.nsx_adsr(0,100);e.nsx_adsr(1,100);e.nsx_adsr(2,50);e.nsx_adsr(3,100);
 e.nsx_note_on(69,127);render(e,8);let s=state(e);assert.equal(s[4],1);assert(s[5]>14000&&s[5]<16000);
 render(e,10);assert.equal(state(e)[4],2);render(e,20);s=state(e);assert.equal(s[4],3);assert.equal(s[5],16384);
 e.nsx_adsr(2,25);render(e,1);assert.equal(state(e)[5],8192);
 e.nsx_note_off(69);assert.equal(state(e)[4],4);render(e,8);assert(state(e)[5]>0&&state(e)[5]<8192);render(e,12);assert.equal(state(e)[4],0);assert.equal(e.nsx_active(),0);
 e.nsx_adsr(0,0);e.nsx_adsr(1,0);e.nsx_adsr(2,100);e.nsx_adsr(3,4000);e.nsx_note_on(69,127);render(e,1);e.nsx_note_off(69);render(e,200);assert.equal(state(e)[4],4);e.nsx_midi(0xb0,120,0);assert.equal(e.nsx_active(),0);assert.equal(state(e)[5],0);
});
test('recorded release unit is selected on key-up, sustain defers it, revoice preserves the envelope',{skip:!built},async()=>{
 const e=await engine();e.nsx_adsr(0,100);e.nsx_note_on(69,127);render(e,5);const level=state(e)[5];e.nsx_revoice(72,127);assert.equal(state(e)[5],level);assert.equal(state(e)[4],1);
 e.nsx_midi(0xb0,64,127);e.nsx_note_off(72);assert.equal(state(e)[17],0);assert.equal(state(e)[16],1);
 e.nsx_midi(0xb0,64,0);assert.equal(state(e)[4],4);assert.equal(state(e)[17],1);render(e,40);assert.equal(e.nsx_active(),0);
 e.nsx_adsr(3,0);e.nsx_note_on(69,127);render(e,2);e.nsx_note_off(69);assert.equal(e.nsx_active(),0);
});
test('original MIDI ranges, 14-bit vibrato delay and reserved NRPNs are respected',{skip:!built},async()=>{
 const e=await engine();assert.equal(e.nsx_articulation_get(3),2);assert.equal(e.nsx_articulation_get(4),64);assert.equal(e.nsx_articulation_get(10),64);assert.equal(e.nsx_articulation_get(12),1);
 nrpn(e,7,127,127);assert.equal(e.nsx_articulation_get(7),16383);nrpn(e,5,100);assert.equal(e.nsx_articulation_get(5),0);
 nrpn(e,3,127);assert.equal(e.nsx_articulation_get(3),3);nrpn(e,12,127);assert.equal(e.nsx_articulation_get(12),2);
 e.nsx_midi(0xb0,1,96);assert.equal(e.nsx_articulation_get(5),96);e.nsx_note_on(69,127);render(e,100);assert.equal(e.nsx_vibrato_q16(),0);
 nrpn(e,7,0,0);render(e,20);assert.notEqual(e.nsx_vibrato_q16(),0);e.nsx_midi(0xb0,1,0);render(e,1);assert.equal(e.nsx_vibrato_q16(),0);
});
test('all panel actions and four ADSR knobs act live without advancing the lyric',{skip:!built},async()=>{
 const e=await engine();const buttons=[[2,10],[3,1],[4,2],[6,3],[7,4],[8,7],[10,12],[11,8],[13,5]];
 for(const[button,control]of buttons){const before=e.nsx_articulation_get(control);e.nsx_panel_button(button);assert.notEqual(e.nsx_articulation_get(control),before);assert.equal(e.web_last(),128);}
 e.nsx_panel_button(5);assert.equal(e.nsx_articulation_get(5),0);e.nsx_panel_button(5);assert(e.nsx_articulation_get(5)>0);
 e.nsx_panel_button(12);assert.equal(state(e)[16],1);e.nsx_panel_button(12);assert.equal(state(e)[16],0);
 for(let i=0;i<4;i++){e.nsx_adsr_knob(i,1000);assert.equal(state(e)[i],i===2?100:i===3?4000:2000);e.nsx_adsr_knob(i,-1000);assert.equal(state(e)[i],0);}
 e.nsx_note_on(65,127);e.nsx_panel_button(9);assert.equal(e.nsx_active(),0);
});
test('retriggered long release reaches exact silence and stays silent while idle',{skip:!built},async()=>{
 for(const release of[1000,4000]){
  const e=await engine();e.nsx_adsr(0,80);e.nsx_adsr(1,60);e.nsx_adsr(2,65);e.nsx_adsr(3,release);
  for(const note of[60,64,67,72,69]){e.nsx_note_on(note,127);render(e,20);e.nsx_note_off(note);render(e,6);}
  render(e,Math.ceil(release*22050/1000/128)+4);assert.equal(e.nsx_active(),0);assert.equal(state(e)[4],0);assert.equal(state(e)[5],0);
  const pcm=new Int16Array(e.memory.buffer,e.web_audio_ptr(),128);
  // Twenty seconds covers repeated idle-save intervals as well as stale tails.
  for(let k=0;k<Math.ceil(20*22050/128);k++){e.web_render();assert(pcm.every(s=>s===0),'audio returned after the release ended');}
 }
});
