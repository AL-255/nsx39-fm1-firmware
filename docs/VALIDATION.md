# Validation record

Validated locally on 2026-10-25 using:

- Felucca `440e62ce8a33d9bb285ab6bbca9a0fd50e459648` hardware/updater.
- fm1-emu `eef6d957aa94d3dfa74551688d707b984b792922` (MIT).
- JieLi Linux toolchain `jieli-linux-toolchains-20260730.1`, clang 4.0.1 pi32v2.
- AC79 SDK `d179b4484759423312073f5fbb232501aa491047`.
- Clang 18 WebAssembly, GCC native tests with AddressSanitizer/UBSan, Node 24.

The known user-owned dump parses to 449 units, 744,945 samples, 128 pronunciations,
36 sustained cycles and 413 transitions. The compressed bank is 482,028 bytes;
all sustained PCM samples match the decoded source exactly. Aggregate codec SNR
is 24.61256 dB. The source dump and all resulting audio/banks/packages stay under
ignored `local/` (the input is outside this repository).

The JS/C IMA decoders agree exactly across all 89 initial step indices and a
pseudorandom sequence containing every nibble. All 128 pronunciations render
nonzero audio; release, sustain, panic and channel isolation are checked. Bank
header mutations fail integrity checks. Native and WASM PCM agree exactly for
**33,280 samples per note** (260 blocks × 128), at MIDI 53, 65, 69, 77,
91 and 103. These are cross-build tests, not Yamaha equivalence tests.

The real generated `.fwsc` boots in fm1-emu; panel keys and SELECT affect audio,
LCD diagnostics show the selected and sounded syllables, and the waveform is
visually inspected. An initial tested run observed maximum render **448 us** and
**0 late buffers** against a roughly 2.9 ms output-half deadline. Timing in the
emulator counts its modeled instruction costs; real flash/cache/IRQ timing can
be different. The emulator reports unmapped debug-guard MMIO registers; it does
not validate those hardware protections.

The upstream OTA test passes identity, staging bounds, commit/record CRC, wrong
board size, disconnected host and missing finish. The loader test passes full
installation/readback, preservation of the first 16 KiB, unchanged-sector skips,
changed-in-flight sector retries, corrupt image refusal, wrong-key refusal, and
preservation of unrelated user data. The browser protocol tests completion gates,
request bounds, send failure and packing.

No connected browser was available in the computer-use inventory, so a real
browser UI/audio/Web MIDI test is pending. WebAssembly is instantiated and checked
through Node, and all site assets are served from a local HTTP preview. Physical
FM-1 sound, printed-control mapping across hardware revisions, USB enumeration,
update/recovery and power-interruption testing are pending. No physical FM-1 was
flashed.

## Reproduce

```sh
npm test
python3 tools/build.py
python3 tools/build_web.py
node tests/integration.mjs /path/to/your/dump.BIN
build/core_test local/voice.bank
cc -O2 vendor/felucca/tests/ota_test.c -o build/ota_test
cc -O2 vendor/felucca/tests/ldr_test.c -o build/ldr_test
build/ota_test build/template.fwsc
build/ldr_test build/template.fwsc local/voice.fwsc
/path/to/fm1-emu --fwsc local/voice.fwsc --ms 1700 \
  --keys '900-1500:n4,750:pot:900,800:SELECT:+5' \
  --wav local/emulator.wav --snap 1200
```

Create `work/` before `--snap`; that emulator writes screenshots there. Do not use
`--quiet` when requesting WAV output in this pinned emulator version.

## Physical boot failure and dirty-SRAM regression (2026-10-26)

The first user hardware install of `FM-1_939` resulted in a black screen and USB
`4c4a:8057 / WL80UBOOT1.00`. Public hardware installation was disabled immediately
after investigation began. The application and filesystem CRCs in the generated
file passed an independent check; installed flash has not yet been read back.

The `static` `.noinit` boot marker was incorrectly optimized by pi32v2 Clang:
instead of comparing the stored 32-bit magic `0x4e535842`, the emitted code read
one byte, treated any nonzero value as initialized, and trusted the adjacent
retained attempt counter. Dirty SRAM could therefore enter UBOOT immediately.
This is a confirmed code defect; retained physical RAM is still needed to prove
it was the sole cause of this unit's failure.

The old package was then booted in the pinned emulator with initial SRAM `0xa5`:
it reached the UBOOT-entry path, reset the attempt counter, and produced zero LCD
frames and zero audio halves. The volatile-marker fix emits a real 32-bit magic
comparison. With the same dirty-RAM fixture, candidate `FM-1_940` reached the LCD
and audio output (10 frames / 189 audio halves over an 800 ms local run).

