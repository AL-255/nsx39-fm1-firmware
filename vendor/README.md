# Third-party sources

`felucca/` and `web/lib/fm1pkg.js`, `web/lib/fm1ota.js`, `firmware/crt0.S`,
`firmware/app.ld` originate from [hugelton/Felucca](https://github.com/hugelton/Felucca),
commit `440e62ce8a33d9bb285ab6bbca9a0fd50e459648`, GPL-3.0-only,
Copyright 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments.
Their copyright headers are retained. Local changes are recorded in this repo's history.
The hardware layer and two-stage loader are reused; the Felucca synthesis engines are not.

Build-only SDK input: [Jieli-Tech/fw-AC79_AIoT_SDK](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK),
commit `d179b4484759423312073f5fbb232501aa491047`, tag
`AC79NN_SDK_V1.2.1_2023-12-13`, Apache-2.0. Its three files `uboot.boot`,
`cfg_tool.bin`, `cfg/eq_cfg_hw.bin` are downloaded at build time and checked by
SHA-256. The generated template includes these redistributable SDK components;
their license is `Apache-2.0.txt`. The template is never a vendor FM-1 firmware.
The loader preserves the device's existing flash head, including its SPL and identity.

NSX39 data format and DSP research: [AL-255/NSX39-Disasm](https://github.com/AL-255/NSX39-Disasm).
No dump, audio payloads, transformed Yamaha program, or extracted tables from that
repository are vendored. The extractor reads these from the user's local file.


Display fonts: DejaVu Sans Mono for small debug rows (`DejaVu-LICENSE.txt`),
and Noto Sans CJK JP Bold for readable 40-pixel hiragana/katakana/romaji
(`Noto-CJK-LICENSE.txt`, SIL Open Font License 1.1). `tools/gen_label_font.py`
rasterizes a Unicode subset from the build host's font; the original font file
is not vendored. UI syllable readings in `web/syllables.json` are annotations;
no original voice or lyric sequence is stored there. Original lyric banks are
extracted only from the user's dump.

The controlled local installer and its simulated-device tests originate from
Felucca's `tools/fm1_install.py` and `tests/install_test.py` at the pinned commit
above. The local parser checks complete identity/request headers and identity checksums.
The tests omit the unrelated sample-upload CLI, produce complete wire headers,
and use this project's package/parser paths for cross-language comparison.
