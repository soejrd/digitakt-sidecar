/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "return_lanes.h"
int sc_return_lanes(int32_t tracks[256], volatile int32_t halves[128],
                    unsigned selector, unsigned ready, const int8_t map[2])
{
    if (map[0]<-1 || map[0]>7 || map[1]<-1 || map[1]>7 ||
        (map[0]>=0 && map[0]==map[1])) {
        for (unsigned i=0;i<128;++i) halves[i]=0;
        return 0;
    }
    ready=ready && selector<=1;
    for (unsigned lane=0;lane<2;++lane) if (map[lane]>=0)
        for (unsigned i=0;i<32;++i)
            tracks[(unsigned)map[lane]*32u+i]=ready ? halves[selector*64u+2u*i+lane] : 0;
    /* Preserve BOTH lanes before clearing the consumed half once. */
    if (ready) for (unsigned i=0;i<64;++i) halves[selector*64u+i]=0;
    else for (unsigned i=0;i<128;++i) halves[i]=0;
    return ready!=0;
}
