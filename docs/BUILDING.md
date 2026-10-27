# Building and publishing

All tools are host-side except the pi32v2 app and RAM loader. The build does not
consume an NSX39 dump. Linux x86-64 is the tested hardware-compiler host.

Prerequisites: Python 3 + Pillow, Node.js 20+, GCC/CC, Git, curl, xz, Clang 18 and
wasm-ld and `/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf`. Bootstrap
downloads the pinned Noto CJK Bold font (JP face, index 0) into `.cache/`.
For a conventional Ubuntu environment, the package names are `build-essential`,
`clang`, `lld`, `fonts-dejavu-core`, and `python3-pil`; installing them may need
administrator privileges. Development used existing packages plus toolchains
isolated in `/tmp`, without sudo or APT on the host.

```sh
sh tools/bootstrap.sh
export JIELI_TOOLCHAIN="$PWD/.cache/toolchain"
export AC79_SDK="$PWD/.cache/ac79-sdk"
python3 tools/build.py
python3 tools/build_web.py
npm test
python3 tools/test_updater.py
python3 tools/prepare_pages.py /tmp/nsx39-public-site  # destination must be empty
python3 -m http.server 8000 -d /tmp/nsx39-public-site
```

`NSX39_CLANG` and `NSX39_WASM_LD` select the WebAssembly compiler/linker. There are
no Node dependencies to install. `tools/bootstrap.sh` pins the compiler archive
SHA-256, SDK commit and upstream Noto font SHA-256, and `build.py` checks all
three SDK input hashes. The cached font is byte-identical to Ubuntu 24.04's
`fonts-noto-cjk` Bold font; an installed copy remains a fallback. The
JieLi download URL can change its latest release: if the hash changes, stop and
audit the new compiler rather than accepting it silently.

Outputs: `build/voice-app.bin`, `build/voice.elf`, `build/loader/ota.bin`,
`build/template.fwsc`, `build/manifest.json`, and `build/voice-engine.wasm`.
The template's voice reserve is all zeros and cannot produce vocals until the
browser injects the locally extracted bank. The generated template is suitable
for packaging and simulator checks, not a useful stand-alone instrument.

Native firmware and browser preview share `firmware/src/voice.c`, its included
`firmware/src/articulation.c`, and
`firmware/src/performance.c`. `tools/gen_label_font.py` subsets Noto CJK to the
179 UI glyphs, storing 2-bit coverage with per-glyph RLE when smaller. No font
package download is required at runtime. `NSX39_LABEL_FONT` overrides its local
font file; it must provide the same JP face at index 0. The public bank
format is defined by `web/lib/bank.js` and validated at startup by `nsx_init`.
It is little-endian, version 2, with CRC32 covering header and payload (CRC field
zero during calculation), 449 40-byte descriptors, names, pronunciation strings,
four curves, PCM steady cycles and IMA transition blocks. Header words at offsets
48/52 hold the factory lyric offset/size (both zero when absent in synthetic tests).
The extracted 1,024 bytes at dump offset `0x1de40` contain 16 banks of 64 bytes.
Header words 56/60 optionally point to the 1,120-byte initial settings/memory
record, with its own CRC and range checks. The original table is retained even
when generated memories are edited. Runtime state and its separate flash journal
are documented in [MEMORIES.md](MEMORIES.md).
IDs below 128 are syllables; `0x80` returns to the beginning. Every bank must have
at least one syllable followed only by `0x80` padding. Other command tokens are
rejected for this supported dump revision. Mode 0 replaces source bank 0 with the
manual selector; modes 1–15 preserve the corresponding source indices. Pitch and stretch
streams remain signed Q16.16/integer metadata as recovered. Reserves/padding
are zero; the unused end of the bank slot is not included in the bank CRC.

## GitHub Pages

The workflow builds the empty template and browser engine, runs tests, prepares
an explicit allowlist, uploads a Pages artifact, then deploys on `main`.
Enable repository Settings → Pages → Source → **GitHub Actions**. Private-source
Pages hosting depends on the account's GitHub plan. This repository's visibility
is not changed by the workflow or scripts.

The publication allowlist contains HTML/CSS/JS, the empty template/manifest,
blank-engine WASM, UI syllable annotations, open-source licenses, and an allowlisted corresponding-source
archive. It never copies `local/`, `build/`
as a directory, recordings, firmware dumps or generated user packages. Public
CI uses a synthetic test bank only; no NSX39 secrets are required.

For simulator reproduction, build
[fm1-emu](https://github.com/joakimeriksson/fm1-emu) at
`eef6d957aa94d3dfa74551688d707b984b792922` with `cargo build --release -p fm1-emu`.
Its dependencies and Rust toolchain are not required to build this firmware.
