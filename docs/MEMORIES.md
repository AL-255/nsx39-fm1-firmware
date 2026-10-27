# Lyric memories and permanent settings (FM-1_946)

The webpage has three independent tabs: **Firmware**, **Lyric memories**, and
**Device settings**. The two device editors use USB-MIDI SysEx; neither loads
firmware assets nor requires an NSX39 dump. Generated firmware always starts
with factory defaults. Preview preferences affect only the browser preview.

ALGORITHM cycles **Selectable syllable → DoReMi → Lyrics 00–15**. Selectable
syllable retains its SELECT choice. DoReMi uses fixed Do: C/do, D/re, E/mi,
F/fa, G/so, A/ra, B/shi; sharps use the preceding natural note’s syllable.
SELECT has no action in DoReMi. Each lyric holds 1–63 syllables and loops;
SELECT left rewinds it and right remains reserved. All sixteen source banks,
including bank 00, are available. Original bank numbers are retained.

In **Lyric memories**, connect/read, choose 00–15, and enter hiragana, katakana
or romaji with optional spaces/punctuation. Exact IDs use `#0` through `#127`.
Browser drafts survive reloads. **Save memory to FM-1** reads fresh device state,
replaces only that bank, commits it, and verifies readback. **Reload selected
memory** discards that bank’s browser draft. Reading all banks retains drafts.

In **Device settings**, connect/read, change parameters, then save. Only changed
parameters are merged into freshly read device state; lyrics and untouched
parameters are preserved. **Use default values** stages defaults for review;
**Save settings to FM-1** applies them. Browser drafts contain settings and
syllable IDs, never the dump or audio. Clearing site storage removes drafts.

Release keys and sustain hold before saving. Both editors report success only
after durable acknowledgement and matching readback. FM-1_943 supports these
editors with its older mode list; DoReMi and Lyrics 00 selection require
FM-1_944. Older firmware is refused before memory-write commands.

## Panel and display

- Briefly press and release both OCT buttons together to open or close settings.
- Individual OCT buttons still change octave. Holding both for five seconds
  retains the existing UBOOT recovery entry; it is not a settings shortcut.
- In settings, knob 1 selects an item and knob 2 changes its value. Knobs 3/4
  do not alter the ADSR while settings is open. Outside settings, knobs 1–4
  still set attack, decay, sustain and release.
- Settings include octave, label script, mode/manual syllable, ADSR, automatic
  pitch/dynamics, vibrato type/rate/depth/delay, glide, noise, connection and hold.
- At **Reset all**, turn knob 2 right to arm, then right again to confirm.
  Selecting another item or exiting cancels the confirmation. Reset restores
  the original factory memories and parameter defaults, including OCT0.

The default register is OCT0 (MIDI 53–79); a saved register is restored on later
boots. The last-played label and smaller upcoming text share a row, with upcoming
syllables extending to the right edge and following the memory's loop. Diagnostic
type is 12 pixels rather than 8. The bottom knob/render-time legend is removed.
Waveforms scale their current minimum/maximum to the available height, with a
small silence floor to avoid amplifying digital silence. Scaling is visual only.

## Saving and flash boundaries

Memories and parameters are saved after two seconds without a configuration
change, once the keys/voice are idle. A burst of changes is combined into one
attempt after the last change. From FM-1_946, failed saves (including uncertain
commit readback) remain visible without periodic automatic retries. Another
parameter change schedules a new attempt; an explicit webpage Save can also
attempt again. Playing notes and remaining idle do not schedule saves. Active notes, lyric playback position,
last-played text, pitch bend and the physical MASTER position are not restored
as sounding performance state. This avoids writing flash on every played note.
The settings screen reports pending, saved or failed/occupied storage.

FM-1_945 keeps I2S/DMA running during saves. Once the voice is idle, it lets the
audio interrupt fill both DMA halves with silence before touching flash. That
silent ring can repeat autonomously while the RAM driver masks interrupts. A
20 ms timeout defers the save if audio stops advancing; it never forces a codec
restart. Explicit subsequent saves also keep the audio clock running.
This also applies to saves requested by the MIDI editors.

The journal uses only `0xE5000–0xE7000`, the two autosave sectors documented by
Felucca, immediately above OTA staging `0xE0000–0xE5000`. The original loader,
boot head, app layout and update-record addresses are unchanged. Settings writes
also require the known flash JEDEC ID. The driver and hooks enforce the journal
bounds independently. The application never exposes arbitrary flash addresses
through the memory protocol.

Each 4 KiB sector contains three 1,120-byte records at offsets `0`, `0x480`,
`0x900`, followed by a separate four-byte commit marker. Padding, including the
last 256 bytes scanned by the updater for boot records, remains erased. A record
contains `NSXS`, format/version, CRC32, generated-seed identity, sequence,
18 bounded 16-bit settings and a 16×64 lyric table. All payload bytes are read
back before writing the commit marker. Rollover erases only the other sector;
the newest valid record is retained until its replacement is committed. Before
touching the other sector, a checksummed rollover intent is written into a
separate metadata page of the current sector. That intent lets a later save
retry an interrupted erase/program without treating torn target bytes as a
foreign format. The updater's last-256-byte scan region remains erased.
Unfamiliar formats are not erased: saving reports occupied storage instead.

A committed device configuration takes precedence over the package’s initial
seed, even when that seed changes. The seed is used only when no valid saved
record exists. Updating firmware therefore preserves saved lyrics and settings.
Reset all restores the original source banks and defaults.

State format 2 uses mode 0 for selectable syllable, 1 for DoReMi and 2–17 for
Lyrics 00–15. Format 1 records migrate automatically: manual stays 0, and an old
nonzero lyric mode gains 2. New journal writes use format 2, so the migration
happens once. The browser converts to format 1 when talking to FM-1_943 and
rejects modes that firmware cannot play before staging data. The SysEx envelope
version remains 1.

The private, noncommercial MIDI envelope is `F0 7D 4E 53 58 01 command request-id
payload checksum F7`, with a 7-bit additive checksum. Commands read chunks,
stage chunks, or validate/apply/commit a full state. Chunks hold at most 128 raw
bytes encoded with the updater's 8-to-7 packing. Configuration CRC and all ranges
are checked before application. This protocol carries no executable code and
does not invoke the updater.
