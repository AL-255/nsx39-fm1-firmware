// SPDX-License-Identifier: GPL-3.0-only
// Each tool initializes independently; a device-only visit loads no firmware assets.
const names=['firmware','memories','settings'],modules={firmware:'./app.js',memories:'./memory-editor.js',settings:'./settings-editor.js'};
const loaded=new Map();let selected,busy;
function controls(){for(const name of names)document.getElementById('tab-'+name).disabled=!!busy&&busy!==name;}
async function select(name){
 if(!names.includes(name))name='firmware';if(busy&&busy!==name)return;
 selected=name;history.replaceState(null,'','#'+name);
 for(const n of names){const tab=document.getElementById('tab-'+n);tab.setAttribute('aria-selected',String(n===name));tab.tabIndex=n===name?0:-1;document.getElementById(n+'-panel').hidden=n!==name;}
 document.dispatchEvent(new CustomEvent('fm1-tab-change',{detail:name}));controls();
 if(!loaded.has(name)){const promise=import(modules[name]);loaded.set(name,promise);try{await promise;}catch(e){loaded.delete(name);const p=document.createElement('p');p.role='status';p.textContent='Could not load this tool: '+e.message;document.getElementById(name+'-panel').append(p);}}
}
for(const name of names){const tab=document.getElementById('tab-'+name);tab.onclick=()=>select(name);tab.onkeydown=e=>{
 const i=names.indexOf(name);let next;if(e.key==='ArrowRight')next=(i+1)%3;if(e.key==='ArrowLeft')next=(i+2)%3;if(e.key==='Home')next=0;if(e.key==='End')next=2;
 if(next!==undefined&&!document.getElementById('tab-'+names[next]).disabled){e.preventDefault();document.getElementById('tab-'+names[next]).focus();select(names[next]);}
};}
document.addEventListener('fm1-midi-busy',e=>{busy=e.detail.busy?e.detail.owner:null;controls();});
window.addEventListener('beforeunload',e=>{if(busy){e.preventDefault();e.returnValue='A device operation is in progress.';}});
window.addEventListener('hashchange',()=>select(location.hash.slice(1)));
select(location.hash.slice(1));
