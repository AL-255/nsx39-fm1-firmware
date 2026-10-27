// SPDX-License-Identifier: GPL-3.0-only
import test from'node:test';import{execFileSync}from'node:child_process';
test('autosave uses a silent DMA ring and attempts only once per settled change, including failures',()=>{
 execFileSync('cc',['-O1','-g','-fsanitize=address,undefined','tests/settings_save.c','-o','build/settings_save_test'],{stdio:'inherit'});
 execFileSync('build/settings_save_test',[],{stdio:'inherit'});
});
