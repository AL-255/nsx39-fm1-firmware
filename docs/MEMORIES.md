# Lyric memories and permanent settings (FM-1_943)

The webpage edits memories 1–15; memory 0 remains the manual syllable selector.
Each memory holds 1–63 pronunciation IDs and loops. Enter hiragana, katakana or
romaji, with optional spaces/punctuation. Exact IDs use `#0` through `#127`;
these also distinguish the nasal variants. Unsupported characters and oversized
memories are rejected before generating or sending anything.

**Keep changes** saves the selected memory in this browser, updates the live
preview, and rebuilds the firmware download without recompressing the recordings.
The downloaded package includes all edited memories and the current preview
parameters. The original factory table is retained separately, allowing
**Restore original memory** and the firmware's reset option to restore its source
defaults. Browser storage contains settings and syllable IDs, never the dump or
audio. Clearing the site's storage removes its remembered edits.

After installing FM-1_943, **Read from FM-1** reads all memories and settings.
**Save this memory to FM-1** first reads the instrument's state, replaces only
the selected memory, transfers the complete validated configuration, commits it
to the settings journal, then reads it back. Other device parameters and memories
are preserved. Release keys and sustain hold before saving. The webpage reports
success only after the durable-save acknowledgement and matching readback.
Older firmware is detected and refused before any memory-write command is sent.

## Panel and display

- Briefly press and release both OCT buttons together to open or close settings.
- Individual OCT buttons still change octave. Holding both for five seconds
  retains the existing UBOOT recovery entry; it is not a settings shortcut.
- In settings, knob 1 selects an item and knob 2 changes its value. Knobs 3/4
  do not alter the ADSR while settings is open. Outside settings, knobs 1–4
  still set attack, decay, sustain and release.
- Settings include octave, label script, memory/manual syllable, ADSR, automatic
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
change, once the keys/voice are idle. Active notes, lyric playback position,
last-played text, pitch bend and the physical MASTER position are not restored
as sounding performance state. This avoids writing flash on every played note.
The settings screen reports pending, saved or failed/occupied storage.

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

A new generated settings seed takes precedence over records from a different
seed. An unchanged seed preserves the instrument's saved changes across firmware
updates. Thus explicitly edited packages take effect without silently discarding
ordinary device changes on every update.

The private, noncommercial MIDI envelope is `F0 7D 4E 53 58 01 command request-id
payload checksum F7`, with a 7-bit additive checksum. Commands read chunks,
stage chunks, or validate/apply/commit a full state. Chunks hold at most 128 raw
bytes encoded with the updater's 8-to-7 packing. Configuration CRC and all ranges
are checked before application. This protocol carries no executable code and
does not invoke the updater.
