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

At this initial validation, no connected browser was available, and no physical
FM-1 had been flashed. Later physical update and Chrome tests are recorded below.
Physical FM-1 sound, printed-control mapping across hardware revisions and
power-interruption testing remain pending.

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

## Physical installation and live Pages test (2026-10-26)

The user explicitly requested an installation attempt on the connected FM-1.
The device answered `FM-1_941` on its Felucca USB-MIDI port. The adjacent NSX39
MIDI device was excluded from update discovery and was not an install target.

The candidate was checked before installation:

- Product: `FM-1_942`.
- Package SHA-256: `7db10c2448c15d631887a04f01f05b31afbff55045a3ab2c188fe610e647c03b`.
- Package size: 610,090 bytes; logical update image: 610,070 bytes.

The native RtMidi installer completed staging, loader verification and application
transfer, then observed `FM-1_942` after automatic restart at 57.8 seconds.

Chrome 155.0.8059.84 in a separate headless test profile then loaded the actual
HTTPS Pages generator, selected the local original dump, and generated the same
package hash. The first browser connection failed at discovery with no package
requests. A direct browser MIDI identity probe subsequently succeeded. Repeating
the complete Pages flow then succeeded: the visible progress reached 100%,
reported 2,341 data requests / 1,169.8 KiB, and ended with `Installed FM-1_942`
after about 52 seconds. This second run installed the identical package, allowing
the loader to skip application sectors already equal to their requested content.
No reset/erase fallback, privileged recovery or power interruption was used.

The exact cause of the initial browser discovery miss is not established. The
follow-up hardening adds the native installer's 300 ms port-settling delay,
three identity attempts and a fresh discovery pass, plus specific port errors.
Public regressions cover a settling MIDI transport, repeated discovery, port-open
failure and preserving failed-resume errors. The application and RAM loader
binary are unchanged by this browser fix. Full physical audio/display and
power-cycle checks are still separate from successful update/restart identity.

## Editable memories, settings and retained flash (FM-1_943)

Public tests now cover kana/katakana/romaji parsing, the 63-syllable limit,
browser storage, settings CRC/range validation, applying an embedded settings
seed at startup, preserving lyric position during parameter changes, and reset
to the original factory table. C/WASM state records round-trip identically.
The private genuine-dump integration also repackages an edited memory, checks
the resulting app bank, and boots that bank through the shared WASM controller.

The native C journal tests simulate 2,574 first-save/program/rollover cut points,
each followed by a restart and a further successful save, plus duplicate-save suppression, CRC
rejection, foreign/future-format protection and generated-seed precedence.
Commit-read failures are injected too: retries receive unique sequence numbers,
and an uncertain commit cannot be incorrectly skipped as an unchanged save.
They assert that writes stay inside `0xE5000–0xE7000` and that the prior committed
record survives an interrupted replacement. AddressSanitizer and UBSan are
enabled. The actual firmware memory-SysEx handler is exercised against synthetic
flash, including chunk reads, missing/invalid payloads, busy-key refusal,
durable commit and restart restoration. Browser protocol framing/readback is
also tested with an independent fake peer.

The real generated pi32v2 application was run in the pinned emulator with OCT
chords, knob navigation, parameter changes, lyric playback and all label scripts.
The 4,500 ms session wrote a committed journal record containing attack 200 ms,
memory 6, romaji and OCT0. Its raw sector contents were carried into a fresh
emulator process; the new process restored those values, confirmed by both a
RAM read of the ADSR and a settings-screen capture. A second settings-page turn
confirmed reset, producing a newer committed record with all original lyric
memories and default parameters. Screen captures cover reset arming, confirmation
and the final Saved state. No physical device was flashed for this change.

For retained-flash tests, use a populated `--bin`, its matching decrypted
`--flash`, and the retained `--raw-flash`. In the pinned emulator, `--fwsc` calls
`stock_boot` after loading `--raw-flash`, replacing its retained contents with
the package's flash. That combination therefore does not model a power cycle
with saved settings.

Chrome testing in an isolated headless profile covered generating from the local
dump, editing a memory using kana, an updated download hash, playback of that
edited memory, multiple upcoming syllables, remembered ADSR values and restoring
the editor after reload. The native 240×240 layout and the webpage were visually
inspected. Firmware data stays in ignored local files. Physical persistence,
new memory-protocol writes and this build's audio/display behavior remain pending
hardware validation; the prior FM-1_942 installation result is not extended to 943.

## Independent device tools, DoReMi and black UI (FM-1_944)

The webpage now has independently loaded Firmware, Lyric memories and Device
settings tabs. Chrome tests in an isolated profile used a synthetic MIDI peer to
read and edit all sixteen slots, save parameters, verify preservation of unrelated
state, lock tab changes during transactions, and restore drafts after reload.
Network inspection confirmed that device-only visits fetch no firmware assets,
WASM or generation worker. A genuine-dump browser run generated FM-1_944, checked
the exact eighteen-mode order, played C/do through the AudioWorklet, checked the
last/next display, then saved a lyric over simulated SysEx and confirmed that the
firmware package hash remained unchanged. No physical device writes were made.

The public suite passes 38 JavaScript tests and 11 Python tests. New cases cover
pitch-class DoReMi over three octaves, held-key fallback, SELECT inactivity in
DoReMi, wrapping through all sixteen lyric memories, and state format 1/2
migration. The actual C journal accepts an old committed selection, translates
it once, saves format 2 and restores it without translating again. Saved device
state now wins over a different package seed, replacing 943's seed precedence.
Old-device protocol tests verify format 1 encoding for FM-1_943 and rejection of
unsupported DoReMi/Lyrics 00 selection before any staging commands are sent.
Journal power-cut/bounds tests continue under AddressSanitizer and UBSan.

