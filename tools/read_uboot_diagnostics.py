#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Read fixed SRAM diagnostics from an FM-1 already in mask-ROM UBOOT.

Only SCSI INQUIRY (0x12) and JieLi READ_MEMORY (0xFD07) are implemented.
No flash erase/program, RAM upload, jump, reset, or chip-key operation exists here.
Protocol reference: https://github.com/kagaimiq/jl-uboot-tool
Run with sudo only when device permissions require it.
"""
import argparse,ctypes,fcntl,json,os,struct
from pathlib import Path
class SG(ctypes.Structure):
 _fields_=[('interface_id',ctypes.c_int),('dxfer_direction',ctypes.c_int),('cmd_len',ctypes.c_ubyte),('mx_sb_len',ctypes.c_ubyte),('iovec_count',ctypes.c_ushort),('dxfer_len',ctypes.c_uint),('dxferp',ctypes.c_void_p),('cmdp',ctypes.c_void_p),('sbp',ctypes.c_void_p),('timeout',ctypes.c_uint),('flags',ctypes.c_uint),('pack_id',ctypes.c_int),('usr_ptr',ctypes.c_void_p),('status',ctypes.c_ubyte),('masked_status',ctypes.c_ubyte),('msg_status',ctypes.c_ubyte),('sb_len_wr',ctypes.c_ubyte),('host_status',ctypes.c_ushort),('driver_status',ctypes.c_ushort),('resid',ctypes.c_int),('duration',ctypes.c_uint),('info',ctypes.c_uint)]
REGIONS=[('entry',0x01c07f20,32),('crash',0x01c7c000,72)]
def receive(fd,cdb,n):
 if cdb[0]!=0x12 and cdb[:2]!=b'\xfd\x07':raise ValueError('Read-only command allowlist')
 cmd=ctypes.create_string_buffer(cdb);data=ctypes.create_string_buffer(n);sense=ctypes.create_string_buffer(64)
 h=SG();h.interface_id=ord('S');h.dxfer_direction=-3;h.cmd_len=len(cdb);h.mx_sb_len=64;h.dxfer_len=n;h.dxferp=ctypes.addressof(data);h.cmdp=ctypes.addressof(cmd);h.sbp=ctypes.addressof(sense);h.timeout=2000
 fcntl.ioctl(fd,0x2285,h)
 if h.status or h.host_status or h.driver_status or h.resid or h.info&1:raise OSError(f'SCSI read failed: status={h.status}, host={h.host_status}, driver={h.driver_status}, residual={h.resid}, sense={sense.raw[:h.sb_len_wr].hex()}')
 return data.raw

def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--device',default='/dev/sg0');ap.add_argument('--output',type=Path,default=Path('/tmp/fm1-boot-diagnostic'));a=ap.parse_args()
 if a.output.exists() and any(a.output.iterdir()):raise SystemExit('Output directory must be empty')
 fd=os.open(a.device,os.O_RDONLY|os.O_NONBLOCK)
 try:
  inquiry=receive(fd,bytes([0x12,0,0,0,36,0]),36)
  vendor=inquiry[8:16].decode('ascii').strip();product=inquiry[16:32].decode('ascii').strip()
  if vendor!='WL82' or product!='UBOOT1.00':raise SystemExit(f'Refusing unexpected SCSI device {vendor!r} {product!r}')
  a.output.mkdir(parents=True,exist_ok=True);report={'device':a.device,'vendor':vendor,'product':product,'regions':{}}
  for label,address,n in REGIONS:
   cdb=b'\xfd\x07'+struct.pack('>IH',address,n)+b'\xff'*8
   data=receive(fd,cdb,n);(a.output/(label+'.bin')).write_bytes(data)
   report['regions'][label]={'address':hex(address),'words':[f'{x:08x}' for x in struct.unpack('<'+'I'*(n//4),data)]}
  (a.output/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
 finally:os.close(fd)
if __name__=='__main__':main()
