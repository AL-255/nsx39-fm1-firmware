// SPDX-License-Identifier: GPL-3.0-only
import {parseLyrics,lyricTable,loadMemory} from './lib/memory.js';
import {withDevice} from './lib/device-session.js';
const $=id=>document.getElementById(id),KEY='nsx39.fm1.lyric-editor.v1';
const response=await fetch('syllables.json',{cache:'no-store'});if(!response.ok)throw Error('Syllable labels unavailable');const labels=await response.json();
let memories=null,drafts={},slot=0,busy=false;
const text=ids=>ids.map(id=>id>=124?'#'+id:labels[id][0]).join(' ');
const status=s=>{$('memory-status').textContent=s;};
function persist(){try{localStorage.setItem(KEY,JSON.stringify({version:1,memories,drafts,slot}));}catch(e){status('Browser draft storage unavailable: '+e.message);}}
function controls(){
 let valid=false;try{const ids=parseLyrics($('memory-text').value,labels);$('memory-preview').textContent=`${ids.length} / 63 syllables · ${ids.map(id=>labels[id][1]).join('')}`;valid=true;}catch(e){$('memory-preview').textContent=memories?e.message:'';}
 $('memory-keyboard').disabled=$('memory-slot').disabled=$('memory-text').disabled=busy||!memories;
 $('memory-read').disabled=busy||!navigator.requestMIDIAccess;$('memory-send').disabled=busy||!memories||!valid||!navigator.requestMIDIAccess;$('memory-reload').disabled=busy||!memories||!navigator.requestMIDIAccess;
}
function render(){if(memories)$('memory-text').value=drafts[slot]??text(memories[slot]);controls();}
function insertSyllable(id){
 if(busy||!memories)return;
 const editor=$('memory-text'),start=editor.selectionStart,end=editor.selectionEnd;
 const token=(start&&!/\s/.test(editor.value[start-1])?' ':'')+(id>=124?'#'+id:labels[id][0])+(end<editor.value.length&&!/\s/.test(editor.value[end])?' ':'');
 const candidate=editor.value.slice(0,start)+token+editor.value.slice(end);
 try{parseLyrics(candidate,labels);}catch(e){if(e.message==='Use 1–63 syllables per memory'){status('Memory full: use up to 63 syllables. Select text to replace it.');return;}}
 editor.setRangeText(token,start,end,'end');editor.focus({preventScroll:true});editor.dispatchEvent(new Event('input',{bubbles:true}));
}
labels.forEach(([roman,kana],id)=>{
 const key=document.createElement('button');key.type='button';key.dataset.syllable=id;key.title=`Syllable #${id}`;key.setAttribute('aria-label',`Insert ${kana}, ${roman}, syllable ${id}`);
 for(const [value,className]of [[kana,'syllable-kana'],[roman,'syllable-romaji']]){const span=document.createElement('span');span.className=className;span.textContent=value;key.append(span);}
 key.onclick=()=>insertSyllable(id);$('syllable-keys').append(key);
});
for(let i=0;i<16;i++){const o=document.createElement('option');o.value=i;o.textContent='Lyrics '+String(i).padStart(2,'0');$('memory-slot').append(o);}
try{
 const raw=localStorage.getItem(KEY);
 if(raw){const c=JSON.parse(raw);if(c.version!==1)throw Error('Unsupported draft version');lyricTable(c.memories);memories=c.memories;slot=Number.isInteger(c.slot)&&c.slot>=0&&c.slot<=15?c.slot:0;for(const[k,v]of Object.entries(c.drafts||{}))if(Number(k)>=0&&Number(k)<=15&&typeof v==='string')drafts[k]=v;}
 else{const legacy=loadMemory(localStorage);if(legacy)memories=legacy.lyrics;}
 if(memories)status('Browser drafts restored. Read from the FM-1 to refresh, or save a memory to the device.');
}catch(e){memories=null;status('Could not restore browser drafts: '+e.message);}
$('memory-slot').value=slot;render();
$('memory-slot').onchange=()=>{slot=Number($('memory-slot').value);persist();render();};
$('memory-text').oninput=()=>{drafts[slot]=$('memory-text').value;status('Draft kept in this browser. Save to send it to the FM-1.');persist();controls();};
async function run(action){
 if(busy)return;let ids;if(action==='save'){try{ids=parseLyrics($('memory-text').value,labels);}catch(e){status(e.message);return;}}
 busy=true;controls();const target=slot;status('Connecting to FM-1…');
 try{await withDevice('memories',async device=>{
  $('memory-device').textContent=device.device.id.text+' · '+device.device.name;
  const state=action==='save'?await device.update({lyric:{slot:target,ids}}):await device.read();memories=state.lyrics;
  if(action==='save'||action==='reload')delete drafts[target];
  status(action==='save'?`Memory ${target} saved and verified on the FM-1. It will survive power-off.`:action==='reload'?`Memory ${target} reloaded from the FM-1.`:'Device memories read. Existing browser drafts are retained.');persist();
 });}catch(e){status(e.message);}finally{busy=false;render();}
}
$('memory-read').onclick=()=>run('read');$('memory-send').onclick=()=>run('save');$('memory-reload').onclick=()=>run('reload');
