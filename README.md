# NSX39 vocal debug firmware for M-VAVE FM-1

A GPL-3.0-only native vocal firmware and **browser-local firmware generator**.
The first build is a debug instrument: **SELECT chooses a syllable;
the 27 keys trigger it at different pitches; the display shows what is sounding
and its waveform.**

Pages generator: <https://al-255.github.io/nsx39-fm1-firmware/>.

**Hardware installation disabled:** the first physical test of build `FM-1_939`
returned to UBOOT with a black screen. A retained-SRAM startup bug was reproduced
and corrected in candidate `FM-1_940`; physical recovery/validation is pending.
This is a new recorded-unit/PSOLA-style vocal implementation, not a bit-exact
port of Yamaha's frontend. Full NSX39 expression, timing, command-slot and
persistence compatibility remains unfinished. Do not mistake bank coverage for
complete behavioral equivalence. See [compatibility](docs/COMPATIBILITY.md).

## Use

1. Open the generator and select your own NSX39 SPI dump. Nothing is uploaded.
2. Generate, then listen using the same native engine compiled to WebAssembly.
3. Download the generated `.fwsc` to open in the
   [FM-1 emulator](https://joakimeriksson.github.io/fm1-emu/) or test on hardware.
4. For hardware, review the recovery instructions, explicitly acknowledge the
   experimental build, and use **Connect and install** in a SysEx Web MIDI browser
   (Chrome/Edge). No physical device is flashed automatically by this repository.

Supported input is the analyzed 2,097,152-byte flash image, optionally followed
by the 272-byte programmer trailer. Flash SHA-256:
`4e9b8eb35198f2b809fa49f9c704a8982ec6b5fa6cd610c4bde807d8fb02a9e4`.
Unknown or modified flash contents are rejected, including user-edited persistent
slots. Supporting other revisions requires an explicit validated profile.

On the FM-1:

| Control | Debug behavior |
|---|---|
| SELECT encoder | Select pronunciation 0–127, wrapping |
| 27 keys | Monophonic trigger and pitch, repeat the selected syllable |
| OCT− / OCT+ | Three registers: MIDI 53–79, 65–91 (default), 77–103 |
| MASTER | Output level |
| SAVE | Panic / all sound off (does not save to flash) |
| Both OCT buttons, held 5 s | Enter UBOOT for recovery |
| USB MIDI and TRS MIDI | Vocal channel 1 note/velocity/CC/bend |

The screen distinguishes **SELECTED** (the encoder's next choice) from **SOUNDED**
(the last triggered syllable). It also shows MIDI note/velocity, source unit and
phonemes, onset/loop state, a 10.9 ms output-waveform window, render microseconds,
maximum render time, and late-buffer count. It retains the last waveform after
release. Other panel controls are intentionally unused in this debug build.

## No proprietary data in this repository or its Pages deployment

The generator validates the dump, XOR-decodes its NSXP body, parses the serialized
vocal database and pronunciation packets, decodes PCM/Yamaha ADPCM, then creates
an FM-1 voice bank. All of that runs in a browser worker. No original executable
code is copied into the result: the synthesis engine is this project's C code.

The bank contains all **449 units / 744,945 decoded samples**, paired source pitch
and stretch streams, four modulation curves, and all **128 pronunciation strings**.
The 36 tiny sustained cycles stay **lossless PCM16**; 413 transitions use standard
IMA ADPCM in independently seeded **128-sample blocks**, 68 bytes each. This is
lossy for transitions. The tested bank occupies **482,028 bytes** of a 491,520-byte
reserve. Aggregate codec SNR against exact decoded input is **24.61 dB**; that
measures the codec, not fidelity to the complete original synthesizer.

The native renderer decodes a transition when it is selected into one of two
bounded 8,192-sample buffers. No heap or unbounded decoding runs in the audio path.
The firmware uses about **44 KB normal RAM**, **118 KB buffer pool**, and a
**515 KB app image**, inside the fixed 581,564-byte app slot. Audio renders at
22,050 Hz and is interpolated to the FM-1's approximately 44.1 kHz output.

Published artifacts contain only our compiled code, an **empty voice reserve**,
openly licensed font data and three **Apache-2.0 JieLi SDK** packaging components.
These SDK files are not NSX39 or M-VAVE firmware. The site also publishes a
corresponding-source archive for the GPL app and browser engine. Their exact inputs and licenses
are recorded in [vendor/README.md](vendor/README.md).
Generated user packages retain the original rights in the user-supplied voice
material; GPL licensing of this code does not relicense Yamaha/Gakken data.

## Build locally

Linux x86-64, Python 3 with Pillow, Node.js 20+, a C compiler, Clang/wasm-ld and
DejaVu Sans Mono are required. The hardware compiler is JieLi's pi32v2 compiler.
No system-wide installation is required for it. Download instructions are in
[BUILDING.md](docs/BUILDING.md).

```sh
python3 tools/build.py              # blank app + open-source loader + template
python3 tools/build_web.py          # same vocal engine, WebAssembly
npm test                           # no npm dependencies
python3 tools/prepare_pages.py      # creates an empty/new dist/ via allowlist
python3 -m http.server 8000 -d dist
```

For private local integration tests, use your own dump; all outputs remain ignored:

```sh
node tools/generate.mjs /path/to/NSX39_MX25L1635E.BIN
node tests/integration.mjs /path/to/NSX39_MX25L1635E.BIN
cc -O1 -g -fsanitize=address,undefined -Ifirmware/include -Ibuild/gen \
  tests/core.c firmware/src/voice.c -o build/core_asan
build/core_asan local/voice.bank
```

## Updates and validation

The two-stage updater and RAM-resident loader are reused from
[Felucca](https://github.com/hugelton/Felucca). The loader verifies layout, chip key,
whole-package CRC and sector consistency before replacing the application;
it preserves flash `[0,0x4000)` and verifies written sectors. The application
can write only the five OTA staging sectors. No NSX39 command can write flash.
A failed or interrupted transfer must be resumed with the **same generated
package**. Read [update safety](docs/UPDATES.md) before hardware tests.

Tests cover cross-language codec agreement, malformed input and completion gates,
all 128 pronunciations, C sanitizers, package round trips, and exact native/WASM
output agreement at MIDI 53, 65, 69, 77, 91 and 103. Upstream updater tests cover
wrong-board images, corruption, changed transfer sectors, chip-key mismatch,
interrupted staging, preservation of the boot head/user data, and repeat-install
no-op behavior. See [validation](docs/VALIDATION.md) for limits and reproduction.

GitHub Actions builds and tests without any NSX39 input, then deploys an allowlisted
static artifact to Pages. Deployment requires Pages with source **GitHub Actions**.
No dump, recording, derived bank or generated user firmware enters public CI.

## References and credits

- [NSX39-Disasm](https://github.com/AL-255/NSX39-Disasm): recovered firmware layout,
  database grammar, Yamaha source decoder, source periods, grains and controller behavior.
- [Felucca](https://github.com/hugelton/Felucca): GPL FM-1 hardware layer,
  freestanding runtime, package builder, USB-MIDI updater and recoverable loader.
- [FM-1 emulator](https://joakimeriksson.github.io/fm1-emu/) /
  [source](https://github.com/joakimeriksson/fm1-emu): runs the actual `.fwsc` package;
  primary integration test environment.
- [FM-1-RE](https://github.com/AL-255/FM-1-RE): hardware, flash layout and OTA research.
- [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter): recovery tooling.
- [JieLi AC79 SDK](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK): Apache-2.0 packaging files.

No affiliation with Yamaha, Gakken or M-VAVE.
