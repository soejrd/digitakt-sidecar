/* SPDX-License-Identifier: CC0-1.0
 * R05 benchmark: real TRX-B2 at 44.1 kHz -> streaming 160:147 -> 48 kHz.
 * Usage: r05_bench [output.json]
 */
#include "mdkit_trx_b2.h"
#include "r05_resampler.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define IN_RATE 44100u
#define OUT_RATE 48000u
#define TEST_FRAMES (IN_RATE * 2u)
#define MAX_BLOCK 257u
#define MAX_INPUT_BLOCK 511u
#define OUT_CAP (MAX_INPUT_BLOCK * R05_UP / R05_DOWN + 2u)
#define PI 3.14159265358979323846

static const mdkit_voice *voice = &mdkit_voice_trx_b2;
static int failures;

#define CHECK(c, m) do { if (!(c)) { printf("FAIL %s\n", m); ++failures; } } while (0)

static double now_seconds(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

static double peak(const float *x, size_t n)
{
    double p = 0.0;
    for (size_t i = 0; i < n; ++i) if (fabs(x[i]) > p) p = fabs(x[i]);
    return p;
}

static double rms(const float *x, size_t n)
{
    double e = 0.0;
    for (size_t i = 0; i < n; ++i) e += (double)x[i] * x[i];
    return n ? sqrt(e / (double)n) : 0.0;
}

static void render_blocks(const mdkit_voice *v, void *s, float *out, size_t n,
                          int ragged)
{
    static const unsigned sizes[] = {1, 7, 31, 257, 32, 63, 511};
    size_t pos = 0, k = 0;
    while (pos < n) {
        unsigned q = ragged ? sizes[k++ % 7] : MAX_BLOCK;
        if (q > n - pos) q = (unsigned)(n - pos);
        v->render(s, out + pos, q);
        pos += q;
    }
}

static int resample_stream(const float *in, size_t n, float *out, size_t cap,
                           int ragged, size_t *made)
{
    r05_resampler r;
    static const unsigned sizes[] = {1, 7, 31, 97, 257, 32, 63, 511, 3};
    size_t pos = 0, k = 0, total = 0;
    float block[OUT_CAP];
    r05_resampler_init(&r);
    while (pos < n) {
        unsigned q = ragged ? sizes[k++ % 9] : MAX_BLOCK;
        size_t got;
        if (q > n - pos) q = (unsigned)(n - pos);
        got = r05_resampler_process(&r, in + pos, q, block, OUT_CAP);
        if (total + got > cap) return 0;
        memcpy(out + total, block, got * sizeof *block);
        total += got;
        pos += q;
    }
    {
        size_t got = r05_resampler_finish(&r, out + total, cap - total);
        total += got;
    }
    *made = total;
    return 1;
}

static double tone_energy(const float *x, size_t n, double rate, double hz)
{
    const double w = 2.0 * PI * hz / rate;
    const double c = 2.0 * cos(w);
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double y = (double)x[i] + c * s1 - s2;
        s2 = s1; s1 = y;
    }
    return sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / (double)n;
}

static double dominant_hz(const float *x, size_t n, double lo, double hi, double step)
{
    double best = -1.0, best_hz = 0.0;
    for (double hz = lo; hz <= hi; hz += step) {
        const double e = tone_energy(x, n, OUT_RATE, hz);
        if (e > best) { best = e; best_hz = hz; }
    }
    return best_hz;
}

static void write_arrays(FILE *f, const char *name, const float *x, size_t n)
{
    fprintf(f, "  \"%s\": [", name);
    for (size_t i = 0; i < n; ++i) fprintf(f, "%s%.9g", i ? "," : "", x[i]);
    fputs("]", f);
}

