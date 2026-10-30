/* Actual one-voice render/worker/native adapter; stalled TX with valid RX. */
#define I04_EVENT_WORKER_TEST
#define main original_integrated_main
#include "i03_integrated.c"
#undef main

int main(int argc,char **argv)
{
    if(argc>1 && strcmp(argv[1],"wrap")==0) usb_track_clock=UINT32_MAX-700u;
    sc_circle_init(&host,install,reply,clear,&host,USB_TRACK_LEASE);
    test_ready=test_isr=test_auto_complete=1;
    test_rx_slot=(uintptr_t)stock; test_tx_observer=collect;
    fw_io_mode=6; fw_io_sync=1; fw_render_machine[0]=121;
    fw_voice_note[0]=60<<16; fw_voice_vel[0]=100<<8;
    for(unsigned k=0;k<8;++k) fw_voice_params[k]=60<<8;
    usb_track_control_set_wakeup(wake_worker);
    usbmidi_device_set_wakeup(usb_track_worker_rx_wake);
    step(1); usb_track_tick(0); pump(); step(499);
    assert(usb_track_gate && device.state==SC_DEVICE_ACTIVE);
    const uint32_t session=device.session, began=usb_track_clock;
    const uint32_t accepted=device.accepted_sequence, block=device.accepted_block;
    const unsigned submitted=native_calls;
    test_auto_complete=0; test_queued=0xffb-1;
    unsigned keepalives=0;
    while(usb_track_clock-began<USB_TRACK_LEASE+128u) {
        if(device.session==session && running && device.state==SC_DEVICE_ACTIVE &&
           (usb_track_clock-began)%32u==0) {
            struct sc_control c={session,accepted,block,SC_KEEPALIVE};
            uint8_t wire[SC_CONTROL_WIRE_SIZE]; assert(sc_encode_control(wire,&c));
            ((void (*)(const uint8_t *,uint32_t))test_rx_slot)(wire,sizeof wire);
            ++keepalives;
        }
        step(1);
        if(!running || device.session!=session) break;
    }
    const unsigned elapsed=usb_track_clock-began;
    assert(keepalives>10 && native_calls==submitted);
    if(running && device.session==session) {
        fprintf(stderr,"FAIL: stalled TX still active after %u blocks despite %u valid keepalives; gate=%u queued=%u\n",
                elapsed,keepalives,(unsigned)usb_track_gate,usbmidi_device_diag(4));
        return 1;
    }
    assert(!usb_track_gate && elapsed<=USB_TRACK_LEASE+64u);
    test_queued=0; test_auto_complete=1; step(1000);
    assert(usb_track_gate && device.session!=session);
    printf("V01 stalled TX: retired after %u blocks despite %u valid keepalives; fresh-session recovery: PASS\n",elapsed,keepalives);
}
