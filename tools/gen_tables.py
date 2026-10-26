#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Mathematically generated DSP tables. No firmware or audio input."""
import math
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'build/gen'
p.mkdir(parents=True,exist_ok=True)
def array(name,kind,values):
 return 'static const '+kind+' '+name+'[]={'+','.join(map(str,values))+'};\n'
(p/'dsp_tables.h').write_text(array('noise_gain_q12','uint32_t',[round(4096*10**(((i-64)*(24/64 if i<=64 else 24/63))/20)) for i in range(128)])+array('sin_q15','int16_t',[round(32767*math.sin(2*math.pi*i/256)) for i in range(256)])+array('hann_q15','int16_t',[round(32767*math.sin(math.pi*i/256)**2) for i in range(257)])+array('note_period','uint32_t',[round(22050/(440*2**((i-69)/12))*65536) for i in range(128)]))