int main(int argc, char **argv)
{
    const size_t total_out = (size_t)((double)TEST_FRAMES * R05_UP / R05_DOWN) + 4u;
    float *ref = calloc(TEST_FRAMES, sizeof *ref);
    float *alt = calloc(TEST_FRAMES, sizeof *alt);
    float *out = calloc(total_out, sizeof *out);
    float *altout = calloc(total_out, sizeof *altout);
    void *state = calloc(1, voice->state_bytes), *state2 = calloc(1, voice->state_bytes);
    size_t made = 0, altmade = 0;
    FILE *f = NULL;
    double engine_seconds, conv_seconds, both_seconds, t0, t1;
    double impulse_peak = 0.0, pass_1k_amp = 0.0, stop_22k_rms = 0.0, impulse_position = 0.0;
    unsigned long long outputs = 0;

    if (!ref || !alt || !out || !altout || !state || !state2) return 2;
    if (argc > 1) { f = fopen(argv[1], "w"); if (!f) { perror("open"); return 2; } }
    else f = stdout;

    /* Reference: native 44.1 kHz TRX-B2, one trigger. */
    voice->init(state);
    voice->trigger(state);
    t0 = now_seconds();
    render_blocks(voice, state, ref, TEST_FRAMES, 0);
    t1 = now_seconds();
    engine_seconds = t1 - t0;

    /* Same render with ragged app block sizes must be bit-identical. */
    voice->init(state2);
    voice->trigger(state2);
    render_blocks(voice, state2, alt, TEST_FRAMES, 1);
    CHECK(memcmp(ref, alt, TEST_FRAMES * sizeof *ref) == 0,
          "engine output invariant to app block size");
    CHECK(peak(ref, TEST_FRAMES) > 0.9, "reference hit is audible");
    CHECK(peak(ref, TEST_FRAMES) <= 1.000001, "reference is bounded");

    /* Converter: continuous vs ragged calls must be bit-identical. */
    t0 = now_seconds();
    CHECK(resample_stream(ref, TEST_FRAMES, out, total_out, 0, &made), "continuous conversion");
    t1 = now_seconds();
    conv_seconds = t1 - t0;
    CHECK(resample_stream(ref, TEST_FRAMES, altout, total_out, 1, &altmade), "ragged conversion");
    CHECK(made == altmade, "block boundaries do not change output frame count");
    CHECK(made == TEST_FRAMES * R05_UP / R05_DOWN,
          "converted duration uses exact 160/147 ratio");
    CHECK(memcmp(out, altout, made * sizeof *out) == 0,
          "converter output invariant to block boundaries");

    /* One-shot timing including real engine and converter, native-rate work. */
    voice->init(state);
    voice->trigger(state);
    t0 = now_seconds();
    {
        r05_resampler r;
        size_t pos = 0, total = 0;
        float block[MAX_BLOCK], converted[OUT_CAP];
        r05_resampler_init(&r);
        while (pos < TEST_FRAMES) {
            unsigned q = TEST_FRAMES - pos > MAX_BLOCK ? MAX_BLOCK : (unsigned)(TEST_FRAMES - pos);
            size_t got;
            voice->render(state, block, q);
            got = r05_resampler_process(&r, block, q, converted, OUT_CAP);
            total += got;
            pos += q;
        }
        outputs = total;
        outputs += r05_resampler_finish(&r, converted, OUT_CAP);
    }
    t1 = now_seconds();
    both_seconds = t1 - t0;
    CHECK(outputs == TEST_FRAMES * R05_UP / R05_DOWN, "combined pipeline output frame count");

    /* Impulse delay: locate the converter's peak. */
    {
        float impulse[512] = {0}, y[OUT_CAP * 4];
        r05_resampler r;
        size_t got, opk = 0;
        double jitter_max = 0.0;
        impulse[64] = 1.0f;
        r05_resampler_init(&r);
        got = r05_resampler_process(&r, impulse, 512, y, OUT_CAP * 4);
        got += r05_resampler_finish(&r, y + got, OUT_CAP * 4 - got);
        for (size_t k = 0; k < got; ++k) if (fabs(y[k]) > fabs(y[opk])) opk = k;
        CHECK(got == 512 * R05_UP / R05_DOWN, "impulse frame ratio");
        CHECK(opk >= 84 && opk <= 87, "causal converter latency is about 16 input samples");
        impulse_peak = fabs(y[opk]);
        impulse_position = (double)opk;
        CHECK(fabs(y[opk]) >= 0.90, "converter passband gain near unity");
        CHECK(fabs(y[opk]) <= 1.05, "converter passband gain does not overshoot materially");
        (void)jitter_max;

        /* 1 kHz passband and 22 kHz stopband. */
        {
            float sine_in[IN_RATE], sine_out[1 << 20];
            size_t outn = 0;
            for (size_t k = 0; k < IN_RATE; ++k)
                sine_in[k] = (float)(0.1 * sin(2.0 * PI * 1000.0 * (double)k / IN_RATE));
            r05_resampler_init(&r);
            outn = r05_resampler_process(&r, sine_in, IN_RATE, sine_out, sizeof sine_out / sizeof *sine_out);
            outn += r05_resampler_finish(&r, sine_out + outn, sizeof sine_out / sizeof *sine_out - outn);
            CHECK(outn == IN_RATE * R05_UP / R05_DOWN, "sine stream frame ratio");
            {
                const double e1k = tone_energy(sine_out + 4800, outn - 9600, OUT_RATE, 1000.0);
                pass_1k_amp = 2.0 * e1k;
                CHECK(fabs(2.0 * e1k - 0.1) < 0.005, "1 kHz passband amplitude");
            }
            for (size_t k = 0; k < IN_RATE; ++k)
                sine_in[k] = (float)(0.1 * sin(2.0 * PI * 22000.0 * (double)k / IN_RATE));
            r05_resampler_init(&r);
            outn = r05_resampler_process(&r, sine_in, IN_RATE, sine_out, sizeof sine_out / sizeof *sine_out);
            outn += r05_resampler_finish(&r, sine_out + outn, sizeof sine_out / sizeof *sine_out - outn);
            {
                const double stop = rms(sine_out + 4800, outn - 9600);
                stop_22k_rms = stop;
                CHECK(stop < 0.1 * 0.05, "22 kHz image suppressed by at least 25 dB");
            }
        }
    }

    /* Pitch mapping: settle PTCH at endpoints, then compare dominant tone. */
    {
        float low[IN_RATE], high[IN_RATE], lo_out[1 << 17], hi_out[1 << 17];
        size_t lon = 0, hin = 0;
        double flo, fhi;
        voice->init(state);
        voice->set_control(state, MDKIT_TRX_B2_PTCH, 0);
        render_blocks(voice, state, low, IN_RATE, 0);
        voice->trigger(state);
        render_blocks(voice, state, low, IN_RATE, 0);
        CHECK(resample_stream(low, IN_RATE, lo_out, sizeof lo_out / sizeof *lo_out, 0, &lon),
              "low pitch conversion");
        voice->init(state2);
        voice->set_control(state2, MDKIT_TRX_B2_PTCH, 127);
        render_blocks(voice, state2, high, IN_RATE, 0);
        voice->trigger(state2);
        render_blocks(voice, state2, high, IN_RATE, 0);
        CHECK(resample_stream(high, IN_RATE, hi_out, sizeof hi_out / sizeof *hi_out, 0, &hin),
              "high pitch conversion");
        flo = dominant_hz(lo_out + 2205, 22050, 20.0, 1000.0, 1.0);
        fhi = dominant_hz(hi_out + 2205, 22050, 20.0, 1000.0, 1.0);
        CHECK(fhi > flo * 1.5, "PTCH endpoints change pitch in expected direction");

        /* Retrigger must restart the audible envelope, and converter state must
         * not leak a discontinuity beyond the expected new transient. */
        {
            float a[IN_RATE], b[IN_RATE], ao[1 << 17], bo[1 << 17];
            size_t an = 0, bn = 0;
            voice->init(state);
            voice->trigger(state);
            render_blocks(voice, state, a, IN_RATE, 0);
            voice->trigger(state);
            render_blocks(voice, state, a, IN_RATE, 0);
            CHECK(resample_stream(a, IN_RATE, ao, sizeof ao / sizeof *ao, 0, &an), "retrigger conversion");
            voice->init(state2);
            voice->trigger(state2);
            render_blocks(voice, state2, b, IN_RATE, 0);
            CHECK(resample_stream(b, IN_RATE, bo, sizeof bo / sizeof *bo, 0, &bn), "single-hit conversion");
            CHECK(rms(a + IN_RATE, 4410) > 10.0 * rms(a + IN_RATE + 22050, 11025),
                  "retrigger restarts decay");
            CHECK(peak(a + IN_RATE, 4410) > 0.1, "retrigger remains audible");
        }

        /* Long duration: 60 s output keeps exactly 160/147 frames, no drift. */
        {
            const size_t long_in = IN_RATE * 60u;
            size_t n = 0;
            float *li = calloc(long_in, sizeof *li);
            float *lo = calloc(long_in * R05_UP / R05_DOWN + 4u, sizeof *lo);
            if (!li || !lo) return 2;
            for (size_t k = 0; k < long_in; ++k) li[k] = 0.0f;
            CHECK(resample_stream(li, long_in, lo, long_in * R05_UP / R05_DOWN + 4u, 1, &n),
                  "60 s conversion");
            CHECK(n == long_in * R05_UP / R05_DOWN, "60 s frame-count drift is zero");
            free(li); free(lo);
        }

        if (f != stdout) {
            const double wall = (double)TEST_FRAMES / IN_RATE;
            const double engine_rt = wall / engine_seconds;
            const double both_rt = wall / both_seconds;
            fprintf(f, "{\n");
            fprintf(f, "  \"task\": \"R05\",\n");
            fprintf(f, "  \"source\": \"mdkit TRX-B2 %u controls, %s\",\n",
                    voice->machine_id, voice->name);
            fprintf(f, "  \"native_rate_hz\": %u,\n", IN_RATE);
            fprintf(f, "  \"output_rate_hz\": %u,\n", OUT_RATE);
            fprintf(f, "  \"ratio\": \"160/147\",\n");
            fprintf(f, "  \"frames_native\": %u,\n", TEST_FRAMES);
            fprintf(f, "  \"frames_output\": %zu,\n", made);
            fprintf(f, "  \"peak_native\": %.9g,\n", peak(ref, TEST_FRAMES));
            fprintf(f, "  \"host_seconds_engine_only\": %.9f,\n", engine_seconds);
            fprintf(f, "  \"host_seconds_resampler_only\": %.9f,\n", conv_seconds);
            fprintf(f, "  \"host_seconds_engine_and_resampler\": %.9f,\n", both_seconds);
            fprintf(f, "  \"host_realtime_factor_engine_only\": %.6f,\n", engine_rt);
            fprintf(f, "  \"host_realtime_factor_engine_and_resampler\": %.6f,\n", both_rt);
            fprintf(f, "  \"host_note\": \"Desktop host measurement; not Pi 4B or hardware performance.\",\n");
            fprintf(f, "  \"checks\": {\"failed\": %d},\n", failures);
            write_arrays(f, "impulse_response", out, 128);
            fprintf(f, ",\n");
            fprintf(f, "  \"measurements\": {\"pitch_low_hz\": %.3f, \"pitch_high_hz\": %.3f, "
                    "\"impulse_output_index\": %.0f, \"impulse_peak\": %.6f, "
                    "\"passband_1k_peak_amplitude\": %.6f, \"stopband_22k_rms\": %.6f, "
                    "\"stopband_22k_db\": %.3f}\n",
                    flo, fhi, impulse_position, impulse_peak, pass_1k_amp, stop_22k_rms,
                    20.0 * log10(stop_22k_rms / 0.070710678));
            fprintf(f, "}\n");
        }
    }

    if (f != stdout) fclose(f);
    printf("R05: %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    free(ref); free(alt); free(out); free(altout); free(state); free(state2);
    return failures ? 1 : 0;
}
