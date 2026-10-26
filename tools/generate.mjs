// SPDX-License-Identifier: GPL-3.0-only
// Private, offline CLI equivalent of the browser generator. Output is ignored.
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {extractDump,packBank} from '../web/lib/bank.js';
import {Package} from '../web/lib/fm1pkg.js';
const [input,output='local/voice.fwsc']=process.argv.slice(2);
if(!input)throw Error('Usage: node tools/generate.mjs /path/to/NSX39-dump.BIN [local/output.fwsc]');
const db=await extractDump(await readFile(input));const bank=packBank(db);
const manifest=JSON.parse(await readFile('build/manifest.json','utf8'));
const pkg=new Package(await readFile('build/template.fwsc'));const app=pkg.dec.slice(pkg.appOff,pkg.appOff+pkg.app.size);app.set(bank.bytes,manifest.bankOffset);pkg.setApp(app);
await mkdir('local',{recursive:true});await writeFile(output,pkg.serialize());await writeFile('local/voice.bank',bank.bytes);await writeFile('local/generation.json',JSON.stringify({used:bank.used,samples:bank.samples,snrDB:bank.snrDB,names:bank.names,units:db.units.map(u=>({source:u.source,target:u.target,kind:u.kind,samples:u.pcm.length})),pronunciations:db.pronunciations},null,2));
console.log(JSON.stringify({output,bankBytes:bank.used,samples:bank.samples,snrDB:bank.snrDB,units:db.units.length,pronunciations:db.pronunciations.length}));
