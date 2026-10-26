# Firmware-update safety

Hardware installation is currently disabled after a physical boot failure in
`FM-1_939`. Candidate `FM-1_940` fixes a reproduced retained-SRAM boot-marker bug;
physical recovery and validation are pending. Emulator success is not a hardware safety certification.

The unchanged Felucca loader is pinned at commit
`440e62ce8a33d9bb285ab6bbca9a0fd50e459648`. Its portable core and regression tests
are vendored. The browser protocol adds strict identity CRC/length checks,
phase-specific finish addresses and refusal of zero-length/out-of-bounds reads.

## Flash boundaries

| Range | Behavior |
|---|---|
| `0x000000–0x004000` | Existing SPL/flash head/identity: loader never erases or writes |
| `0x004000–0x093000` | Application area: written only by the RAM loader |
| `0x093000–0x100000` | Existing data preserved except recognized update records |
| `0x0e0000–0x0e5000` | Running application's only write window: OTA loader staging |
| `0x0e4f00` | Staged update record, committed after verification and host finish |
| `0x0e8f00` | Stock update-record position, cleared by loader after successful install |

The package flash length must be exactly `0x93000`. Its app area must decrypt
with the key read from **the device's own flash head**. The whole flash payload's
CRC is checked before erase; each sector is checked against that preflight before
writing, read back after writing, and skipped when already equal. The update
record remains available to resume after power loss until installation completes.
The loader only clears known valid update records, preserving neighboring user data.

The app rejects unknown flash JEDEC IDs at update entry (tested ID `0x856014`).
It halts a possibly inherited second core before touching shared boot RAM, uses
watchdog/exception recovery, initializes volume before audio, and places the
flash driver in SRAM. `tools/build.py` checks image bounds and entry, RAM budget,
empty-bank state and obvious direct calls from SRAM flash code into XIP.

## Before a physical test

Back up the FM-1 and retain its official updater/package. Use stable power and a
reliable USB data cable; close MIDI software and other updater tabs. Generate and
save the exact `.fwsc`, and try it in fm1-emu first. Do not unplug, close the tab,
or sleep the host during a write. The page prevents ordinary navigation and
requests a screen wake lock where supported, but the OS can still interrupt it.

If interrupted, regenerate the same package from the same dump/build, reconnect,
and press Install. The installer checks for a waiting loader and resumes before
starting a new session. A final matching identity is required before reporting
success. Holding OCT− and OCT+ for five seconds on this app enters UBOOT.
If the device does not enumerate, use the documented
[FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter) recovery path;
additional hardware may be needed. Do not assume every failure is USB-recoverable.

No physical device was flashed during development. Hardware installation is a
separate, explicit user action. This does not replace the device's mask-ROM/SPL.
