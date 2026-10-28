#include "src_44148.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#define PI 3.14159265358979323846
static sc_src s;
static double image_db;
static double tone(double hz) {
    sc_src_init(&s);
    double e=0, re=0, im=0;
    double image=44100-hz; if(image>24000) image=48000-image;
    unsigned count=0;
    for(unsigned n=0;n<50048;++n) {
        float y;
        if(!sc_src_read(&s,&y)) {
            const float x=(float)(0.1*sin(2*PI*hz*(double)s.consumed/44100));
            assert(sc_src_push(&s,x)); assert(sc_src_read(&s,&y));
        }
        assert(isfinite(y));
        if(n>=2048) {
            e+=(double)y*y; ++count;
            re+=y*cos(2*PI*image*n/48000); im+=y*sin(2*PI*image*n/48000);
        }
    }
    image_db=20*log10(2*sqrt(re*re+im*im)/(count*0.1));
    return sqrt(e/count);
}
int main(int argc,char **argv) {
    const double ref=tone(1000);
    const double pass=tone(18000);
    const double stop=tone(22000);
    const double db=20*log10(stop/ref);
    assert(fabs(ref/(0.1/sqrt(2))-1)<0.001);
    assert(fabs(pass/ref-1)<0.01);
    assert(db < -60);
    double worst_pass=0, worst_stop=-1000, worst_image=-1000;
    for(unsigned hz=1000;hz<=18000;hz+=1000) {
        double gain=20*log10(tone(hz)/ref);
        if(fabs(gain)>worst_pass) worst_pass=fabs(gain);
        if(image_db>worst_image) worst_image=image_db;
    }
    for(unsigned hz=22000;hz<=22050;hz+=10) {
        double gain=20*log10(tone(hz)/ref);
        if(gain>worst_stop) worst_stop=gain;
    }
    assert(worst_pass<0.1 && worst_stop<-60 && worst_image<-60);
    // A constant stream must preserve DC across every fractional phase.
    sc_src_init(&s);
    for(unsigned n=0;n<48000;++n) {
        float y;
        if(!sc_src_read(&s,&y)) { assert(sc_src_push(&s,0.25f)); assert(sc_src_read(&s,&y)); }
        if(n>256) assert(fabs(y-0.25f)<1e-6);
    }
    assert(s.consumed==44100 && s.produced==48000);
    // Causal impulse peak establishes the converter's own delay only.
    sc_src_init(&s); double peak=0; unsigned at=0;
    for(unsigned n=0;n<512;++n) {
        float y;
        if(!sc_src_read(&s,&y)) {
            assert(sc_src_push(&s,s.consumed==64 ? 1.0f : 0.0f));
            assert(sc_src_read(&s,&y));
        }
        if(fabs(y)>peak) { peak=fabs(y); at=n; }
    }
    assert(at>=138 && at<=140);
    FILE *out=argc>1 ? fopen(argv[1],"w") : stdout; assert(out);
    fprintf(out,"{\n  \"scope\": \"host converter only, no USB or hardware\",\n  \"taps\": 128,\n  \"cutoff_hz\": 20000,\n  \"reference_1khz_rms\": %.9g,\n  \"passband_18khz_db\": %.6f,\n  \"stopband_22khz_db\": %.6f,\n  \"worst_passband_deviation_db\": %.6f,\n  \"worst_22khz_to_nyquist_db\": %.6f,\n  \"worst_first_image_db\": %.6f,\n  \"input_impulse_frame\": 64,\n  \"output_impulse_peak_frame\": %u,\n  \"group_delay_native_frames\": 63.5,\n  \"result\": \"PASS\"\n}\n",ref,20*log10(pass/ref),db,worst_pass,worst_stop,worst_image,at);
    if(out!=stdout) fclose(out);
    puts("I04 filter: 18 kHz passband, >60 dB at 22 kHz, DC/phase/count and causal impulse delay: PASS");
}
