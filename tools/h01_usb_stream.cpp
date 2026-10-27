// SPDX-License-Identifier: GPL-2.0-or-later
// Physical Digitakt mk1 transport diagnostic. Descriptor capacities: R02.
// Codec: pinned Sidecar ob_block.h. No firmware or firmware-derived code.
// Silence by default; --tone is -36 dBFS, with 200 ms fade and -12 dBFS clamp.
#include <libusb-1.0/libusb.h>
#include "ob_block.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <chrono>
#include <vector>

using Clock=std::chrono::steady_clock;
static double now(){return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();}
struct Host;
struct Slot {libusb_transfer *t=nullptr; std::array<uint8_t,368> bytes{}; Host *host=nullptr; bool pending=false;};
static volatile sig_atomic_t interrupted=0;
struct Host {
 libusb_context *ctx=nullptr;libusb_device_handle *dev=nullptr;
 std::array<Slot,16> in{},out{};
 bool running=true,tone=false,pulse=false;int out_channel=0;
 uint64_t incoming=0,outgoing=0,in_errors=0,out_errors=0,bad=0,shorts=0,gaps=0,lost=0,out_busy=0;
 uint16_t last=0,counter=0;bool have=false;
 double start=0,first=0,latest=0,max_callback_gap=0,previous=0;
 unsigned credits=0,max_credits=0;
 uint64_t frames=0;
 std::vector<double> sent_onsets,received_onsets;double last_onset=0;
 std::array<int64_t,12> peaks{};
 std::array<uint8_t,32> header{};
 std::vector<int32_t> capture;size_t captured=0;static constexpr size_t cap_frames=96000;
 static void callback(libusb_transfer *t){static_cast<Slot*>(t->user_data)->host->done(*static_cast<Slot*>(t->user_data));}
 void pump(){
  while(credits&&std::any_of(out.begin(),out.end(),[](const Slot&s){return !s.pending;})){--credits;send();}
 }
 void send(){
  auto it=std::find_if(out.begin(),out.end(),[](const Slot&s){return !s.pending;});
  if(it==out.end()){++out_busy;return;}auto&s=*it;
  std::fill(s.bytes.begin(),s.bytes.end(),0);ob::write_header(s.bytes.data(),counter);counter+=7;
  for(int f=0;f<7;++f,++frames){
   double fade=std::min(1.0,frames/9600.0);
   double gate=1;
   if(pulse){uint64_t position=frames>=48000?(frames-48000)%24000:24000;
    gate=position<2400?std::min(1.0,std::min(position/96.0,(2400-position)/96.0)):0;
    if(position==0)sent_onsets.push_back(now());
   }
   double value=tone?gate*0.0158489319246111*fade*std::sin(2*3.141592653589793*440*frames/48000.0):0;
   value=std::max(-0.25,std::min(0.25,value));
   ob::put_be32(s.bytes.data()+32+f*8+out_channel*4,(int32_t)std::llround(value*2147483647.0));
  }
  int r=libusb_submit_transfer(s.t);if(r){++out_errors;}else{s.pending=true;}
 }
 void done(Slot&s){
  s.pending=false;
  bool input=s.t->endpoint==0x83;
  if(s.t->status==LIBUSB_TRANSFER_CANCELLED&&!running)return;
  if(s.t->status!=LIBUSB_TRANSFER_COMPLETED){if(input)++in_errors;else++out_errors;}
  else if(!input){++outgoing;if(running)pump();}
  else if(s.t->actual_length!=368){++shorts;}
  else if(!ob::block_from_device(s.bytes.data())){++bad;}
  else {
   double at=now();if(!incoming){first=at;std::copy_n(s.bytes.begin(),32,header.begin());}
   if(previous){max_callback_gap=std::max(max_callback_gap,at-previous);}
   previous=at;latest=at;
   uint16_t c=ob::block_counter(s.bytes.data());
   if(have){uint16_t d=c-last;if(d!=7){++gaps;lost+=(uint16_t)(d-7)/7;}}
   last=c;have=true;++incoming;
   for(int f=0;f<7;++f){
    if(pulse&&received_onsets.size()<sent_onsets.size()&&at-last_onset>0.25
       &&std::llabs((long long)ob::block_sample(s.bytes.data(),12,f,10))>100000){
      received_onsets.push_back(at);last_onset=at;
    }
    for(int ch=0;ch<12;++ch){int32_t v=ob::block_sample(s.bytes.data(),12,f,ch);
     peaks[ch]=std::max(peaks[ch],(int64_t)std::llabs((long long)v));
     if(captured<cap_frames&&at-start>0.5)capture[captured*12+ch]=v;
    }
    if(captured<cap_frames&&at-start>0.5)++captured;
   }
   if(running){if(credits>=32)++out_busy;else ++credits;max_credits=std::max(max_credits,credits);pump();}
  }
  if(input&&running){int r=libusb_submit_transfer(s.t);if(r)++in_errors;else s.pending=true;}
 }
 void allocate(){
  capture.resize(cap_frames*12);sent_onsets.reserve(2048);received_onsets.reserve(2048);
  for(int i=0;i<16;++i)for(int dir=0;dir<2;++dir){auto&s=dir?out[i]:in[i];s.host=this;s.t=libusb_alloc_transfer(0);
   if(!s.t){std::fprintf(stderr,"transfer allocation failed\n");std::exit(2);}
   libusb_fill_interrupt_transfer(s.t,dev,dir?0x03:0x83,s.bytes.data(),dir?88:368,callback,&s,dir?1000:0);
  }
 }
 bool pending(){for(auto&s:in)if(s.pending)return true;for(auto&s:out)if(s.pending)return true;return false;}
 void stop(){running=false;for(auto&s:in)if(s.pending)libusb_cancel_transfer(s.t);for(auto&s:out)if(s.pending)libusb_cancel_transfer(s.t);
  double end=now()+5;while(pending()&&now()<end){timeval tv{0,10000};libusb_handle_events_timeout(ctx,&tv);}
 }
};
int main(int argc,char**argv){
 double seconds=5;const char*json=nullptr,*raw=nullptr;Host h;
 for(int i=1;i<argc;++i){
  if(!std::strcmp(argv[i],"--seconds")&&i+1<argc)seconds=std::atof(argv[++i]);
  else if(!std::strcmp(argv[i],"--json")&&i+1<argc)json=argv[++i];
  else if(!std::strcmp(argv[i],"--capture")&&i+1<argc)raw=argv[++i];
  else if(!std::strcmp(argv[i],"--pulse")){h.pulse=true;h.tone=true;}
  else if(!std::strcmp(argv[i],"--tone")&&i+1<argc){h.tone=true;h.out_channel=std::atoi(argv[++i]);}
  else{std::fprintf(stderr,"usage: %s --seconds N [--tone 0|1] [--pulse] [--json FILE] [--capture FILE]\n",argv[0]);return 2;}
 }
 if(seconds<=0||seconds>600||h.out_channel<0||h.out_channel>1)return 2;
 int r=libusb_init(&h.ctx);if(r){std::fprintf(stderr,"init: %s\n",libusb_error_name(r));return 1;}
 libusb_device**list;ssize_t n=libusb_get_device_list(h.ctx,&list);
 if(n<0){std::fprintf(stderr,"device list: %s\n",libusb_error_name((int)n));libusb_exit(h.ctx);return 1;}
 libusb_device*device=nullptr;
 for(ssize_t i=0;i<n;++i){libusb_device_descriptor d{};libusb_get_device_descriptor(list[i],&d);if(d.idVendor==0x1935&&d.idProduct==0x0b2c){device=list[i];break;}}
 if(!device){std::fprintf(stderr,"Digitakt 1935:0b2c not found\n");libusb_free_device_list(list,1);libusb_exit(h.ctx);return 1;}
 r=libusb_open(device,&h.dev);if(r){std::fprintf(stderr,"open: %s\n",libusb_error_name(r));libusb_free_device_list(list,1);libusb_exit(h.ctx);return 1;}
 libusb_config_descriptor*cfg=nullptr;r=libusb_get_active_config_descriptor(device,&cfg);
 bool layout=false;
 if(!r){bool in=false,out=false;for(int i=0;i<cfg->bNumInterfaces;++i)for(int j=0;j<cfg->interface[i].num_altsetting;++j){auto&a=cfg->interface[i].altsetting[j];
  if(a.bAlternateSetting!=3){continue;}
  for(int k=0;k<a.bNumEndpoints;++k){auto&e=a.endpoint[k];
   if(a.bInterfaceNumber==1&&e.bEndpointAddress==0x83&&e.wMaxPacketSize==368&&(e.bmAttributes&3)==3)in=true;
   if(a.bInterfaceNumber==2&&e.bEndpointAddress==0x03&&e.wMaxPacketSize==88&&(e.bmAttributes&3)==3)out=true;
  }}layout=in&&out;libusb_free_config_descriptor(cfg);}
 libusb_free_device_list(list,1);
 if(!layout){std::fprintf(stderr,"active descriptor does not match R02 368/88 alt3\n");libusb_close(h.dev);libusb_exit(h.ctx);return 1;}
 int claimed=0;for(int iface:{1,2}){r=libusb_claim_interface(h.dev,iface);std::fprintf(stderr,"claim %d: %s\n",iface,libusb_error_name(r));if(r)break;++claimed;}
 if(claimed==2)for(int iface:{1,2}){r=libusb_set_interface_alt_setting(h.dev,iface,3);std::fprintf(stderr,"alt3 %d: %s\n",iface,libusb_error_name(r));if(r)break;}
 if(claimed!=2||r){for(int i=1;i<=claimed;++i){libusb_set_interface_alt_setting(h.dev,i,0);libusb_release_interface(h.dev,i);}libusb_close(h.dev);libusb_exit(h.ctx);return 1;}
 h.allocate();h.start=now();std::signal(SIGINT,[](int){interrupted=1;});std::signal(SIGTERM,[](int){interrupted=1;});
 for(auto&s:h.in){r=libusb_submit_transfer(s.t);if(r)++h.in_errors;else s.pending=true;}
 for(int i=0;i<4;++i)h.send();
 int printed=0;while(!interrupted&&now()-h.start<seconds){timeval tv{0,10000};r=libusb_handle_events_timeout(h.ctx,&tv);if(r&&r!=LIBUSB_ERROR_INTERRUPTED){++h.in_errors;break;}
  int t=(int)(now()-h.start);if(t>=printed+10){printed=t;std::fprintf(stderr,"%ds IN %llu OUT %llu gaps %llu errors %llu/%llu\n",t,(unsigned long long)h.incoming,(unsigned long long)h.outgoing,(unsigned long long)h.gaps,(unsigned long long)h.in_errors,(unsigned long long)h.out_errors);}
  if(h.bad>10||h.in_errors+h.out_errors>100)break;
 }
 double elapsed=now()-h.start;
 double rate=h.incoming>1?(h.incoming-1)*7/(h.latest-h.first):0;
 h.stop();bool drained=!h.pending();
 bool pass=!interrupted&&elapsed>=seconds&&h.incoming>0&&!h.gaps&&!h.bad&&!h.shorts&&!h.in_errors&&!h.out_errors&&!h.out_busy&&drained;
 if(h.pulse){pass=pass&&!h.sent_onsets.empty()&&h.sent_onsets.size()==h.received_onsets.size();}
 FILE*f=json?std::fopen(json,"w"):stdout;if(!f){std::perror("json");pass=false;f=stdout;}
 std::fprintf(f,"{\n  \"platform\": \"libusb diagnostic\", \"hardware_test\": true, \"pass\": %s,\n  \"seconds\": %.6f, \"tone_channel\": %d, \"tone_dbfs\": -36,\n  \"in_packet_bytes\": 368, \"out_packet_bytes\": 88,\n  \"in_blocks\": %llu, \"out_blocks\": %llu, \"counter_gaps\": %llu, \"lost_blocks\": %llu,\n  \"bad_headers\": %llu, \"short_packets\": %llu, \"in_errors\": %llu, \"out_errors\": %llu, \"out_pool_exhausted\": %llu,\n  \"measured_frame_rate_hz\": %.6f, \"max_host_callback_gap_ms\": %.6f, \"transfers_drained\": %s,\n  \"first_header\": \"",
 pass?"true":"false",elapsed,h.tone?h.out_channel:-1,(unsigned long long)h.incoming,(unsigned long long)h.outgoing,(unsigned long long)h.gaps,(unsigned long long)h.lost,(unsigned long long)h.bad,(unsigned long long)h.shorts,(unsigned long long)h.in_errors,(unsigned long long)h.out_errors,(unsigned long long)h.out_busy,rate,h.max_callback_gap*1000,drained?"true":"false");
 for(auto v:h.header){std::fprintf(f,"%02x",v);}
 std::fprintf(f,"\",\n  \"peaks\": [");for(int ch=0;ch<12;++ch)std::fprintf(f,"%s%lld",ch?", ":"",(long long)h.peaks[ch]);std::fprintf(f,"],\n  \"captured_frames\": %zu,\n  \"pulse_sent\": %zu, \"pulse_received\": %zu,\n  \"host_observed_roundtrip_ms\": [",h.captured,h.sent_onsets.size(),h.received_onsets.size());
 for(size_t i=0;i<h.received_onsets.size();++i){std::fprintf(f,"%s%.6f",i?", ":"",1000*(h.received_onsets[i]-h.sent_onsets[i]));}
 std::fprintf(f,"]\n}\n");if(f!=stdout)std::fclose(f);
 if(raw){FILE*o=std::fopen(raw,"wb");if(o){std::fwrite(h.capture.data(),sizeof(int32_t),h.captured*12,o);std::fclose(o);}else{std::perror("capture");pass=false;}}
 if(!drained){std::fprintf(stderr,"transfers did not drain; reconnect device before retry\n");return 1;}
 for(auto&s:h.in){libusb_free_transfer(s.t);}
 for(auto&s:h.out){libusb_free_transfer(s.t);}
 for(int i:{1,2}){libusb_set_interface_alt_setting(h.dev,i,0);libusb_release_interface(h.dev,i);}libusb_close(h.dev);libusb_exit(h.ctx);return pass?0:1;
}
