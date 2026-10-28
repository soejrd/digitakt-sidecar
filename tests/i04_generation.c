/* Real firmware render/queue/worker/native transport -> portable host ->
 * actual scheduled TRX-B2. MMIO/USB are modelled by the existing harness. */
#include <stdlib.h>
#define I04_ENGINE_PEER
#define main original_i03_main
#include "i03_integrated.c"
#undef main
int main(int argc,char **argv)
{
    tick_period=argc>1 ? (unsigned)strtoul(argv[1],0,10) : 50;
    assert(tick_period>0);
    sc_playout_init(&playout);
    sc_circle_init(&host,install,reply,clear,&host,USB_TRACK_LEASE);
    test_ready=test_isr=1; test_rx_slot=0;
    test_auto_complete=1; test_tx_observer=collect;
    fw_io_mode=6; fw_io_sync=1; fw_render_machine[0]=121;
    fw_voice_note[0]=60<<16; fw_voice_vel[0]=100<<8;
    const unsigned defaults[]={60,65,41,62,91,44,56,58};
    for(unsigned i=0;i<8;++i) fw_voice_params[i]=defaults[i]<<8;
    step(1); usb_track_tick(0); test_rx_slot=(uintptr_t)stock; step(499);
    assert(usb_track_gate && installed.voice==1 && playout.voice.consumed_starts==1);
    const uint32_t session=device.session;
    for(unsigned hit=2;hit<=4;++hit) {
        fw_voice_start=1; step(1); fw_voice_start=0; step(199);
        printf("native hit %u: session retained %u, installed generation %u, consumed %llu\n",hit,device.session==session,installed.voice,(unsigned long long)playout.voice.consumed_starts);
        assert(device.session==session && usb_track_gate && sc_circle_audio_allowed(&host));
        assert(installed.voice==hit && playout.voice.consumed_starts==hit);
    }
    tick_period=50;
    const uint64_t applied_before=playout.applied;
    for(unsigned n=0;n<1000;++n) {
        fw_voice_params[0]=(n%127)<<8; step(1);
        assert(device.session==session && usb_track_gate && sc_circle_audio_allowed(&host));
    }
    step(500);
    assert(device.session==session && usb_track_gate && sc_circle_audio_allowed(&host));
    assert(playout.applied>=applied_before+1000);
    printf("dense SRC retained: applied %llu, queue peak %u, delay %u frames\n",
           (unsigned long long)(playout.applied-applied_before),playout.peak,SC_PLAYOUT_DELAY);
    puts("I04 actual firmware worker -> scheduled engine retains generations 1..4 and consumes each START: PASS");
}
