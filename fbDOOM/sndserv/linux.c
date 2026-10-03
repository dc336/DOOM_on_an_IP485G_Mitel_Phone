/*
 * ShoreTel / Broadcom HALAUDIO backend for fbDOOM sndserver.
 *
 * Replaces the old OSS /dev/dsp backend.
 *
 * Input from fbDOOM sndserver:
 *   11025 Hz, stereo, signed 16-bit little-endian
 *
 * ShoreTel output:
 *   /dev/halaudio
 *   APM codec 1
 *   16000 Hz, mono, signed 16-bit little-endian
 *   80 samples / 5 ms frame (160 bytes)
 *
 * This file uses the same Broadcom HALAUDIO ioctl ABI that was verified
 * with shoretel_beep.c on the target phone.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "soundsrv.h"

#define HALAUDIO_MAGIC_TYPE 'A'

#define HALAUDIO_CMD_SET_POWER    0x35
#define HALAUDIO_CMD_WRITE_PARMS  0x38
#define HALAUDIO_CMD_SET_FREQ     0x3a

#define HALAUDIO_FMT_S16_LE       1
#define HALAUDIO_POWER_FULL_POWER 2

#define SHORETEL_CODEC            1
#define SHORETEL_RATE             16000
#define SHORETEL_FRAME_SAMPLES    80

struct halaudio_ioctl_rw_parms
{
    int cid;
    int format;
};

struct halaudio_ioctl_setfreq
{
    int cid;
    int freqhz;
};

#define HALAUDIO_IOCTL_SET_POWER \
    _IO(HALAUDIO_MAGIC_TYPE, HALAUDIO_CMD_SET_POWER)

#define HALAUDIO_IOCTL_WRITE_PARMS \
    _IOR(HALAUDIO_MAGIC_TYPE, HALAUDIO_CMD_WRITE_PARMS, \
         struct halaudio_ioctl_rw_parms)

#define HALAUDIO_IOCTL_SET_FREQ \
    _IOR(HALAUDIO_MAGIC_TYPE, HALAUDIO_CMD_SET_FREQ, \
         struct halaudio_ioctl_setfreq)

int audio_fd = -1;

static int source_rate = 11025;

/*
 * Streaming nearest-neighbour resampler.
 *
 * For every source frame, add the destination rate. Whenever the
 * accumulator crosses the source rate, emit one destination sample.
 * Keeping this accumulator across calls prevents long-term timing drift.
 */
static unsigned int resample_accum = 0;

/* HALAUDIO wants 5 ms / 80-sample chunks at 16 kHz. */
static int16_t out_frame[SHORETEL_FRAME_SAMPLES];
static int out_used = 0;

static int write_all(int fd, const void *buf, size_t count)
{
    const unsigned char *p = (const unsigned char *) buf;

    while (count > 0)
    {
        ssize_t n = write(fd, p, count);

        if (n < 0)
        {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (n == 0)
            return -1;

        p += n;
        count -= (size_t) n;
    }

    return 0;
}

static void submit_phone_frame(void)
{
    if (out_used != SHORETEL_FRAME_SAMPLES)
        return;

    if (write_all(audio_fd, out_frame, sizeof(out_frame)) < 0)
    {
        fprintf(stderr, "halaudio write failed: %s\n", strerror(errno));
        return;
    }

    /*
     * Pace sndserver at the actual hardware period.
     * The old OSS backend was paced by blocking /dev/dsp writes.
     */
   // usleep(5000);

    out_used = 0;
}

static void emit_sample(int16_t sample)
{
    out_frame[out_used++] = sample;

    if (out_used == SHORETEL_FRAME_SAMPLES)
        submit_phone_frame();
}

void I_InitMusic(void)
{
}

void I_InitSound(int samplerate, int samplesize)
{
    struct halaudio_ioctl_rw_parms wp;
    struct halaudio_ioctl_setfreq sf;

    (void) samplesize;

    source_rate = samplerate > 0 ? samplerate : 11025;
    resample_accum = 0;
    out_used = 0;

    audio_fd = open("/dev/halaudio", O_RDWR);

    if (audio_fd < 0)
    {
        fprintf(stderr, "Could not open /dev/halaudio: %s\n",
                strerror(errno));
        exit(1);
    }

    if (ioctl(audio_fd,
              HALAUDIO_IOCTL_SET_POWER,
              HALAUDIO_POWER_FULL_POWER) < 0)
    {
        fprintf(stderr, "HALAUDIO SET_POWER failed: %s\n",
                strerror(errno));
        /* Not fatal: the phone may already be powered. */
    }

    sf.cid = SHORETEL_CODEC;
    sf.freqhz = SHORETEL_RATE;

    if (ioctl(audio_fd, HALAUDIO_IOCTL_SET_FREQ, &sf) < 0)
    {
        fprintf(stderr, "HALAUDIO SET_FREQ failed: %s\n",
                strerror(errno));
        close(audio_fd);
        audio_fd = -1;
        exit(1);
    }

    wp.cid = SHORETEL_CODEC;
    wp.format = HALAUDIO_FMT_S16_LE;

    if (ioctl(audio_fd, HALAUDIO_IOCTL_WRITE_PARMS, &wp) < 0)
    {
        fprintf(stderr, "HALAUDIO WRITE_PARMS failed: %s\n",
                strerror(errno));
        close(audio_fd);
        audio_fd = -1;
        exit(1);
    }

    fprintf(stderr,
            "ShoreTel audio: codec=%d input=%dHz stereo -> "
            "%dHz mono S16_LE\n",
            SHORETEL_CODEC, source_rate, SHORETEL_RATE);
}

void I_SubmitOutputBuffer(void *samples, int samplecount)
{
    int16_t *in = (int16_t *) samples;
    int i;

    if (audio_fd < 0 || source_rate <= 0)
        return;

    /*
     * fbDOOM's sndserver mixbuffer is interleaved stereo:
     *   L0 R0 L1 R1 ...
     *
     * Convert to mono and attenuate another 6 dB for a conservative
     * first-pass speaker level:
     *
     *   normal mono average = (L + R) / 2
     *   here               = (L + R) / 4
     */
    for (i = 0; i < samplecount; ++i)
    {
        int32_t l = in[i * 2];
        int32_t r = in[i * 2 + 1];
        int16_t mono = (int16_t) ((l + r) / 10);

        resample_accum += SHORETEL_RATE;

        while (resample_accum >= (unsigned int) source_rate)
        {
            emit_sample(mono);
            resample_accum -= (unsigned int) source_rate;
        }
    }
}

void I_ShutdownSound(void)
{
    if (audio_fd >= 0)
    {
        /*
         * Flush a final partial 5 ms frame with silence.
         */
        if (out_used > 0)
        {
            while (out_used < SHORETEL_FRAME_SAMPLES)
                out_frame[out_used++] = 0;

            submit_phone_frame();
        }

        close(audio_fd);
        audio_fd = -1;
    }
}

void I_ShutdownMusic(void)
{
}
