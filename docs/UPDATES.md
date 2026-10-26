# Firmware-update safety

Hardware installation is enabled for candidate `FM-1_941`, which includes the
retained-SRAM boot-marker fix and new lyric UI. The earlier `FM-1_939` failed to boot.
RAM-only recovery succeeded, returning the unit to its existing `FM-1_939`.
Physical cold-boot validation of the new candidate is pending. Emulator success is not a hardware safety certification.

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

The browser displays separate progress for loader staging, package verification,
application transfer and restart. Coverage is calculated from distinct requested
byte ranges in the actual package; retries count toward traffic statistics but
do not advance the bar twice. These are transfer measurements, not acknowledgements
of individual flash writes. The bar stays below 100% until the device reconnects
with the expected identity. USB reconnection waits are indeterminate, with elapsed
time and last-response age visible. Failures preserve the last progress and the
recovery message. Loader behavior, timeouts and flash boundaries are unchanged.

If the device does not enumerate, use the documented
[FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter) recovery path;
additional hardware may be needed. Do not assume every failure is USB-recoverable.

The first user-installed candidate failed to boot and was recovered without a
flash write. Installation of a replacement is a separate, explicit user action. This does not replace the device's mask-ROM/SPL.

## RAM-only recovery for the diagnosed FM-1_939 marker failure

The physical report is MengLi-scrambled. Decoding it locally showed valid SPL
entry parameters, an invalid nonzero retained marker and a zeroed attempt counter,
consistent with the faulty early UBOOT branch. There was no valid crash record.
The supplied diagnostic now prints both raw and decoded words.

`tools/recover_boot_state.py` defaults to read-only preflight. With explicit
`--apply`, it checks the expected marker and update-request state, uploads a
SHA-256-pinned 512-byte stub to SRAM `0x01c02000`, reads it back, and only then
executes it. The stub's GPL source is in `firmware/recovery/`. It clears the two
old boot-state words and the 128-byte RAM mailbox, then requests a core reset.
No flash/NOR/SFC driver is linked, and the host helper has no erase/program or
chip-key command. The existing installed firmware is left in flash.

This is a temporary recovery step, not installation of the fixed image. A failed
preflight/readback stops before execution. If the device stays black, do not
repeat blindly or erase flash. Keep the physical diagnostic and use its result
to choose the next step. Root device access may require sudo; do not send a
password in chat. The user successfully ran this recovery; the existing
`FM-1_939` subsequently answered the FM-1 identity query.


## Controlled candidate preflight

`tools/install_candidate.py` validates a locally generated candidate's SHA-256,
checks its product is `FM-1_941`, and queries the connected FM-1 identity. It uses
the vendored GPL Felucca installer with stricter request/completion gates and the
existing Python RtMidi backend. The default sends no update command. The current
identity must equal `--expected-current` (default `FM-1_939`). `--apply` starts the
actual two-stage flash update and must be an explicit hardware-test decision.
The NSX39 MIDI device is not an install target. The public page allows explicit
installation of the fixed candidate after generation and acknowledgement;
physical cold-boot validation remains pending.
