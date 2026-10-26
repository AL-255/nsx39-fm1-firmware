# Live articulation and ADSR (FM-1_942)

The four numbered knobs control a new amplitude ADSR: A/D 0–2,000 ms, S 0–100%,
R 0–4,000 ms. Defaults are 0 ms / 0 ms / 100% / 100 ms. The encoders use a
quadratic time scale for finer control at short times. Changing an active stage
retargets the envelope from its current level. A zero release stops immediately;
SAVE always stops immediately. Settings remain in RAM.

The largest label remains the last played syllable, with the upcoming syllable
below. The ADSR graph highlights the active segment and shows the actual envelope
level with a white cursor. Its horizontal spacing includes a representative
500 ms sustain segment; sustain lasts until key release or hold is released.
The separate waveform shows actual output, so a quiet tail can approach a flat
line even while the envelope remains in release. Button LEDs reflect live state,
including external MIDI changes.

| FM-1 control | Action |
|---|---|
| LFO | Toggle vibrato; remembers its enabled depth |
| EDIT | Cycle extreme / fast / normal / slight vibrato |
| GLO | Step vibrato rate through 16–112 |
| HOME | Step vibrato onset delay by 250 ms, returning to zero after 1,000 ms |
| REC | Step vibrato depth through 16–112 |
| SCL | Toggle automatic pitch (native approximation) |
| ENV | Toggle automatic dynamics (native approximation) |
| FX | Cycle recorded-noise gain: 0, +12, +24, −24, −12 dB |
| ARP | Cycle 50 ms / minimum / velocity-dependent connection modes |
| SEQ | Step glide amount through 0, 32, 64, 96, 127 |
| PLAY | Toggle sustain/hold; turning it off starts a pending release |
| SAVE | Panic; immediately silence and release hold |
| Knobs 1 / 2 / 3 / 4 | Attack / decay / sustain / release |
| SELECT / ALGORITHM / PRESETS | Syllable or rewind / lyric / script, unchanged |
| OCT− / OCT+ / MASTER | Pitch register / output level |

The browser preview exposes the same button actions, ADSR sliders and live graph
under **Articulation and ADSR**. Preview settings do not change a generated
package's defaults.

## What follows the original firmware

The four vibrato templates are extracted from the user's dump, never embedded in
public binaries. The new fixed-point interpolator follows the original routine
at `0409330e`: fractional interpolation, mirrored endpoints, the rate transform
around 0.5, twice the normalized modulation depth, and the 60-frame onset ramp.
Delay accepts the documented full 14-bit millisecond value. An optional private
reference test executes the original SH routine and compares its output after
original frequency conversion with the new control offset: all 4,864 checked
frames match across four types, four rate/depth combinations, delayed onsets and
live modulation toggles. This validates
the vibrato control path, not bit-identical audio from the complete synth.

Key-up now selects the available recorded vowel/nasal-to-`Sil` transition, rather
than only fading a loop. Release duration is controlled by the added ADSR; the
recorded source is stretched to that duration. If there is no recorded release
for the current phoneme, the envelope fades its current source. A held note
continues until hold is released. Returning to another already-held keyboard key
changes pitch without restarting the ADSR or advancing lyrics.

The noise control changes recorded unvoiced material, with unity at MIDI 64 and
−24/+24 dB endpoints. It does not add generated noise over every sung vowel.
Connection modes use the documented velocity-duration endpoints: 2x at velocity
0, 1x at 100 and 0.5x at 127; velocity-zero MIDI note-on is still note-off.

The panel's 27 keys send velocity 127. External channel-1 MIDI supplies other
velocities. Vibrato depth uses CC1; other original controls use NRPN MSB `70`
with LSB `01`, `02`, `03`, `04`, `07`, `08`, `0A`, `0C`. Reserved `70.05` and
`70.06` are ignored. Original ADSR CCs for GM voices are not repurposed as vocal
ADSR controls.

## Remaining differences from the original

The request for fully original articulation is **not yet completely satisfied**.
The original automatic pitch/dynamics contour generator at `0408e320` and its
note-to-note scheduler have not been fully ported. These two panel switches still
control native approximations; glide and minimum-connection scheduling also
remain approximate. Do not treat their availability on buttons as proof of
original contour fidelity. The original synth's filtered pitch tracker and
phase-aligned joins also remain different.

The user-requested ADSR is an FM-1 extension. The original vocal channel does not
expose this four-knob ADSR or selectable named tail styles. Recorded release
units are original data; stretching them to a user-selected release time is the
new envelope behavior, not a claim of exact original note-off scheduling.

References: local NSX39-Disasm analysis/GENERALPLUS.md, analysis/SYNTHESIS.md,
and analysis/generated/ghidra/decompiled/0409330e.c / 04096e72.c / 040975c0.c.
The public manufacturer documentation is Yamaha's YMW820 MIDI specification,
NRPN table, page 4. No original code or recordings are copied into this source.
