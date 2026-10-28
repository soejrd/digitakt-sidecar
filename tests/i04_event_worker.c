/* Actual render/worker/native bridge/host/scheduler with modelled task wake and
 * USB completion. The native RTOS ABI/runtime is checked separately in emu. */
#include <stdlib.h>
#define I04_ENGINE_PEER
#define I04_EVENT_WORKER_TEST
#define main original_i03_main
#include "i03_integrated.c"
#undef main
int main(int argc,char **argv)
{
    event_worker=argc<2 || strcmp(argv[1],"legacy")!=0;
    worker_cadence=argc>2 ? (unsigned)strtoul(argv[2],0,10) : 1;
    assert(worker_cadence>0 && worker_cadence<=3);
    sc_playout_init(&playout);
    sc_circle_init(&host,install,reply,clear,&host,USB_TRACK_LEASE);
    test_ready=test_isr=1;test_rx_slot=(uintptr_t)stock;
    test_auto_complete=1;test_tx_observer=collect;
    fw_io_mode=6;fw_io_sync=1;fw_render_machine[0]=121;
    fw_voice_note[0]=60<<16;fw_voice_vel[0]=100<<8;
    for(unsigned i=0;i<8;++i)fw_voice_params[i]=60<<8;
    if(event_worker) {
        usb_track_control_set_wakeup(wake_worker);
        usbmidi_device_set_wakeup(usb_track_worker_rx_wake);
    }
    step(1);usb_track_tick(0);pump();step(499);
    assert(usb_track_gate && playout.voice.consumed_starts==1);
    printf("first source snapshot age: %u device blocks; delay %u frames; wake mode %u\n",
           first_install_age,SC_PLAYOUT_DELAY,event_worker);fflush(stdout);
    assert(first_install_age<=2);
    const uint32_t session=device.session;
    for(unsigned n=0;n<1000;++n) {
        fw_voice_params[n%8]=(n%127)<<8;
        fw_voice_start=n%16==0;
        step(1);
        assert(device.session==session && usb_track_gate && !playout.fault);
    }
    fw_voice_start=0;step(100);
    assert(playout.voice.starts==64 && playout.voice.consumed_starts==64);
    assert(!playout.fault && device.session==session);
    /* ACK loss and fresh handshake must remain fail closed in event mode. */
    drop_acks=1;step(1500);assert(!usb_track_gate);
    drop_acks=0;step(2000);assert(usb_track_gate && device.session!=session);
    printf("event worker cadence %u: dense integer SRC, 64 STARTs, ACK loss/recovery: PASS\n",worker_cadence);
}
