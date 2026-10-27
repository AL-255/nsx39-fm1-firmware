// SPDX-License-Identifier: GPL-3.0-only
import {Updater} from './lib/fm1ota.js';
import {InstallProgress,renderInstallProgress} from './lib/install-progress.js';
import {encodeMemory,decodeMemory,loadMemory} from './lib/memory.js';
const $=id=>document.getElementById(id);
let template,manifest,result,worker,downloadURL,audio,node,selected=0,octave=0,installing=false,lyric=0,labels,configuration,busLocked=false,generating=false;
let lastPlayed=128,upcoming=[0];
const PREVIEW_KEY='nsx39.fm1.preview.v2';
const active=()=>!$('firmware-panel').hidden;
const status=message=>{$('status').textContent=message;};
const label=id=>id<128?`${labels[id][1]} / ${labels[id][0]}`:'—';
const scriptLabel=id=>id<128?(configuration?.values[16]===2?labels[id][0]:configuration?.values[16]===1?labels[id][1].replace(/[ぁ-ゖ]/g,c=>String.fromCharCode(c.charCodeAt(0)+0x60)):labels[id][1]):'—';
function syllableDisplay(){$('last-syllable').textContent=scriptLabel(lastPlayed);$('next-syllable').textContent=configuration?.values[14]===1?'By pitch':upcoming.map(scriptLabel).join(configuration?.values[16]===2?' ':'');}
function remember(){if(!configuration)return;try{localStorage.setItem(PREVIEW_KEY,JSON.stringify(configuration.values));}catch(e){status('Cannot remember preview settings: '+e.message);}controls();}
function previewMemory(){if(configuration)node?.port.postMessage({memory:encodeMemory(configuration)});}
function syncPreviewControls(){if(!configuration)return;selected=configuration.values[15];lyric=configuration.values[14];octave=configuration.values[17];$('preview-script').value=configuration.values[16];$('syllable').value=selected;$('lyric').value=lyric;lyricControls();for(let i=0;i<4;i++){$('adsr-'+i).value=configuration.values[i];$('adsr-value-'+i).textContent=configuration.values[i]+(i===2?'%':' ms');}syllableDisplay();}
function updateDownload(){if(downloadURL)URL.revokeObjectURL(downloadURL);downloadURL=URL.createObjectURL(new Blob([result.bytes],{type:'application/octet-stream'}));$('download').href=downloadURL;$('details').textContent=`Build ${manifest.version} · ${result.product}\nSHA-256 ${result.sha256}\nVoice bank ${(result.bankBytes/1024).toFixed(1)} KiB · ADPCM SNR ${result.snrDB.toFixed(2)} dB\nPhysical validation of this build: pending`;}
function lyricControls(){$('syllable').disabled=lyric!==0||installing||busLocked;$('rewind').disabled=lyric<2||installing||busLocked;}
const held=new Map();let lastNote;
function panic(persist=false){held.clear();lastNote=undefined;node?.port.postMessage({panic:true,keepHold:persist!==true});if(persist===true&&configuration?.values[13]){configuration.values[13]=0;remember();}$('playing').textContent='Released';document.querySelectorAll('.keys button').forEach(b=>b.classList.remove('down'));}
function noteOn(key,offset){if(!active()||!node||installing||busLocked)return;const note=53+12*octave+offset;held.set(key,note);lastNote=note;node.port.postMessage({note,selected});$('playing').textContent='Playing';$('pitch').textContent=`MIDI ${note}`;document.querySelector(`[data-offset="${offset}"]`)?.classList.add('down');}
function noteOff(key){const old=held.get(key);held.delete(key);if(old===undefined)return;document.querySelectorAll('.keys button').forEach(b=>b.classList.remove('down'));const notes=[...held.values()];if(notes.length){lastNote=notes.at(-1);node.port.postMessage({note:lastNote,revoice:true});}else{node?.port.postMessage({off:old});lastNote=undefined;$('playing').textContent='Released';}}
function controls(){const ready=!!result&&!installing&&!busLocked;$('install').disabled=!ready||!$('ack').checked||!navigator.requestMIDIAccess;$('ack').disabled=installing;$('generate').disabled=!template||!$('dump').files.length||installing||busLocked||generating;$('dump').disabled=installing||generating;$('listen').disabled=!result||installing||busLocked;for(const e of document.querySelectorAll('#preview button,#preview input,#preview select'))e.disabled=installing||busLocked;lyricControls();$('download').setAttribute('aria-disabled',String(installing));}

