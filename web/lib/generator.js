// SPDX-License-Identifier: GPL-3.0-only
import {extractDump,packBank,sha256,CAPACITY} from './bank.js';
import {encodeMemory,lyricArrays,DEFAULT_VALUES} from './memory.js';
import {Package,logicalImage} from './fm1pkg.js';
export async function generate(dump,templateBytes,manifest,report=()=>{}) {
 if(manifest.format!==1||manifest.product!=='FM-1_947'||manifest.bankCapacity!==CAPACITY)throw Error('Unsupported firmware build manifest');
 if(await sha256(templateBytes)!==manifest.templateSha256)throw Error('Firmware template hash mismatch. Reload the page.');
 report('Validating the NSX39 dump…');const database=await extractDump(dump);
 const config={lyrics:lyricArrays(database.factoryLyrics),values:[...DEFAULT_VALUES]};
 report('Compressing all 449 vocal units…');const bank=packBank({...database,seed:encodeMemory(config)});
 const pkg=new Package(templateBytes);
 if(pkg.product!==manifest.product||pkg.app.size!==0x8dfbc||pkg.flashSize!==0x93000||pkg.appOff!==0x4120)throw Error('Unexpected FM-1 flash layout');
 if(!Number.isInteger(manifest.bankOffset)||manifest.bankOffset<4||manifest.bankOffset+CAPACITY>manifest.appBytes||manifest.appBytes>pkg.app.size)throw Error('Invalid voice slot');
 const app=pkg.dec.slice(pkg.appOff,pkg.appOff+pkg.app.size);
 if(app.subarray(manifest.bankOffset,manifest.bankOffset+CAPACITY).some(b=>b))throw Error('The public template must contain no voice data');
 app.set(bank.bytes,manifest.bankOffset);pkg.setApp(app);const bytes=pkg.serialize();
 const checked=new Package(bytes);if(checked.product!==manifest.product||!checked.dec.subarray(checked.appOff+manifest.bankOffset,checked.appOff+manifest.bankOffset+CAPACITY).every((x,i)=>x===bank.bytes[i]))throw Error('Generated package failed readback');
 return {bytes,bank:bank.bytes,image:logicalImage(bytes),product:manifest.product,sha256:await sha256(bytes),bankBytes:bank.used,snrDB:bank.snrDB,samples:bank.samples,pronunciations:database.pronunciations,factoryLyrics:database.factoryLyrics,configuration:config};
}
