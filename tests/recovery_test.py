# SPDX-License-Identifier: GPL-3.0-only
"""Offline safety tests: all hardware I/O is replaced by a fake RAM transport."""
import hashlib,struct,sys,unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import recover_boot_state as r
from uboot_codec import crc16,mengli
class Recovery(unittest.TestCase):
 def test_crc_and_scrambling(self):
  self.assertEqual(crc16(b'123456789'),0x31c3)
  b=bytes(range(256))*2;self.assertEqual(mengli(mengli(b)),b)
 def test_state_rejection(self):
  for marker,n,request in [(0x4e535842,0,bytes(16)),(0x12345600,0,bytes(16)),(0x12345601,1,bytes(16)),(0x12345601,0,b'x'*16)]:
   with self.assertRaises(ValueError):r.checked_state(struct.pack('<II',marker,n),request,0x12345601)
 def scenario(self,apply=False,bad_readback=False):
  payload=bytes(range(256))*2;events=[]
  def read(fd,address,n):
   events.append(('read',address,n))
   return struct.pack('<II',0x12345601,0) if n==8 else b'usb_update_mode\0' if n==16 else bytes(512) if bad_readback else payload
  def transfer(fd,cdb,data,write=False):
   events.append(('write' if write else 'jump',cdb))
   if write:self.assertEqual(data,mengli(payload));self.assertEqual(cdb[:8],b'\xfb\x06'+struct.pack('>IH',r.LOAD,512));self.assertEqual(struct.unpack('<H',cdb[9:11])[0],crc16(data))
   return b'\xfb\x08'+bytes(14)
  with patch.object(r,'PAYLOAD_SHA',hashlib.sha256(payload).hexdigest()),patch.object(r,'read_ram',read),patch.object(r,'transfer',transfer):
   if bad_readback:
    with self.assertRaisesRegex(ValueError,'NOT executed'):r.recover(0,payload,0x12345601,True)
   else:r.recover(0,payload,0x12345601,apply)
  return events
 def test_default_is_read_only(self):self.assertEqual([x[0] for x in self.scenario()],['read','read'])
 def test_verified_ram_only_flow(self):self.assertEqual([x[0] for x in self.scenario(True)],['read','read','write','read','jump'])
 def test_bad_readback_never_executes(self):self.assertNotIn('jump',[x[0] for x in self.scenario(True,True)])
 def test_hash_mismatch_never_accesses_device(self):
  with patch.object(r,'read_ram') as read:
   with self.assertRaisesRegex(ValueError,'hash mismatch'):r.recover(0,bytes(512),0)
   read.assert_not_called()
if __name__=='__main__':unittest.main()
