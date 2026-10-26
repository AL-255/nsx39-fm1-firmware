// SPDX-License-Identifier: GPL-3.0-only
import {Updater} from './lib/fm1ota.js';
const $=id=>document.getElementById(id);
let template,manifest,result,worker,downloadURL,audio,node,selected=0,octave=1,installing=false;
const status=message=>{$('status').textContent=message;};
const held=new Map();let lastNote;
function panic(){held.clear();lastNote=undefined;node?.port.postMessage({panic:true});$('playing').textContent='Released';document.querySelectorAll('.keys button').forEach(b=>b.classList.remove('down'));}
function noteOn(key,offset){if(!node||installing)return;const note=53+12*octave+offset;held.set(key,note);lastNote=note;node.port.postMessage({note,selected});$('playing').textContent=`Playing #${selected}: ${result.pronunciations[selected]}`;$('pitch').textContent=`MIDI ${note}`;document.querySelector(`[data-offset="${offset}"]`)?.classList.add('down');}
function noteOff(key){const old=held.get(key);held.delete(key);if(old===undefined)return;document.querySelectorAll('.keys button').forEach(b=>b.classList.remove('down'));const notes=[...held.values()];if(notes.length){lastNote=notes.at(-1);node.port.postMessage({note:lastNote,selected});}else{node?.port.postMessage({off:old});lastNote=undefined;$('playing').textContent='Released';}}
function controls(){const ready=!!result&&!installing;$('install').disabled=!ready||!$('ack').checked||!navigator.requestMIDIAccess;$('generate').disabled=!template||!$('dump').files.length||installing;$('dump').disabled=installing;$('listen').disabled=!ready||installing;}
try{const [a,b]=await Promise.all([fetch('assets/template.fwsc'),fetch('assets/manifest.json')]);if(!a.ok||!b.ok)throw Error('Build assets are unavailable');template=await a.arrayBuffer();manifest=await b.json();status('Choose your NSX39 dump to begin.');controls();}catch(e){status(`Cannot load firmware build: ${e.message}`);}
$('dump').onchange=()=>{panic();result=null;$('result').hidden=true;$('preview').hidden=true;$('listen').textContent='Start audio preview';worker?.terminate();if(downloadURL)URL.revokeObjectURL(downloadURL);status('Ready to validate your dump.');controls();};
$('generate').onclick=async()=>{
 panic();worker?.terminate();$('generate').disabled=true;result=null;$('result').hidden=true;controls();$('generate').disabled=true;
 try{const f=$('dump').files[0];if(!f||![0x200000,0x200110].includes(f.size))throw Error('Expected a 2 MiB NSX39 SPI dump, with or without the 272-byte trailer.');const dump=await f.arrayBuffer();const copy=template.slice(0);worker=new Worker('worker.js',{type:'module'});worker.onerror=()=>{status('Generation worker failed. Reload and try again.');controls();};worker.onmessage=({data})=>{
  if(data.message)status(data.message);if(data.error){status(data.error);worker.terminate();controls();return;}if(!data.result)return;
  result=data.result;worker.terminate();if(downloadURL)URL.revokeObjectURL(downloadURL);downloadURL=URL.createObjectURL(new Blob([result.bytes],{type:'application/octet-stream'}));$('download').href=downloadURL;
  $('stats').textContent=`Ready: 128 syllables, 449 vocal units. Voice bank ${(result.bankBytes/1024).toFixed(1)} KiB. Package ${(result.bytes.length/1024).toFixed(1)} KiB.`;
  $('details').textContent=`Build ${manifest.version} · ${result.product}\nSHA-256 ${result.sha256}\nNative vocals; experimental timing and expression\nTransition codec: IMA ADPCM, 128-sample independent blocks\nAll-source codec SNR: ${result.snrDB.toFixed(2)} dB; sustained cycles exact\nPhysical hardware validation: pending`;
  $('result').hidden=false;$('syllable').replaceChildren(...result.pronunciations.map((p,i)=>{const o=document.createElement('option');o.value=i;o.textContent=`${i.toString().padStart(3,'0')} — ${p}`;return o;}));selected=0;status('Firmware generated. Listen, download, or explicitly start installation.');controls();
 };worker.postMessage({dump,template:copy,manifest},[dump,copy]);
 }catch(e){status(e.message);controls();}
};
function draw(wave,start){const c=$('scope'),x=c.getContext('2d');x.fillStyle='#08120c';x.fillRect(0,0,c.width,c.height);x.strokeStyle='#294334';x.beginPath();x.moveTo(0,90);x.lineTo(720,90);x.stroke();x.strokeStyle='#8bdcb0';x.beginPath();for(let i=0;i<240;i++){const y=90-wave[(start+i)%240]*78;i?x.lineTo(i*3,y):x.moveTo(0,y);}x.stroke();}
$('listen').onclick=async()=>{
 try{panic();if(audio)await audio.close();audio=new AudioContext();await audio.audioWorklet.addModule('voice-worklet.js');node=new AudioWorkletNode(audio,'nsx-voice',{numberOfInputs:0,numberOfOutputs:1,outputChannelCount:[2]});node.connect(audio.destination);node.port.onmessage=({data})=>{if(data.ready)node.port.postMessage({select:selected});if(data.wave)draw(data.wave,data.scopeIndex%240);if(data.error){status(`Preview failed: ${data.error}`);panic();}};const r=await fetch('assets/voice-engine.wasm');if(!r.ok)throw Error('Preview engine unavailable');node.port.postMessage({init:true,wasm:await r.arrayBuffer(),bank:result.bank});await audio.resume();$('preview').hidden=false;$('listen').textContent='Restart audio preview';
 }catch(e){status(`Cannot start preview: ${e.message}`);}
};
$('syllable').onchange=()=>{selected=Number($('syllable').value);node?.port.postMessage({select:selected});};
const names=['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
for(let i=0;i<27;i++){const b=document.createElement('button'),n=65+i;b.type='button';b.dataset.offset=i;b.textContent=names[n%12];b.className=names[n%12].includes('#')?'black':'';b.setAttribute('aria-label',`Play ${names[n%12]}, keyboard position ${i+1}`);b.onpointerdown=e=>{e.preventDefault();b.setPointerCapture(e.pointerId);noteOn(e.pointerId,i);};b.onpointerup=b.onpointercancel=e=>noteOff(e.pointerId);$('keys').append(b);}
const mapping="awsedftgyhujkolp;'";
window.addEventListener('keydown',e=>{if(e.repeat||e.ctrlKey||e.altKey||e.metaKey||['INPUT','SELECT','TEXTAREA','BUTTON'].includes(e.target.tagName))return;const i=mapping.indexOf(e.key.toLowerCase());if(i>=0){e.preventDefault();noteOn(e.code,i);}});
window.addEventListener('keyup',e=>noteOff(e.code));window.addEventListener('blur',panic);document.addEventListener('visibilitychange',()=>{if(document.hidden)panic();});
$('oct-down').onclick=()=>{panic();octave=Math.max(0,octave-1);};$('oct-up').onclick=()=>{panic();octave=Math.min(2,octave+1);};$('panic').onclick=panic;$('ack').onchange=controls;
window.addEventListener('beforeunload',e=>{if(installing){e.preventDefault();e.returnValue='Firmware installation is in progress.';}});
$('install').onclick=async()=>{
 if(!result||installing||!$('ack').checked)return;installing=true;panic();controls();let wake;
 const progress=(phase,info)=>{$('install-status').textContent=({start:'Connected. Checking package…',verify:'Staging update loader…',loader:'Waiting for update loader…',write:'Checking and writing application sectors…',reboot:'Waiting for firmware to reboot…',done:`Installed: ${info}`})[phase]||phase;};
 try{wake=navigator.wakeLock?await navigator.wakeLock.request('screen').catch(()=>null):null;const access=await navigator.requestMIDIAccess({sysex:true});const updater=new Updater(access);if(!await updater.resume(result.image,progress,result.product))await updater.install(result.image,result.product,progress);}
 catch(e){$('install-status').textContent=`Installation stopped: ${e.message} Keep the generated package and follow the recovery instructions.`;}
 finally{await wake?.release();installing=false;controls();}
};
