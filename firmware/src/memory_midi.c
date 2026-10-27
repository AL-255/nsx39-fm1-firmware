// SPDX-License-Identifier: GPL-3.0-only
// Noncommercial SysEx: 7D 'N' 'S' 'X' version command request-id payload checksum.
// Chunked state is validated as a whole before applying or saving. No command
// accepts flash addresses, code, update records or executable payloads.
#ifndef MEMORY_LOCK
#define MEMORY_LOCK() __asm__ volatile("cli":::"memory")
#define MEMORY_UNLOCK() __asm__ volatile("sti":::"memory")
#endif
static uint8_t memory_staged[STATE_BYTES],memory_before[STATE_BYTES],memory_packet[192];
static unsigned memory_mask;static uint32_t memory_time;
static void memory_reply(unsigned command,unsigned id,const uint8_t*data,unsigned n){
 unsigned k=0,sum=0;memory_packet[k++]=0xf0;memory_packet[k++]=0x7d;memory_packet[k++]=78;memory_packet[k++]=83;memory_packet[k++]=88;memory_packet[k++]=1;memory_packet[k++]=(uint8_t)(command|64u);memory_packet[k++]=(uint8_t)id;
 for(unsigned i=0;i<n;i++)memory_packet[k++]=data[i];for(unsigned i=1;i<k;i++)sum+=memory_packet[i];memory_packet[k++]=(uint8_t)(-sum&127u);memory_packet[k++]=0xf7;ota_wire_send(memory_packet,k);
}
static int memory_sysex(const uint8_t*p,unsigned n){
 if(n<8||p[0]!=0x7d||p[1]!=78||p[2]!=83||p[3]!=88||p[4]!=1)return 0;
 unsigned sum=0;for(unsigned i=0;i<n;i++){if(p[i]>127)return 1;sum+=p[i];}if(sum&127)return 1;
 unsigned command=p[5],id=p[6];uint8_t status=4;
 if(fm1_ms-memory_time>10000)memory_mask=0;memory_time=fm1_ms;
 if(command==1&&n==9&&p[7]<9){
  unsigned off=p[7]*128u,len=STATE_BYTES-off;if(len>128)len=128;
  MEMORY_LOCK();instrument_export(memory_before,0);MEMORY_UNLOCK();
  uint8_t payload[150];payload[0]=p[7];unsigned packed=ota_pack7(memory_before+off,len,payload+1);memory_reply(command,id,payload,packed+1);return 1;
 }
 if(command==2&&n>=10&&p[7]<9){
  unsigned off=p[7]*128u,len=STATE_BYTES-off;if(len>128)len=128;
  if(n==9u+(len*8+6)/7){if(!p[7])memory_mask=0;if(ota_unpack7(p+8,n-9,memory_staged+off,len)==len){memory_mask|=1u<<p[7];status=0;}}
 }
 if(command==3&&n==8){
  if(memory_mask!=511||!instrument_valid(memory_staged,STATE_BYTES))status=2;
  else if(nsx_active()||fm1_in.notes||updating)status=3;
  else{
   MEMORY_LOCK();instrument_export(memory_before,0);instrument_import(memory_staged,STATE_BYTES,0);MEMORY_UNLOCK();
   int rc=settings_flush();if(rc){MEMORY_LOCK();instrument_import(memory_before,STATE_BYTES,0);MEMORY_UNLOCK();status=rc==-3?3:rc==-2?6:5;}else status=0;
  }
  memory_mask=0;
 }
 memory_reply(command,id,&status,1);return 1;
}