`tools/test_boot.py` makes this regression part of CI using a pinned MIT emulator
and a small, explicit initial-SRAM fixture. It checks zero, `0xa5` and `0xff`
startup RAM and confirms a separate untouched word retains each fill pattern.
The test uses only the empty public template, without proprietary voice data.
RAM-only physical recovery subsequently succeeded and the installed `FM-1_939`
answered an identity query. Installation of the fixed candidate is now enabled;
a physical cold-boot check is pending. `tools/read_uboot_diagnostics.py` performs only INQUIRY and two fixed
SRAM reads, and may need sudo for `/dev/sg0`; it has no write/erase/execute commands.


## Readable syllables and original lyrics (`FM-1_941`)

The native app and AudioWorklet share `performance.c`. Public synthetic-bank
checks cover manual retention, all selector directions, stepping and wrap,
rewind without a trigger, last/next reporting, positive-velocity channel-1 input,
zero-velocity note-on, panic and held-key revoice without a lyric advance.
Malformed offsets, table size, missing/endless return markers and unsupported
jump commands fail startup validation even when the bank CRC has been recomputed.

The private dump integration test verifies that the packed 1,024-byte table is
identical to the factory defaults at `0x1de40`, then plays all 15 banks for two
loops through the actual WebAssembly controller. No table contents are in public
fixtures. The bank is now 483,052 bytes; the app is about 556 KB of the 581,564-byte
slot, with 47,320 bytes of normal RAM and 117,888 bytes of pool buffers.

A 1,650 ms run of the actual generated `.fwsc` used these panel inputs:

```sh
/path/to/fm1-emu --fwsc local/voice.fwsc --ms 1650 \
  --keys '300:enc1:+6,400-550:n0,650:enc6:+1,750-900:n4,1000:enc6:+1,1100-1250:n7,1350:enc0:-1' \
  --snap 500,850,1200,1500 --wav local/lyrics-debug.wav
```

All four LCD captures were visually inspected: readable hiragana, katakana and
romaji; large last-played versus smaller upcoming syllables; and rewind resetting
the next position while retaining the last played label. The run produced 21 LCD
frames, 482 audio halves and zero late buffers, with a maximum observed render
time of 441 microseconds. This simulator timing is not a measured hardware limit.
The empty new template also passed all three retained-SRAM startup cases (8 LCD
frames / 140 audio halves over 550 ms each).

The controlled local candidate installer is exercised without device access by
`tests/install_candidate_test.py` and the vendored simulated-device suite. It
checks package SHA/product, identity framing/checksum, read bounds, phase-specific
completion and failed sends. Default candidate preflight does not start an update.

## Articulation and ADSR (FM-1_942)

Public synthetic tests cover ADSR times and boundaries, active sustain changes,
zero/maximum release, panic, hold-delayed release, recorded release selection,
held-key pitch changes without envelope retrigger, 14-bit delay, reserved NRPNs,
all eleven articulation button actions and all four encoder endpoints.

Private validation additionally runs:

```sh
node tools/test_articulation_reference.mjs /path/to/NSX39-Disasm
```

This requires the reference project's existing original-instruction WASM and a
locally generated genuine bank. It compares 4,864 frames against SH routine
`0409330e`, using the original cents/frequency conversions, across all four
vibrato types, four rate/depth combinations, delayed onsets and live modulation
toggles. Those control outputs match.
It does not establish complete original vocal/audio or automatic-contour fidelity.

The actual generated firmware was exercised for 1,900 ms with knob and button
inputs, held notes, sustain and release. Four LCD captures were inspected for
readability and live envelope cursor/state. The run produced 24 LCD frames,
568 audio halves and zero late buffers; maximum observed render time was 411 us.
Its script was:

```sh
/path/to/fm1-emu --fwsc local/voice.fwsc --ms 1900 \
  --keys '300:enc2:+32,350:enc3:+48,400:enc4:-48,450:enc5:+24,500:LFO,550:EDIT,700-1100:n7,900:enc6:+1,1050:PLAY,1200:enc6:+1,1300:PLAY' \
  --snap 850,1150,1500,1850 --wav local/articulation-debug.wav
```

ADSR is an added FM-1 envelope. See ARTICULATION.md for the explicit remaining
differences in automatic pitch/dynamics and scheduling. Firmware-update and
startup marker code are unchanged; the standard updater/recovery and dirty-SRAM
boot regressions are required for this new application image.
