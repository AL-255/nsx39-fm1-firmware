// SPDX-License-Identifier: GPL-3.0-only
// Private reference check. Requires an existing original-instruction VM from
// NSX39-Disasm and a locally generated bank. No Yamaha bytes are published.
import{readFile}from'node:fs/promises';import assert from'node:assert/strict';
const root=process.argv[2];if(!root)throw Error('Usage: node tools/test_articulation_reference.mjs /path/to/NSX39-Disasm');
const {instance}=await WebAssembly.instantiate(await readFile(root+'/analysis/generated/portable_voice/voice-engine.wasm'),{}),v=instance.exports;
v.vm_init();for(let i=0;i<449;i++)v.vm_set_loaded(i);
function call(pc,...args){v.vm_begin(pc,...[...args,0,0,0,0].slice(0,4));let status;while((status=v.vm_run(10000000))===1);assert.equal(status,0,`VM ${status} at ${v.vm_error_pc().toString(16)} addr ${v.vm_error_address().toString(16)}`);return v.vm_reg(0)>>>0;}
const mem=new DataView(v.memory.buffer),ptr=(a,n=4)=>v.vm_pointer(a,n),write=(a,x)=>mem.setUint32(ptr(a),x>>>0,false),read=a=>mem.getUint32(ptr(a),false);
call(0x040888f8,0x041f04c0);call(0x04096bc0);
const ctx=0x041f04c0+0x6f4,dest=0x060e0000,scalar=x=>(((x*0x10102+0x8000)>>>16)*512)>>>0;
const {instance:native}=await WebAssembly.instantiate(await readFile('build/voice-engine.wasm'),{}),e=native.exports;
const bank=await readFile('local/voice.bank');new Uint8Array(e.memory.buffer,e.web_bank_ptr(),bank.length).set(bank);
const base=call(0x0409129c,440*65536);
let cases=0;
for(const type of [0,1,2,3])for(const [rate,depth]of[[1,1],[32,64],[64,96],[127,127]]){
 assert.equal(e.web_init(),1);e.nsx_articulation(3,type);e.nsx_articulation(4,rate);e.nsx_articulation(5,depth);e.nsx_note_on(69,127);
 write(ctx+4+0x81c4,scalar(depth));write(ctx+4+0x81c8,scalar(rate));write(ctx+4+0x81cc,type);write(ctx+0x9430,0);write(ctx+0x9434,0);write(ctx+0x9438,0);
 for(let frame=0;frame<256;frame++){
  write(dest+0x40,440*65536);call(0x0409330e,ctx,frame,dest);const actual=read(dest+0x40);
  e.web_render();const q=e.nsx_vibrato_q16(),expected=call(0x04091276,(base+q)>>>0);
  assert.equal(actual,expected,`type=${type} rate=${rate} depth=${depth} frame=${frame} delta=${q}`);
 }
 cases++;
}
for(const delay of [0,123,1000]){
 assert.equal(e.web_init(),1);e.nsx_articulation(3,2);e.nsx_articulation(4,64);e.nsx_articulation(7,delay);e.nsx_note_on(69,127);
 write(ctx+4+0x81c8,scalar(64));write(ctx+4+0x81cc,2);write(ctx+0x9430,0);write(ctx+0x9434,0);write(ctx+0x9438,0);
 for(let frame=0;frame<256;frame++){
  const depth=frame<Math.floor(delay*22050/128000)||(frame>=190&&frame<210)?0:96;
  e.nsx_articulation(5,96); // The original outer delay wrapper suppresses depth until ready.
  if(frame>=190&&frame<210)e.nsx_articulation(5,0);
  write(ctx+4+0x81c4,scalar(depth));write(dest+0x40,440*65536);call(0x0409330e,ctx,frame,dest);const actual=read(dest+0x40);
  e.web_render();const expected=depth?call(0x04091276,(base+e.nsx_vibrato_q16())>>>0):440*65536;assert.equal(actual,expected,`delay=${delay} frame=${frame}`);
 }
 cases++;
}
const report={cases,frames:cases*256,originalFrequencyOutputsMatch:true,reference:'SH 0409330e; original cents/frequency conversions, including delayed onset and live depth toggles'};console.log(report);
await (await import('node:fs/promises')).writeFile('local/articulation-reference.json',JSON.stringify(report,null,2)+'\n');
