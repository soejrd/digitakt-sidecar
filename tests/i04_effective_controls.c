/* Production sender capture: suppress only changes that the real integer
 * engine API cannot distinguish. All START/note/velocity changes stay ordered. */
#define main original_i03_main
#include "i03_integrated.c"
#undef main
int main(void)
{
    fw_io_mode=6; fw_io_sync=1; fw_render_machine[0]=121;
    fw_voice_note[0]=60<<16; fw_voice_vel[0]=100<<8;
    for(unsigned i=0;i<8;++i) fw_voice_params[i]=60<<8;
    assert(usb_track_control_open(100));
    for(unsigned n=0;n<128;++n) {
        fw_voice_params[0]=36*256-(int)n*16;
        usb_track_render(blocks);
    }
    struct ut_record r; unsigned count=0;
    while(usb_track_control_peek(&r)) { ++count; assert(usb_track_control_pop(&r)); }
    printf("128 fractional render updates -> %u effective-control records (expected 9)\n",count);
    assert(count==9);
    usb_track_control_close();
    assert(usb_track_control_open(101));
    fw_voice_start=1; usb_track_render(blocks); usb_track_render(blocks); fw_voice_start=0;
    count=0;
    while(usb_track_control_peek(&r)) {
        assert(r.flags&UT_START); ++count; assert(usb_track_control_pop(&r));
    }
    assert(count==2);
    fw_voice_params[0]=-1; fw_voice_params[1]=-300; fw_voice_params[2]=32767;
    fw_voice_note[0]=61<<16; fw_voice_vel[0]=90<<8; usb_track_render(blocks);
    assert(usb_track_control_pop(&r));
    assert(r.params[0]==0 && r.params[1]==0 && r.params[2]==127*256);
    assert(r.note==(61<<16) && r.velocity==(90<<8) && !(r.flags&UT_START));
    puts("I04 effective sender controls: integer normalization/clamp, fractional suppression, every adjacent START, note/velocity preservation: PASS");
    return 0;
}
