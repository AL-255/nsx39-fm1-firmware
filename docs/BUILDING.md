# Building and publishing

All tools are host-side except the pi32v2 app and RAM loader. The build does not
consume an NSX39 dump. Linux x86-64 is the tested hardware-compiler host.

Prerequisites: Python 3 + Pillow, Node.js 20+, GCC/CC, Git, curl, xz, Clang 18 and
wasm-ld, and `/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf`.
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
SHA-256 and SDK commit, and `build.py` checks all three SDK input hashes. The
JieLi download URL can change its latest release: if the hash changes, stop and
audit the new compiler rather than accepting it silently.

Outputs: `build/voice-app.bin`, `build/voice.elf`, `build/loader/ota.bin`,
`build/template.fwsc`, `build/manifest.json`, and `build/voice-engine.wasm`.
The template's voice reserve is all zeros and cannot produce vocals until the
browser injects the locally extracted bank. The generated template is suitable
for packaging and simulator checks, not a useful stand-alone instrument.

Native firmware and browser preview share `firmware/src/voice.c`. The public bank
format is defined by `web/lib/bank.js` and validated at startup by `nsx_init`.
It is little-endian, version 1, with CRC32 covering header and payload (CRC field
zero during calculation), 449 40-byte descriptors, names, pronunciation strings,
four curves, PCM steady cycles and IMA transition blocks. Pitch and stretch
streams remain signed Q16.16/integer metadata as recovered. Reserves/padding
are zero; the unused end of the bank slot is not included in the bank CRC.

## GitHub Pages

The workflow builds the empty template and browser engine, runs tests, prepares
an explicit allowlist, uploads a Pages artifact, then deploys on `main`.
Enable repository Settings → Pages → Source → **GitHub Actions**. Private-source
Pages hosting depends on the account's GitHub plan. This repository's visibility
is not changed by the workflow or scripts.

The publication allowlist contains HTML/CSS/JS, the empty template/manifest,
blank-engine WASM, open-source licenses, and an allowlisted corresponding-source
archive. It never copies `local/`, `build/`
as a directory, recordings, firmware dumps or generated user packages. Public
CI uses a synthetic test bank only; no NSX39 secrets are required.

For simulator reproduction, build
[fm1-emu](https://github.com/joakimeriksson/fm1-emu) at
`eef6d957aa94d3dfa74551688d707b984b792922` with `cargo build --release -p fm1-emu`.
Its dependencies and Rust toolchain are not required to build this firmware.
