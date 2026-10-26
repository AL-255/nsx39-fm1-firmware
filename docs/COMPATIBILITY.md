# Compatibility boundary

The user-selected first target is vocals and controller functions. GM tones and
original hardware effects require absent internal NSX-1 ROM and are out of scope.

This first delivery is a **debug build**, not a completed NSX39 replica.

| Capability | Current implementation |
|---|---|
| Complete vocal bank | All 449 units extracted; 36 cycles lossless, 413 transitions lossy ADPCM |
| Complete pronunciation table | All 128 original strings extracted from six packets |
| Keyboard and pitch | 27 FM-1 keys, three original MIDI ranges, monophonic |
| Syllable selector | Lyric 0: SELECT chooses a fixed syllable; repeated keys repeat it |
| Factory lyrics | ALGORITHM selects banks 1–15 from the dump; keys step, loop and pitch; SELECT left rewinds, right reserved |
| Readable display | PRESETS selects hiragana/katakana/romaji; large last played and smaller upcoming label |
| Vocal DSP | New integer pitch-synchronous grain renderer; finite transitions and three steady variants |
| Source pitch metadata | Used to select pitch-mark neighborhoods |
| Stretch streams / modulation templates | Extracted and stored; original algorithms not yet implemented |
| Note velocity, expression, volume | Implemented; original contour fidelity pending |
| Pitch bend, bend-range RPN, fine tuning | Implemented; original integer log/exponent rounding not matched |
| Sustain, note-off, panic | Implemented |
| NRPN controls | Basic approximate vibrato, automatic pitch/dynamics, timing, noise, connection and seek dispatch; not faithful original policies |
| Lyric SysEx | Parses phonetic replace/append messages; panel lyric banks index the original 128-entry pronunciation table; custom SysEx sequences do not have kana/romaji annotations |
| Solfege | Experimental API only; no debug-panel exposure or compatibility claim |
| Generalplus command sequencer | Not implemented |
| 128 persistent command/lyric slots | Not implemented; no musical flash writes |
| Host command queries/responses | Not implemented |
| USB/TRS note input | Implemented, channel 1; TRS ignores SysEx |
| NSX39 firmware-update commands | Ignored intentionally; never translated to FM-1 writes |
| GM tones / exact effects | Not available from this SPI dump; omitted |

The original engine's filtered pitch tracker, phase correction, duration-overlap
state machine, exact randomized three-cycle blending, expression contours,
phase-aligned joins and integer control math remain future porting work. Original
SH executable code is neither shipped nor run by this firmware. Native/WASM
agreement proves these two builds agree with each other, not that they agree with
Yamaha hardware. No original-device audio capture has been used as a fidelity oracle.
