// SPDX-License-Identifier: GPL-3.0-only
import {DEFAULT_VALUES,VALUE_LIMITS} from './lib/memory.js';
import {withDevice} from './lib/device-session.js';
const $=id=>document.getElementById(id),KEY='nsx39.fm1.device-settings.v2';
const response=await fetch('syllables.json',{cache:'no-store'});if(!response.ok)throw Error('Syllable labels unavailable');const labels=await response.json();
const choices={4:['Extreme','Fast','Normal','Slight'],12:['50 ms','Minimum','Velocity'],14:['Selectable syllable','DoReMi',...Array.from({length:16},(_,i)=>'Lyrics '+String(i).padStart(2,'0'))],15:labels.map(([r,h],i)=>`${i} · ${h} / ${r}`),16:['Hiragana','Katakana','Romaji'],17:['OCT0','OCT1','OCT2']};
const fields=[[17,'Octave'],[16,'Labels'],[14,'Mode'],[15,'Manual syllable'],[0,'Attack (ms)'],[1,'Decay (ms)'],[2,'Sustain (%)'],[3,'Release (ms)'],[8,'Automatic pitch'],[9,'Automatic dynamics'],[4,'Vibrato type'],[5,'Vibrato rate'],[6,'Vibrato depth'],[7,'Vibrato delay (ms)'],[10,'Glide'],[11,'Recorded noise level'],[12,'Connection'],[13,'Hold']];
let backup=null,backupURL,downloaded=false;
let base=null,edits={},busy=false;const inputs=new Map();const status=s=>{$('settings-status').textContent=s;};
const valid=(i,v)=>Number.isInteger(i)&&i>=0&&i<18&&Number.isInteger(v)&&v>=0&&v<=VALUE_LIMITS[i];
function persist(){try{localStorage.setItem(KEY,JSON.stringify({version:1,base,edits}));}catch(e){status('Browser draft storage unavailable: '+e.message);}}
function controls(){if(typeof storageControls==='function')storageControls();$('settings-fields').disabled=busy||!base;$('settings-read').disabled=busy||!navigator.requestMIDIAccess;$('settings-save').disabled=busy||!base||!Object.keys(edits).length||!$('settings-form').checkValidity()||!navigator.requestMIDIAccess;$('settings-defaults').disabled=busy||!base;}
function render(){if(base)for(const[i,e]of inputs){e.setCustomValidity('');const v=edits[i]??base[i];if(e.type==='checkbox')e.checked=!!v;else e.value=v;if(i===11)$('setting-noise-value').textContent=((v-64)*24/(v<=64?64:63)).toFixed(1)+' dB';}controls();}
for(const[i,label]of fields){
 const l=document.createElement('label');l.textContent=label;const e=document.createElement(choices[i]?'select':'input');e.id='setting-'+i;
 if(choices[i])choices[i].forEach((text,value)=>{const o=document.createElement('option');o.value=value;o.textContent=text;e.append(o);});
 else{e.type=[8,9,13].includes(i)?'checkbox':i===11?'range':'number';if(e.type!=='checkbox'){e.min=0;e.max=VALUE_LIMITS[i];e.step=1;}}
 e.oninput=()=>{const value=e.type==='checkbox'?Number(e.checked):Number(e.value);if((e.type==='number'&&e.value==='')||!valid(i,value)){e.setCustomValidity('Use a value from 0 to '+VALUE_LIMITS[i]);controls();return;}e.setCustomValidity('');if(value===base[i])delete edits[i];else edits[i]=value;status(Object.keys(edits).length+' unsaved settings. Save to apply them to the FM-1.');persist();if(i===11)$('setting-noise-value').textContent=((value-64)*24/(value<=64?64:63)).toFixed(1)+' dB';controls();};
 inputs.set(i,e);l.append(e);if(i===11){const o=document.createElement('output');o.id='setting-noise-value';l.append(o);}$('settings-fields').append(l);
}
try{const raw=localStorage.getItem(KEY);if(raw){const c=JSON.parse(raw);if(c.version!==1||!Array.isArray(c.base)||c.base.length!==18||c.base.some((v,i)=>!valid(i,v)))throw Error('Invalid settings draft');base=c.base;for(const[k,v]of Object.entries(c.edits||{}))if(valid(Number(k),v))edits[k]=v;status('Browser drafts restored. Read current settings or save your changes.');}}catch(e){status('Could not restore settings draft: '+e.message);}
render();$('settings-form').onsubmit=e=>e.preventDefault();
$('settings-defaults').onclick=()=>{for(let i=0;i<18;i++)if(base[i]!==DEFAULT_VALUES[i])edits[i]=DEFAULT_VALUES[i];else delete edits[i];status('Default values staged. Save settings to apply them.');persist();render();};
async function run(write){
 if(busy)return;if(write&&!$('settings-form').reportValidity())return;const requested={...edits};busy=true;controls();status('Connecting to FM-1…');
 try{await withDevice('settings',async device=>{const state=write?await device.update({values:requested}):await device.read();base=state.values;edits={};$('settings-device').textContent=device.device.id.text+' · '+device.device.name;status(write?'Settings saved and verified on the FM-1. Lyric memories are preserved.':'Current device settings read.');persist();});}
 catch(e){status(e.message);}finally{busy=false;render();}
}
$('settings-read').onclick=()=>run(false);$('settings-save').onclick=()=>run(true);

// Explicit, backed-up initialization; never invoked by an ordinary save failure.
const storageStatus=s=>{$('storage-status').textContent=s;};
function storageControls(){$('storage-backup').disabled=busy||!navigator.requestMIDIAccess;$('storage-ack').disabled=busy||!backup||!downloaded;$('storage-initialize').disabled=busy||!backup||!downloaded||!$('storage-ack').checked;}
$('storage-download').onclick=()=>{downloaded=true;storageControls();};$('storage-ack').onchange=storageControls;
async function storageRun(initialize){
 if(busy||initialize&&(!backup||!downloaded||!$('storage-ack').checked))return;
 busy=true;render();storageControls();storageStatus('Connecting to FM-1…');
 try{await withDevice('settings',async device=>{
  if(initialize){await device.initializeStorage(backup);backup=null;$('storage-download').hidden=true;$('storage-ack').checked=false;storageStatus('Storage initialized and current state verified. Return to Lyric memories and save your draft.');}
  else{backup=null;downloaded=false;$('storage-ack').checked=false;$('storage-download').hidden=true;backup=await device.backupStorage();if(backupURL)URL.revokeObjectURL(backupURL);backupURL=URL.createObjectURL(new Blob([JSON.stringify(backup,null,2)],{type:'application/json'}));$('storage-download').href=backupURL;$('storage-download').hidden=false;storageStatus('Backup ready. Download it, then check the confirmation to enable initialization.');}
 });}catch(e){storageStatus(e.message);}finally{busy=false;render();storageControls();}
}
$('storage-backup').onclick=()=>storageRun(false);$('storage-initialize').onclick=()=>storageRun(true);storageControls();
