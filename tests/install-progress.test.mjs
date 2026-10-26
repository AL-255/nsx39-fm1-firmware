// SPDX-License-Identifier: GPL-3.0-only
import test from 'node:test';import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';import {existsSync} from 'node:fs';
import {InstallProgress,renderInstallProgress} from '../web/lib/install-progress.js';
import {Package} from '../web/lib/fm1pkg.js';
const built=existsSync('build/template.fwsc');
async function fixture(){const bytes=await readFile('build/template.fwsc'),pkg=new Package(bytes);let time=0;const view=new InstallProgress(bytes,()=>time);let phase='',sent=0,requests=0;
 return {view,pkg,tick:ms=>time+=ms,send(p,address,length){if(phase!==p){phase=p;sent=requests=0;}sent+=length;requests++;time+=10;view.event(p,requests,{address,length,bytesSent:sent,requests,elapsedMs:time});}};
}
test('loader coverage ignores duplicate requests while traffic totals include them',{skip:!built},async()=>{
 const f=await fixture(),ota=f.pkg.entries.find(e=>e.type===100);
 f.view.event('start','FM-1_939',{port:'Felucca'});f.send('verify',0,512);assert.equal(f.view.snapshot().percent,0);
 f.send('verify',ota.dataOff,512);const percent=f.view.snapshot().percent;
 f.send('verify',ota.dataOff,512);assert.equal(f.view.snapshot().percent,percent);assert.equal(f.view.sent,1536);
 for(let i=0;i<ota.size;i+=512)f.send('verify',ota.dataOff+i,Math.min(512,ota.size-i));
 assert.equal(f.view.snapshot().percent,99);f.view.event('loader');assert.equal(f.view.snapshot().percent,null);assert.match(f.view.snapshot().device,/FM-1_939.*Felucca/);
});
test('package verification and writing have separate bars and never report installation success early',{skip:!built},async()=>{
 const f=await fixture(),fl=f.pkg.flashEntry;
 f.view.event('resume','ota-FM-1_900',{port:'Felucca Update'});f.view.event('write',0,{device:'ota-FM-1_900'});
 assert.match(f.view.snapshot().title,/Verify firmware/);f.send('write',fl.dataOff+0x4000,32);
 for(let i=0;i<fl.size;i+=512)f.send('write',fl.dataOff+i,Math.min(512,fl.size-i));
 assert.equal(f.view.snapshot().percent,99);assert.match(f.view.snapshot().title,/Verify firmware/);
 f.send('write',fl.dataOff+fl.size-512,512);assert.match(f.view.snapshot().title,/Verify firmware/);
 for(let i=0x4000;i<fl.size;i+=512)f.send('write',fl.dataOff+i,Math.min(512,fl.size-i));
 assert.match(f.view.snapshot().title,/Write and verify/);assert.equal(f.view.snapshot().percent,99);
 const before=f.view.snapshot().transfer;f.send('write',fl.dataOff+0x4000,512);assert.equal(f.view.written.done,fl.size-0x4000);assert.notEqual(f.view.snapshot().transfer,before);
 f.view.event('reboot');assert.equal(f.view.snapshot().percent,null);assert.equal(f.view.snapshot().state,'running');
 f.tick(5000);assert.match(f.view.snapshot().timing,/Last response 5s ago/);
 f.view.event('done','FM-1_941');assert.equal(f.view.snapshot().percent,100);assert.equal(f.view.snapshot().state,'done');assert.match(f.view.snapshot().timing,/Last response 0s ago/);
});
test('errors freeze progress and elapsed time and retain transfer details',{skip:!built},async()=>{
 const f=await fixture();f.send('write',f.pkg.flashEntry.dataOff,512);f.tick(2000);f.view.stop('Cable disconnected. Keep the package.');
 const state=f.view.snapshot();assert.equal(state.state,'error');assert(state.percent<100);assert.match(state.description,/Cable disconnected/);
 f.tick(10000);f.view.event('done','FM-1_941');assert.deepEqual(f.view.snapshot(),state);
});
test('render uses an indeterminate progress bar while reconnecting, and a full bar only on success',{skip:!built},async()=>{
 const f=await fixture(),elements=new Map();
 const root={getElementById:id=>{if(!elements.has(id))elements.set(id,{dataset:{},value:17,removeAttribute(name){delete this[name];}});return elements.get(id);}};
 renderInstallProgress(root,f.view.snapshot());assert.equal(elements.get('install-bar').value,undefined);assert.equal(elements.get('install-progress').hidden,false);
 f.view.event('done','FM-1_941');renderInstallProgress(root,f.view.snapshot());assert.equal(elements.get('install-bar').value,100);assert.equal(elements.get('install-percent').textContent,'100%');assert.match(elements.get('install-status').textContent,/expected firmware identity/);
});