try{const [a,b,c]=await Promise.all([fetch('assets/template.fwsc',{cache:'no-store'}),fetch('assets/manifest.json',{cache:'no-store'}),fetch('syllables.json',{cache:'no-store'})]);if(!a.ok||!b.ok||!c.ok)throw Error('Build assets are unavailable');template=await a.arrayBuffer();manifest=await b.json();labels=await c.json();status('Choose your NSX39 dump to begin.');controls();}catch(e){status(`Cannot load firmware build: ${e.message}`);}
$('dump').onchange=()=>{panic();result=null;$('result').hidden=true;$('preview').hidden=true;$('listen').textContent='Start audio preview';worker?.terminate();if(downloadURL)URL.revokeObjectURL(downloadURL);status('Ready to validate your dump.');controls();};
$('generate').onclick=async()=>{
 if(generating||installing||busLocked)return;generating=true;panic();worker?.terminate();$('generate').disabled=true;result=null;$('result').hidden=true;controls();$('generate').disabled=true;
 try{const f=$('dump').files[0];if(!f||![0x200000,0x200110].includes(f.size))throw Error('Expected a 2 MiB NSX39 SPI dump, with or without the 272-byte trailer.');const dump=await f.arrayBuffer();const copy=template.slice(0);worker=new Worker('worker.js',{type:'module'});worker.onerror=()=>{generating=false;status('Generation worker failed. Reload and try again.');controls();};worker.onmessage=({data})=>{
  if(data.message)status(data.message);if(data.error){generating=false;status(data.error);worker.terminate();controls();return;}if(!data.result)return;
  generating=false;result=data.result;configuration=structuredClone(result.configuration);try{const raw=localStorage.getItem(PREVIEW_KEY);const values=raw?JSON.parse(raw):loadMemory(localStorage)?.values;if(values){encodeMemory({...configuration,values});configuration.values=values;}}catch(e){status('Could not restore preview preferences: '+e.message);}worker.terminate();if(downloadURL)URL.revokeObjectURL(downloadURL);downloadURL=URL.createObjectURL(new Blob([result.bytes],{type:'application/octet-stream'}));$('download').href=downloadURL;
  $('stats').textContent=`Ready: 128 syllables, 16 lyric memories, 449 vocal units. Voice bank ${(result.bankBytes/1024).toFixed(1)} KiB. Package ${(result.bytes.length/1024).toFixed(1)} KiB.`;
  $('details').textContent=`Build ${manifest.version} · ${result.product}\nSHA-256 ${result.sha256}\nNative vocals; experimental timing and expression\nTransition codec: IMA ADPCM, 128-sample independent blocks\nAll-source codec SNR: ${result.snrDB.toFixed(2)} dB; sustained cycles exact\nPhysical hardware validation: pending`;
  $('result').hidden=false;$('syllable').replaceChildren(...result.pronunciations.map((p,i)=>{const o=document.createElement('option');o.value=i;o.textContent=`${i.toString().padStart(3,'0')} — ${label(i)}`;return o;}));selected=configuration.values[15];lyric=configuration.values[14];
  $('lyric').replaceChildren(...Array.from({length:18},(_,i)=>{const o=document.createElement('option');o.value=i;o.textContent=i>=2?'Lyrics '+String(i-2).padStart(2,'0'):i===1?'DoReMi':'Selectable syllable';return o;}));lyricControls();syncPreviewControls();updateDownload();
  status('Firmware generated. Preview, download, or connect your FM-1 to install.');controls();
 };worker.postMessage({dump,template:copy,manifest},[dump,copy]);
 }catch(e){generating=false;status(e.message);controls();}
};
function draw(wave,start){const c=$('scope'),x=c.getContext('2d');x.fillStyle='#000';x.fillRect(0,0,c.width,c.height);x.strokeStyle='#294334';x.beginPath();x.moveTo(0,90);x.lineTo(720,90);x.stroke();const low=Math.min(...wave),high=Math.max(...wave),span=high-low;x.strokeStyle=getComputedStyle(document.documentElement).getPropertyValue('--accent').trim();x.beginPath();for(let i=0;i<240;i++){const y=span>=0.002?168-(wave[(start+i)%240]-low)*156/span:90;i?x.lineTo(i*3,y):x.moveTo(0,y);}x.stroke();}
function drawEnvelope(a){
 const c=$('envelope'),x=c.getContext('2d'),total=a[0]+a[1]+500+a[3];const ax=24+a[0]*620/total,dx=ax+12+a[1]*620/total,sx=dx+12+500*620/total,sy=140-a[2];
 const points=[[12,140],[ax,40],[dx,sy],[sx,sy],[700,140]],stage=a[4];x.fillStyle='#000';x.fillRect(0,0,720,160);x.lineWidth=2;
 for(let i=1;i<5;i++){x.strokeStyle=i===stage?'#39c5bb':'#566b5c';x.beginPath();x.moveTo(...points[i-1]);x.lineTo(...points[i]);x.stroke();}
 if(stage){const begin=points[stage-1][0],end=points[stage][0],px=stage===3?(begin+end)/2:begin+(end-begin)*a[6]/32768;x.fillStyle='#e8edea';x.beginPath();x.arc(px,140-a[5]*100/32768,4,0,Math.PI*2);x.fill();}
 const types=['extreme','fast','normal','slight'],joins=['50 ms','minimum','velocity'];
 $('articulation-state').textContent=`Envelope: ${['idle','attack','decay','sustain','release'][stage]} · Vibrato: ${a[9]?types[a[7]]:'off'}, depth ${a[9]}, rate ${a[8]}, delay ${a[10]} ms · Auto pitch ${a[11]?'on':'off'} · Dynamics ${a[12]?'on':'off'} · Glide ${a[13]} · Noise ${Math.round((a[14]-64)*24/(a[14]<=64?64:63))} dB · Join: ${joins[a[15]]} · Hold ${a[16]?'on':'off'}`;
}
for(let i=0;i<4;i++)$('adsr-'+i).oninput=()=>{const value=Number($('adsr-'+i).value);$('adsr-value-'+i).textContent=value+(i===2?'%':' ms');if(configuration){configuration.values[i]=value;remember();}node?.port.postMessage({adsr:[i,value]});};
for(const [id,label]of [[2,'FX: noise'],[3,'SCL: auto pitch'],[4,'ENV: dynamics'],[5,'LFO: vibrato'],[6,'EDIT: vibrato type'],[7,'GLO: vibrato rate'],[8,'HOME: vibrato delay'],[10,'ARP: connection'],[11,'SEQ: glide'],[12,'PLAY: hold'],[13,'REC: vibrato depth']]){const b=document.createElement('button');b.type='button';b.textContent=label;b.onclick=()=>node?.port.postMessage({button:id});$('articulation-buttons').append(b);}
$('listen').onclick=async()=>{
 try{panic();if(audio)await audio.close();audio=new AudioContext();await audio.audioWorklet.addModule('voice-worklet.js');node=new AudioWorkletNode(audio,'nsx-voice',{numberOfInputs:0,numberOfOutputs:1,outputChannelCount:[2]});node.connect(audio.destination);node.port.onmessage=({data})=>{if(data.ready)previewMemory();if(data.memory){configuration=decodeMemory(data.memory);remember();syncPreviewControls();}if(data.state){lastPlayed=data.last;upcoming=data.upcoming||[data.next];syllableDisplay();}if(data.wave)draw(data.wave,data.scopeIndex%240);if(data.articulation)drawEnvelope(data.articulation);if(data.error){status(`Preview failed: ${data.error}`);panic();}};const r=await fetch('assets/voice-engine.wasm',{cache:'no-store'});if(!r.ok)throw Error('Preview engine unavailable');node.port.postMessage({init:true,wasm:await r.arrayBuffer(),bank:result.bank});await audio.resume();$('preview').hidden=false;$('listen').textContent='Restart audio preview';
 }catch(e){status(`Cannot start preview: ${e.message}`);}
};
$('lyric').onchange=()=>{lyric=Number($('lyric').value);configuration.values[14]=lyric;remember();lyricControls();node?.port.postMessage({lyric});};
$('rewind').onclick=()=>node?.port.postMessage({rewind:true});
$('syllable').onchange=()=>{selected=Number($('syllable').value);configuration.values[15]=selected;remember();node?.port.postMessage({select:selected});};
const names=['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];
for(let i=0;i<27;i++){const b=document.createElement('button'),n=65+i;b.type='button';b.dataset.offset=i;b.textContent=names[n%12];b.className=names[n%12].includes('#')?'black':'';b.setAttribute('aria-label',`Play ${names[n%12]}, keyboard position ${i+1}`);b.onpointerdown=e=>{e.preventDefault();b.setPointerCapture(e.pointerId);noteOn(e.pointerId,i);};b.onpointerup=b.onpointercancel=e=>noteOff(e.pointerId);$('keys').append(b);}
const mapping="awsedftgyhujkolp;'";
window.addEventListener('keydown',e=>{if(!active()||e.repeat||e.ctrlKey||e.altKey||e.metaKey||['INPUT','SELECT','TEXTAREA','BUTTON'].includes(e.target.tagName))return;const i=mapping.indexOf(e.key.toLowerCase());if(i>=0){e.preventDefault();noteOn(e.code,i);}});
window.addEventListener('keyup',e=>noteOff(e.code));window.addEventListener('blur',panic);document.addEventListener('visibilitychange',()=>{if(document.hidden)panic();});
$('oct-down').onclick=()=>{panic();octave=Math.max(0,octave-1);configuration.values[17]=octave;remember();previewMemory();};$('oct-up').onclick=()=>{panic();octave=Math.min(2,octave+1);configuration.values[17]=octave;remember();previewMemory();};$('panic').onclick=()=>panic(true);$('ack').onchange=controls;
window.addEventListener('beforeunload',e=>{if(installing){e.preventDefault();e.returnValue='Firmware installation is in progress.';}});
$('install').onclick=async()=>{
 if(!result||busLocked||installing||!$('ack').checked||!navigator.requestMIDIAccess)return;installing=true;document.dispatchEvent(new CustomEvent('fm1-midi-busy',{detail:{owner:'firmware',busy:true}}));panic();controls();let wake,view,timer;
 const render=()=>{if(view)renderInstallProgress(document,view.snapshot());};
 const progress=(phase,info,detail)=>{view.event(phase,info,detail);render();};
 try{view=new InstallProgress(result.bytes);render();timer=setInterval(render,500);wake=navigator.wakeLock?await navigator.wakeLock.request('screen').catch(()=>null):null;const access=await navigator.requestMIDIAccess({sysex:true});const updater=new Updater(access);if(!await updater.resume(result.image,progress,result.product))await updater.install(result.image,result.product,progress);}
 catch(e){const message=`${e.message} Keep the generated package and follow the recovery instructions.`;if(view){view.stop(message);render();}else $('install-status').textContent=`Installation stopped: ${message}`;}
 finally{clearInterval(timer);render();await wake?.release();installing=false;document.dispatchEvent(new CustomEvent('fm1-midi-busy',{detail:{owner:'firmware',busy:false}}));controls();}
};
$('preview-script').onchange=()=>{configuration.values[16]=Number($('preview-script').value);remember();syllableDisplay();previewMemory();};
$('download').onclick=e=>{if(installing)e.preventDefault();};
document.addEventListener('fm1-tab-change',e=>{if(e.detail!=='firmware')panic();});
document.addEventListener('fm1-midi-busy',e=>{if(e.detail.owner!=='firmware'){busLocked=e.detail.busy;controls();}});
