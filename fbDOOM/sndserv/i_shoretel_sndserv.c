/*
 * i_shoretel_sndserv.c
 *
 * fbDOOM sound_module_t wrapper for the classic DOOM sndserver.
 *
 * The actual hardware backend remains in sndserv/linux.c, where we already
 * verified ShoreTel Broadcom HALAUDIO codec 1 at 16 kHz.
 *
 * This module makes the newer fbDOOM/Chocolate-Doom-style sound interface
 * actually launch sndserver and send classic 'p' commands to it.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "doomtype.h"
#include "i_sound.h"
#include "i_timer.h"
#include "sounds.h"
#include "w_wad.h"
#include "z_zone.h"

#define NUM_DOOM_CHANNELS 8

static FILE *sndserver_pipe = NULL;
static boolean use_prefix = true;
static int channel_end_tic[NUM_DOOM_CHANNELS];

static snddevice_t shoretel_devices[] =
{
    SNDDEVICE_SB
};

static int SfxId(sfxinfo_t *sfx)
{
    return (int) (sfx - S_sfx);
}

static int ShoreTel_GetSfxLumpNum(sfxinfo_t *sfx)
{
    char namebuf[9];

    if (sfx->link != NULL)
    {
        sfx = sfx->link;
    }

    if (use_prefix)
    {
        /*
         * WAD lump names are at most 8 chars. Doom SFX names fit in
         * "DS" + six characters.
         */
        snprintf(namebuf, sizeof(namebuf), "ds%.6s", sfx->name);
    }
    else
    {
        snprintf(namebuf, sizeof(namebuf), "%.8s", sfx->name);
    }

    return W_GetNumForName(namebuf);
}

static int SoundDurationTics(sfxinfo_t *sfx)
{
    unsigned char *data;
    unsigned int lumplen;
    unsigned int samplerate;
    unsigned int length;
    unsigned long long tics;
    int lumpnum;

    lumpnum = sfx->lumpnum;

    if (lumpnum < 0)
    {
        return TICRATE;
    }

    lumplen = W_LumpLength((unsigned int) lumpnum);

    if (lumplen < 8)
    {
        return TICRATE;
    }

    data = (unsigned char *) W_CacheLumpNum(lumpnum, PU_STATIC);

    samplerate = (unsigned int) data[2]
               | ((unsigned int) data[3] << 8);

    length = (unsigned int) data[4]
           | ((unsigned int) data[5] << 8)
           | ((unsigned int) data[6] << 16)
           | ((unsigned int) data[7] << 24);

    W_ReleaseLumpNum(lumpnum);

    if (samplerate == 0 || length == 0)
    {
        return TICRATE;
    }

    tics = ((unsigned long long) length * TICRATE + samplerate - 1)
         / samplerate;

    if (tics < 1)
    {
        tics = 1;
    }

    /*
     * Keep pathological/custom lumps from pinning a logical channel forever.
     */
    if (tics > TICRATE * 30)
    {
        tics = TICRATE * 30;
    }

    return (int) tics;
}

static boolean ShoreTel_Init(boolean use_sfx_prefix)
{
    int i;
    const char *command;

    use_prefix = use_sfx_prefix;

    for (i = 0; i < NUM_DOOM_CHANNELS; ++i)
    {
        channel_end_tic[i] = 0;
    }

    if (access("/data/fbdoom/sndserver", X_OK) == 0)
    {
        command =
            "DOOMWADDIR=/data/fbdoom "
            "/data/fbdoom/sndserver -quiet";
    }
    else if (access("./sndserver", X_OK) == 0)
    {
        command = "DOOMWADDIR=. ./sndserver -quiet";
    }
    else
    {
        fprintf(stderr,
                "ShoreTel sound: sndserver executable not found\n");
        return false;
    }

    sndserver_pipe = popen(command, "w");

    if (sndserver_pipe == NULL)
    {
        fprintf(stderr, "ShoreTel sound: popen(sndserver) failed\n");
        return false;
    }

    /*
     * Every sound command should reach the child immediately.
     */
    setvbuf(sndserver_pipe, NULL, _IONBF, 0);

    fprintf(stderr, "ShoreTel sound: sndserver started\n");

    return true;
}

static void ShoreTel_Shutdown(void)
{
    if (sndserver_pipe != NULL)
    {
        fputs("q\n", sndserver_pipe);
        fflush(sndserver_pipe);
        pclose(sndserver_pipe);
        sndserver_pipe = NULL;
    }
}

static void ShoreTel_Update(void)
{
    /* sndserver mixes continuously in its own process. */
}

static void ShoreTel_UpdateSoundParams(int channel, int vol, int sep)
{
    /*
     * The original sndserver protocol has no "update existing channel"
     * command. New sounds get the correct volume/separation at StartSound.
     */
    (void) channel;
    (void) vol;
    (void) sep;
}

static int ShoreTel_StartSound(sfxinfo_t *sfx,
                               int channel,
                               int vol,
                               int sep)
{
    int id;

    if (sndserver_pipe == NULL)
    {
        return -1;
    }

    if (channel < 0 || channel >= NUM_DOOM_CHANNELS)
    {
        return -1;
    }

    id = SfxId(sfx);

    if (id <= 0 || id > 255)
    {
        return -1;
    }

    /*
     * Classic sndserver protocol:
     *
     *   p <sfx id> <pitch> <volume> <separation>\n
     *
     * All fields are two hexadecimal digits.
     * 0x80 is normal pitch.
     */
    fprintf(sndserver_pipe, "p%02x%02x%02x%02x\n",
            id & 0xff,
            0x80,
            vol & 0xff,
            sep & 0xff);

    fflush(sndserver_pipe);

    channel_end_tic[channel] = I_GetTime() + SoundDurationTics(sfx);

    return channel;
}

static void ShoreTel_StopSound(int channel)
{
    /*
     * The classic server has no stop-one-channel command.
     * Mark it stopped from fbDOOM's point of view; the short SFX finishes
     * naturally in sndserver.
     */
    if (channel >= 0 && channel < NUM_DOOM_CHANNELS)
    {
        channel_end_tic[channel] = 0;
    }
}

static boolean ShoreTel_SoundIsPlaying(int channel)
{
    if (channel < 0 || channel >= NUM_DOOM_CHANNELS)
    {
        return false;
    }

    return channel_end_tic[channel] != 0
        && I_GetTime() < channel_end_tic[channel];
}

static void ShoreTel_CacheSounds(sfxinfo_t *sounds, int num_sounds)
{
    /*
     * No-op: the external sndserver loads all SFX from the IWAD itself.
     */
    (void) sounds;
    (void) num_sounds;
}

sound_module_t sound_shoretel_sndserv_module =
{
    shoretel_devices,
    sizeof(shoretel_devices) / sizeof(shoretel_devices[0]),
    ShoreTel_Init,
    ShoreTel_Shutdown,
    ShoreTel_GetSfxLumpNum,
    ShoreTel_Update,
    ShoreTel_UpdateSoundParams,
    ShoreTel_StartSound,
    ShoreTel_StopSound,
    ShoreTel_SoundIsPlaying,
    ShoreTel_CacheSounds
};
