/* SPDX-License-Identifier: CC0-1.0 */
#include "src_44148.h"
#include <math.h>
#include <string.h>
#define PI 3.14159265358979323846
static float coeff[SC_SRC_UP][SC_SRC_TAPS];
static int ready;
void sc_src_init(sc_src *s)
{
    memset(s, 0, sizeof *s);
    if (ready) return;
    for (unsigned p=0; p<SC_SRC_UP; ++p) {
        double sum=0;
        for (unsigned k=0; k<SC_SRC_TAPS; ++k) {
            const double d=k+(double)p/SC_SRC_UP-63.5;
            const double fc=20000.0/44100.0;
            const double w=fabs(d)>64 ? 0 :
                0.42+0.5*cos(PI*d/64)+0.08*cos(2*PI*d/64);
            const double x=2*fc*d;
            const double h=2*fc*(fabs(x)<1e-12 ? 1 : sin(PI*x)/(PI*x))*w;
            coeff[p][k]=(float)h; sum+=h;
        }
        for (unsigned k=0; k<SC_SRC_TAPS; ++k) coeff[p][k]/=(float)sum;
    }
    ready=1;
}
int sc_src_push(sc_src *s, float sample)
{
    const uint64_t base=s->produced*SC_SRC_DOWN/SC_SRC_UP;
    if (s->consumed>base) return 0; /* must drain, never overwrite unread data */
    s->history[s->consumed++ % SC_SRC_TAPS]=sample;
    return 1;
}
int sc_src_read(sc_src *s, float *sample)
{
    const uint64_t t=s->produced*SC_SRC_DOWN;
    const uint64_t base=t/SC_SRC_UP;
    const unsigned phase=(unsigned)(t%SC_SRC_UP);
    float y=0;
    if (s->consumed<=base) return 0;
    for (unsigned k=0; k<SC_SRC_TAPS; ++k) {
        if (base<k) break; /* zero initial history */
        y+=coeff[phase][k]*s->history[(base-k)%SC_SRC_TAPS];
    }
    ++s->produced; *sample=y;
    return 1;
}
