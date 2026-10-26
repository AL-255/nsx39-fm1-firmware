# SPDX-License-Identifier: GPL-3.0-only
"""JieLi MengLi RAM-transfer scrambling (not the flash/SFC cipher).

Protocol reference: kagaimiq/jl-uboot-tool, jltech/cipher.py. WL82 UBOOT uses
this symmetric byte transform for READ_MEMORY and WRITE_MEMORY packets.
"""
MAGIC='孟黎我爱你，玉林'.encode('gb2312')
def crc16(data,crc=0):
 for byte in data:
  crc^=byte<<8
  for _ in range(8):crc=((crc<<1)^(0x1021 if crc&0x8000 else 0))&0xffff
 return crc

def mengli(data):
 out=bytearray(data);crc=crc16(b'\xff\xff',0xffff)
 for i in range(len(out)):
  crc=crc16(MAGIC[i%len(MAGIC):i%len(MAGIC)+1],crc);out[i]^=crc&255
 return bytes(out)
