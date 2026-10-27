#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Controlled local installation of the fixed NSX39 debug candidate.

Default is preflight only: file hash/product validation and identity query.
--apply is the separate, explicit action that starts the two-stage flash update.
Uses the vendored Felucca installer and the existing python-rtmidi backend.
"""
import argparse,hashlib,queue,re,sys,time
from pathlib import Path
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'vendor/felucca/tools'))
import fm1_install as I
class RtMidiLink:
 def __init__(self,backend,name,outname):
  self.backend=backend;self.name=name;self.q=queue.Queue();self.inp=backend.rtmidi.MidiIn();self.out=backend.rtmidi.MidiOut()
  try:
   self.inp.ignore_types(sysex=False,timing=True,active_sense=True)
   self.inp.open_port(self.inp.get_ports().index(name));self.out.open_port(self.out.get_ports().index(outname))
   self.inp.set_callback(self._receive);time.sleep(I.DELAY['open'])
  except Exception:self.close();raise
 def _receive(self,event,_):
  b=bytes(event[0])
  if b and b[0]==0xf0 and b[-1]==0xf7:self.q.put(b)
 @property
 def lost(self):return self.name not in self.backend.input_names()
 def send(self,packet):
  try:self.out.send_message(list(packet));return True
  except Exception:return False
 def read(self,timeout):
  try:return self.q.get(timeout=max(0,timeout))
  except queue.Empty:return None
 def drain(self):
  while not self.q.empty():self.q.get_nowait()
 def close(self):
  for p in [self.inp,self.out]:
   try:p.close_port()
   except Exception:pass
class RtMidiBackend:
 def __init__(self):
  import rtmidi
  self.rtmidi=rtmidi
 def input_names(self):return self.rtmidi.MidiIn().get_ports()
 def output_names(self):return self.rtmidi.MidiOut().get_ports()
 def open(self,name,out):return RtMidiLink(self,name,out)
class CheckedUpdater(I.Updater):
 def serve(self,link,image,finish,idle,progress):
  served=0;last=time.monotonic()
  while True:
   packet=link.read(I.DELAY['poll'])
   if packet is None:
    if link.lost:return served,False,True
    if time.monotonic()-last>idle:return served,False,False
    continue
   request=I.parse_request(packet)
   if request is None:continue
   fl,address,n=request;last=time.monotonic()
   if address in [I.FINISH_CHECK,I.FINISH_WRITE]:
    if address!=finish or n!=8 or not served:raise I.InstallError('badreq','Unexpected completion gate')
    sent=link.send(I.response(address,b'success\0'));return served,sent,not sent
   if not n or n>I.MAXDATA or address+n>len(image):raise I.InstallError('badreq','Invalid package read request')
   time.sleep(I.DELAY['reply'])
   if not link.send(I.response(address,image[address:address+n],fl)):return served,False,True
   served+=1;progress(served,address+n)
def validate_candidate(path,sha):
 raw=path.read_bytes()
 if not re.fullmatch('[0-9a-f]{64}',sha) or hashlib.sha256(raw).hexdigest()!=sha:raise ValueError('Candidate SHA-256 mismatch')
 product,image=I.load_package(str(path),False)
 if product!='FM-1_945':raise ValueError('Only fixed candidate FM-1_945 is accepted')
 return product,image
def main():
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('package',type=Path);ap.add_argument('--sha256',required=True);ap.add_argument('--expected-current',default='FM-1_944');ap.add_argument('--apply',action='store_true');a=ap.parse_args()
 product,image=validate_candidate(a.package,a.sha256.lower());up=CheckedUpdater(RtMidiBackend());dev=up.find()
 if not dev:raise SystemExit('No FM-1 identity reply. Close other MIDI/installer applications and retry preflight only.')
 print('Connected:',dev.id.text,'on',dev.name,flush=True);print('Candidate:',product,'SHA-256',a.sha256,flush=True)
 if dev.id.text!=a.expected_current:dev.link.close();raise SystemExit('Current identity differs from the explicitly expected firmware; nothing written')
 if not a.apply:dev.link.close();print('Preflight passed. No update command was sent.');return
 def progress(phase,value=None):
  if phase!='write' or value in [0,25,50,75,100]:print(phase,value if value is not None else '',flush=True)
 result=up.install(dev,image,product,progress);print('Verified installed identity:',result,flush=True)
if __name__=='__main__':
 try:main()
 except (ValueError,I.InstallError) as error:raise SystemExit(str(error))