The latest real pi32v2 template boots with SRAM filled with 0x00, 0xA5 and 0xFF:
each run reports the LCD on, eight frames and 140 audio halves at 550 ms. Native
OTA and loader tests pass, including interruption/refusal and preservation of the
flash head, settings storage and writes outside allowed windows. The loader and
its flash layout are unchanged. The generated vocal package was run in the
pinned emulator; captures show C/do in DoReMi and original bank 00 in Lyrics 00.
Its 240×240 UI background is RGB(0,0,0), including antialiased glyph backgrounds.
Chrome computed styles confirm black page, editor and graph backgrounds with
Miku cyan accents.

The final public template SHA-256 is
`afa9fdcd63ed6f4f9ef6b9eaf742c50a83868cab00b43c68080c802d45efcab8`.
The app is 566,160 bytes inside the 581,564-byte slot, with 54,552 bytes of normal
RAM and a 117,888-byte pool. The genuine bank remains 484,172/491,520 bytes.
Private integration plays all sixteen extracted banks through the shared
controller and compares six native/WASM renders sample for sample. Physical
validation of the new MIDI editors and FM-1_944 remains pending; the previously
verified FM-1_942 installation is not evidence of a physical 944 test.


## Idle clicks and uninterrupted saves (FM-1_945)

The reported symptom was a click near the end of a long release, followed by
periodic idle clicks (the user estimated about once a second). The recorded-unit
renderer produced exact zero after release in both WASM and the actual pi32v2
emulator. The native application nevertheless disabled/re-enabled ALNK audio
when autosave ran; occupied storage made it repeat that sequence on every retry.
This is a confirmed clock interruption capable of disturbing the external codec.
The emulator does not model that codec's analog pop, so the user's exact audible
symptom still needs physical confirmation.

The diagnostic scenario set release to 4,000 ms and played three panel notes,
last released at 1,120 ms. An emulator-only ALNK enable trace observed the old
application stop/restart at cycles 1,267,283,549/1,267,627,943 on a successful save.
With synthetic foreign journal sectors, it stopped/restarted at
1,267,283,549/1,267,362,749 and again at 1,747,763,583/1,747,842,469. The latter
are about two seconds apart, matching the autosave retry interval; this is not
an exact timing reproduction of the user's estimate. Occupied storage remained
untouched and reported status -2. The fixed save path had only the initial
ALNK enable in the same eight-second successful/occupied-storage scenarios.

The fix waits for two audio interrupts to fill the complete DMA ring with
silence, then leaves DMA and the codec clock running while the RAM flash driver
masks interrupts. Maintenance also clears the interpolator's previous sample,
so the silent halves cannot contain a residual sample after panic/zero release.
Admission is serialized against note handling and a 20 ms timeout defers the
save if audio is not advancing. Saving, verification and journal bounds are
unchanged; there are no loader/update-routine changes.

Public tests pass 40 JavaScript and 11 Python cases. The actual save policy is
host-tested under ASan/UBSan for busy notes/MIDI, complete silent halves before
flash access, frame-counter wrap, stopped-audio timeout, deferred long release,
failed-save retries, uncertain commits and no repeated saves after success.
A DSP regression retriggers five notes with one-/four-second releases and checks
20 seconds of exact silence afterward. Original-dump integration, OTA/loader
checks and zero/A5/FF SRAM startup tests pass. No physical FM-1 was flashed or
recorded for this fix.

The final app is 566,176 bytes; template SHA-256:
`1e8af2f71e8db24edb2ce7fdfbd3a7d226659d324b9c33cae7a016b5c3ac7e53`.


## One save attempt per settled change (FM-1_946)

Autosave keeps an unsaved-state flag separately from its pending-attempt flag.
A changed configuration schedules one attempt two seconds after the last change,
once notes, release and queued MIDI are idle. The attempt is consumed before
waiting for the silent DMA ring. Failure or uncertain commit readback does not
re-arm it. The error stays visible; a later parameter change or an explicit
webpage save can attempt again. An explicit successful save also consumes a
pending autosave, avoiding a second save of the same state. The journal retains
its unchanged-state deduplication and unique sequence numbers after uncertain
commits. Its flash bounds and the update loader are unchanged.

The actual C autosave policy is tested under ASan/UBSan with rapid changes,
a long held/releasing voice, a failed attempt followed by over a minute of idle
polls, another edit, explicit saves before the debounce deadline, failed explicit
saves, uncertain commits and audio-ring timeout. Only the appropriate single
attempt occurs in each case. All 40 JavaScript and 11 Python tests pass.
The existing long-release silence and uninterrupted-DMA checks still apply.
No physical hardware was flashed for this change.

Template SHA-256:
`7785766f4751061b3c83c4b282f9917cc229444e188e12e156cf0a1deb0ce03e`.
The app is 566,184 bytes; the bank remains within its existing reserve.

## On-screen syllable keyboard

The lyric-memory webpage now exposes all 128 syllables as kana/romaji buttons.
Chrome testing with synthetic browser drafts verifies every button parses back
to its exact ID, cursor insertion and selection replacement, explicit spacing
between tokens, distinct nasal variants, rejection of a 64th syllable, replacement
at the 63-syllable limit, browser draft persistence and retained editor focus.
The black/cyan desktop and four-column mobile layouts were visually inspected;
the mobile page has no horizontal overflow. Keyboard clicks change browser drafts
only. No MIDI writes, firmware changes or physical device tests were needed.
