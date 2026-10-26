# SPDX-License-Identifier: GPL-3.0-only
import hashlib,sys,unittest
from collections import deque
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'tools'))
import install_candidate as C
I=C.I

def request(addr,n):
 b=bytearray([0,0x59,0x30,8,0,0,0])+addr.to_bytes(4,'little')+n.to_bytes(3,'little');b.append(~sum(b[6:])&255)
 return b'\xf0'+I.pack7(b)+b'\xf7'
class Link:
 def __init__(self,requests,send_ok=True):self.q=deque(requests);self.sent=[];self.lost=False;self.send_ok=send_ok
 def read(self,timeout):return self.q.popleft() if self.q else None
 def send(self,p):self.sent.append(p);return self.send_ok
class CandidateTests(unittest.TestCase):
 def serve(self,l):return C.CheckedUpdater(None).serve(l,bytes(1024),I.FINISH_WRITE,.01,lambda *_:None)
 def test_wrong_phase_and_premature_completion(self):
  for packets in [[request(I.FINISH_WRITE,8)],[request(0,32),request(I.FINISH_CHECK,8)],[request(0,32),request(I.FINISH_WRITE,7)]]:
   with self.assertRaises(I.InstallError):self.serve(Link(packets))
 def test_read_bounds(self):
  for addr,n in [(0,0),(0,513),(1023,2),(0xffffffff,1)]:
   l=Link([request(addr,n)])
   with self.assertRaises(I.InstallError):self.serve(l)
   self.assertEqual(l.sent,[])
 def test_success_requires_response_delivery(self):
  l=Link([request(0,32),request(I.FINISH_WRITE,8)]);self.assertEqual(self.serve(l),(1,True,False))
  l=Link([request(0,32)],False);self.assertEqual(self.serve(l),(0,False,True))
 def test_header_and_checksum_are_checked(self):
  b=I.unpack7(request(0,32)[1:-1]);self.assertIsNotNone(I.parse_request(request(0,32)))
  for i in [2,3,5,14]:
   x=bytearray(b);x[i]^=1;self.assertIsNone(I.parse_request(b'\xf0'+I.pack7(x)+b'\xf7'))
  d=b'\x00\x59\x11\x1b\0\0'+b'FM-1_939'+bytes(19);packet=b'\xf0'+I.pack7(d+bytes([~sum(d[6:])&255]))+b'\xf7'
  self.assertEqual(I.parse_identity(packet).text,'FM-1_939')
  self.assertIsNone(I.parse_identity(packet[:-2]+bytes([packet[-2]^1])+packet[-1:]))
 def test_candidate_hash_and_product(self):
  p=R/'build/template.fwsc'
  if not p.exists():self.skipTest('Build the blank candidate first')
  digest=hashlib.sha256(p.read_bytes()).hexdigest();self.assertEqual(C.validate_candidate(p,digest)[0],'FM-1_941')
  with self.assertRaises(ValueError):C.validate_candidate(p,'0'*64)
if __name__=='__main__':unittest.main()
