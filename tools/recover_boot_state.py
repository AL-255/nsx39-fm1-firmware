#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Recover this FM-1_939 boot-loop state using RAM only; never program flash.

Default: read-only preflight. --apply uploads a SHA-256-pinned 512-byte recovery
stub to SRAM, verifies it, then executes it. The stub clears the faulty boot
marker/count and RAM update request, and resets through the existing ROM/SPL.
It contains no flash driver. Root may be needed for /dev/sg0.
"""
import argparse,ctypes,fcntl,hashlib,json,os,struct
from pathlib import Path
from read_uboot_diagnostics import SG,receive
from uboot_codec import crc16,mengli
R=Path(__file__).resolve().parents[1]
LOAD=0x01c02000
PAYLOAD_SHA='8a20a4f22dd676bd0ad69fe36549194f0238f200d6665dfcdb4b70341af8d13d'

def read_ram(fd,address,n):
 if (address,n) not in [(0x01c7c040,8),(0x01c7fd80,16),(LOAD,512)]:raise ValueError('SRAM read allowlist')
 return mengli(receive(fd,b'\xfd\x07'+struct.pack('>IH',address,n)+b'\xff'*8,n))

def checked_state(state,request,expected_marker):
 marker,attempts=struct.unpack('<II',state)
 if marker!=expected_marker or marker==0x4e535842 or not marker&255 or attempts!=0:
  raise ValueError(f'Boot state changed or does not match the diagnosed failure: marker={marker:#010x}, attempts={attempts}')
 if request not in [b'\0'*16,b'usb_update_mode\0']:
  raise ValueError('Unexpected RAM update request; stop and inspect before changing it')
 return {'marker':f'{marker:08x}','attempts':attempts,'update_request':request.hex()}

def transfer(fd,cdb,data,write=False):
 # The only outbound commands are a fixed-address SRAM upload and its jump.
 if write:
  if len(data)!=512 or cdb[:8]!=b'\xfb\x06'+struct.pack('>IH',LOAD,512):raise ValueError('SRAM upload allowlist')
 else:
  if cdb!=b'\xfb\x08'+struct.pack('>IH',LOAD,0)+b'\xff'*8 or len(data)!=16:raise ValueError('SRAM entry allowlist')
 cmd=ctypes.create_string_buffer(cdb);buffer=ctypes.create_string_buffer(data);sense=ctypes.create_string_buffer(64)
 h=SG();h.interface_id=ord('S');h.dxfer_direction=-2 if write else -3;h.cmd_len=len(cdb);h.mx_sb_len=64;h.dxfer_len=len(data);h.dxferp=ctypes.addressof(buffer);h.cmdp=ctypes.addressof(cmd);h.sbp=ctypes.addressof(sense);h.timeout=2000
 fcntl.ioctl(fd,0x2285,h)
 if h.status or h.host_status or h.driver_status or h.resid or h.info&1:raise OSError(f'SCSI transfer failed: status={h.status}, host={h.host_status}, driver={h.driver_status}, sense={sense.raw[:h.sb_len_wr].hex()}')
 return buffer.raw[:len(data)]

def recover(fd,payload,expected_marker,apply=False):
 if len(payload)!=512 or hashlib.sha256(payload).hexdigest()!=PAYLOAD_SHA:raise ValueError('Recovery payload hash mismatch')
 before=checked_state(read_ram(fd,0x01c7c040,8),read_ram(fd,0x01c7fd80,16),expected_marker)
 print('Verified diagnosed boot state:',json.dumps(before),flush=True)
 if not apply:return before
 cipher=mengli(payload)
 cdb=b'\xfb\x06'+struct.pack('>IH',LOAD,512)+b'\0'+struct.pack('<H',crc16(cipher))+b'\xff'*5
 transfer(fd,cdb,cipher,write=True)
 if read_ram(fd,LOAD,512)!=payload:raise ValueError('SRAM readback failed; recovery code was NOT executed')
 print('Pinned SRAM recovery code uploaded and verified. Requesting core reset; flash is not written.',flush=True)
 try:
  reply=transfer(fd,b'\xfb\x08'+struct.pack('>IH',LOAD,0)+b'\xff'*8,bytes(16))
  if reply[:2]!=b'\xfb\x08':print('ROM execution reply was not recognized; check USB state before doing anything else.',flush=True)
 except OSError as error:
  print('Execution reply interrupted (the reset may have disconnected USB):',error,flush=True)
 print('Check the FM-1 screen and USB/MIDI enumeration. Do not repeat this command if it stays black.',flush=True)
 return before

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--device',default='/dev/sg0');ap.add_argument('--expected-marker',type=lambda s:int(s,0),required=True);ap.add_argument('--apply',action='store_true');a=ap.parse_args()
 payload=(R/'build/recovery/boot-state.bin').read_bytes()
 fd=os.open(a.device,(os.O_RDWR if a.apply else os.O_RDONLY)|os.O_NONBLOCK)
 try:
  i=receive(fd,bytes([0x12,0,0,0,36,0]),36)
  if i[8:16].decode('ascii').strip()!='WL82' or i[16:32].decode('ascii').strip()!='UBOOT1.00':raise ValueError('Refusing a device other than WL82 UBOOT1.00')
  recover(fd,payload,a.expected_marker,a.apply)
 finally:os.close(fd)
if __name__=='__main__':main()
