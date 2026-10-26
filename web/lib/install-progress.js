// SPDX-License-Identifier: GPL-3.0-only
import {Package} from './fm1pkg.js';

// Count distinct bytes in each requested range. Retries and overlapping reads
// contribute to traffic statistics, but never advance the stage twice.
class Coverage {
 constructor(start,size){this.start=start;this.size=size;this.seen=new Uint8Array(size);this.done=0;}
 add(address,length){
  const lo=Math.max(0,address-this.start),hi=Math.min(this.size,address+length-this.start);
  for(let i=lo;i<hi;i++)if(!this.seen[i]){this.seen[i]=1;this.done++;}
 }
 get complete(){return this.done===this.size;}
}
const kib=n=>`${(n/1024).toFixed(1)} KiB`;
const duration=ms=>{const s=Math.floor(ms/1000);return `${Math.floor(s/60)}:${String(s%60).padStart(2,'0')}`;};

export class InstallProgress {
 constructor(bytes,now=()=>performance.now()){
  const pkg=new Package(bytes),loader=pkg.entries.find(e=>e.type===100);
  if(!loader)throw Error('Update loader missing from package');
  this.loader=new Coverage(loader.dataOff,loader.size);
  this.checked=new Coverage(pkg.flashEntry.dataOff,pkg.flashSize);
  this.written=new Coverage(pkg.flashEntry.dataOff+0x4000,pkg.flashSize-0x4000);
  this.now=now;this.started=now();this.lastResponse=null;this.ended=null;
  this.phase='connect';this.device='';this.port='';this.sent=0;this.requests=0;this.writing=false;
  this.trafficPhase='';this.previousBytes=0;this.previousRequests=0;
 }
 event(phase,info,detail={}){
  if(this.ended!==null)return;
  if(phase==='start'||phase==='resume'){this.device=info;this.port=detail.port||'';this.lastResponse=this.now();}
  if(phase==='write'&&info===0){this.device=detail.device||this.device;this.port=detail.port||this.port;this.lastResponse=this.now();}
  if(detail.bytesSent!==undefined){
   if(this.trafficPhase!==phase){this.trafficPhase=phase;this.previousBytes=0;this.previousRequests=0;}
   this.sent+=Math.max(0,detail.bytesSent-this.previousBytes);this.previousBytes=detail.bytesSent;
   this.requests+=Math.max(0,detail.requests-this.previousRequests);this.previousRequests=detail.requests;
   this.lastResponse=this.now();
   if(phase==='verify')this.loader.add(detail.address,detail.length);
   if(phase==='write'){
    // The pinned Felucca loader completes its full CRC pass before requesting
    // application sectors again. A request for the next sector means the prior
    // one was handled; the final completion gate and identity still decide success.
    if(this.checked.complete&&detail.address===this.written.start)this.writing=true;
    if(this.writing)this.written.add(detail.address,detail.length);else this.checked.add(detail.address,detail.length);
   }
  }
  this.phase=phase;
  if(phase==='done'){this.device=info;this.lastResponse=this.now();this.ended=this.now();}
 }
 stop(message){this.failure=message;this.ended=this.now();}
 snapshot(){
  let title,description,coverage=null,percent=null;
  switch(this.phase){
   case 'start':case 'verify':
    title='1 / 4 · Stage update loader';description='The FM-1 is checking and staging its update loader.';coverage=this.loader;break;
   case 'loader':
    title='1 / 4 · Start update loader';description='Waiting for the update loader to reconnect over USB. Keep the FM-1 connected.';break;
   case 'resume':
    title='2 / 4 · Resume installation';description='Found the update loader. Rechecking the package before continuing.';break;
   case 'write':
    coverage=this.writing?this.written:this.checked;
    if(coverage===this.written){title='3 / 4 · Write and verify firmware';description='The loader writes changed sectors and verifies them on the FM-1.';}
    else{title='2 / 4 · Verify firmware package';description='The loader checks the complete firmware before erasing application sectors.';}
    break;
   case 'reboot':
    title='4 / 4 · Restart and confirm';description='Waiting for the FM-1 to reconnect and confirm its firmware identity. Keep USB connected.';break;
   case 'done':
    title=`Installed ${this.device}`;description='The FM-1 reconnected with the expected firmware identity.';percent=100;break;
   default:title='Connect to FM-1';description='Allow SysEx MIDI access in the browser. Looking for the FM-1 or a waiting update loader.';
  }
  // A full range sent is not a flash-write acknowledgement. Stay below 100%
  // until the device returns with the exact expected identity.
  if(coverage)percent=Math.min(99,Math.floor(coverage.done/coverage.size*100));
  if(this.failure){title='Installation stopped';description=this.failure;percent=percent??0;}
  const elapsed=(this.ended??this.now())-this.started;
  const transfer=coverage?`${kib(coverage.done)} / ${kib(coverage.size)} sent for this stage`:`${kib(this.sent)} sent`;
  const age=this.lastResponse===null?null:Math.max(0,Math.floor(((this.ended??this.now())-this.lastResponse)/1000));
  return {title,description,percent,device:[this.device,this.port].filter(Boolean).join(' · '),
   transfer:`${transfer} · ${this.requests} requests`,
   timing:`Elapsed ${duration(elapsed)}${elapsed>=1000&&this.sent?` · ${kib(this.sent/(elapsed/1000))}/s average`:''}${age===null?'':` · Last response ${age}s ago`}`,
   state:this.failure?'error':this.phase==='done'?'done':'running'};
 }
}

export function renderInstallProgress(root,state){
 const $=id=>root.getElementById(id);$('install-progress').hidden=false;
 $('install-progress').dataset.state=state.state;
 $('install-stage').textContent=state.title;$('install-status').textContent=state.description;
 const bar=$('install-bar');
 if(state.percent===null)bar.removeAttribute('value');else bar.value=state.percent;
 $('install-percent').textContent=state.percent===null?'Waiting…':`${state.percent}%`;
 $('install-device').textContent=state.device;
 $('install-transfer').textContent=state.transfer;$('install-timing').textContent=state.timing;
}
