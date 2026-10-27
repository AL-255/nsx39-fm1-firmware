// SPDX-License-Identifier: GPL-3.0-only
import {generate} from './lib/generator.js';
self.onmessage=async({data})=>{
 try {const result=await generate(new Uint8Array(data.dump),new Uint8Array(data.template),data.manifest,message=>postMessage({message}),data.configuration);postMessage({result},[result.bytes.buffer,result.bank.buffer,result.image.buffer]);}
 catch(error){postMessage({error:error.message});}
};
