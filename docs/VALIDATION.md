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
Hardware installation stays blocked while physical recovery and validation are
pending. `tools/read_uboot_diagnostics.py` performs only INQUIRY and two fixed
SRAM reads, and may need sudo for `/dev/sg0`; it has no write/erase/execute commands.
