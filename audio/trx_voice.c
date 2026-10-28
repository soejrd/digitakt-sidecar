/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "trx_voice.h"
#include <string.h>

void sc_trx_clear(sc_trx_voice *v)
{
    memset(v, 0, sizeof *v);
    mdkit_voice_trx_b2.init(&v->engine);
    sc_src_init(&v->converter);
}
void sc_trx_init(sc_trx_voice *v) { sc_trx_clear(v); }
int sc_trx_install(sc_trx_voice *v, const struct sc_frame *f)
{
    unsigned i;
    if (!f->voice || ((f->flags & SC_START) && v->engine.pending_trigger)) return 0;
    for (i = 0; i < 8; ++i) {
        int x = f->params[i] / 256;
        if (x < 0) x = 0;
        if (x > 127) x = 127;
        mdkit_voice_trx_b2.set_control(&v->engine, i, (unsigned)x);
    }
    if (f->flags & SC_START) {
        mdkit_voice_trx_b2.trigger(&v->engine);
        ++v->starts;
    }
    return 1;
}
void sc_trx_render(sc_trx_voice *v, int16_t (*out)[8], unsigned frames)
{
    unsigned i;
    for (i = 0; i < frames; ++i) {
        float y = 0;
        /* At most one native sample is needed per output sample. */
        if (!sc_src_read(&v->converter, &y)) {
            float x;
            const unsigned pending=v->engine.pending_trigger;
            mdkit_voice_trx_b2.render(&v->engine, &x, 1);
            if (pending && !v->engine.pending_trigger) ++v->consumed_starts;
            ++v->native_frames;
            sc_src_push(&v->converter, x);
            sc_src_read(&v->converter, &y);
        }
        /* Prototype ceiling -18 dBFS, including converter overshoot. AMP owns
         * velocity; applying it here would scale a second time downstream. */
        y *= 0.125f;
        if (y > 0.125f) y = 0.125f;
        if (y < -0.125f) y = -0.125f;
        memset(out[i], 0, sizeof out[i]);
        out[i][0] = (int16_t)(y * 32767.0f);
        ++v->frames;
    }
}
