/// @file audio.h
/// @brief audio utilities
/// @author Jeff Perry <jeffsp@gmail.com>
/// @version 1.0
/// @date 2013-09-03

#ifndef AUDIO_H
#define AUDIO_H

#include <alsa/asoundlib.h>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace soma
{

class audio
{
    private:
    snd_pcm_t *handle;
    static const int SAMPLE_RATE = 44100;
    public:
    audio ()
        : handle (0)
    {
        int err;
        if ((err = snd_pcm_open (&handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0)
            throw std::runtime_error (snd_strerror (err));
        if ((err = snd_pcm_set_params (handle,
                    SND_PCM_FORMAT_U8,
                    SND_PCM_ACCESS_RW_INTERLEAVED,
                    1, // mono
                    SAMPLE_RATE,
                    0, // soft-resample
                    250000)) < 0) // 1/4 sec latency
            throw std::runtime_error (snd_strerror (err));
        sleep (1);
    }
    ~audio ()
    {
        sleep (1);
        snd_pcm_close (handle);
    }
    /// @see http://en.wikipedia.org/wiki/Piano_key_frequencies
    void play (int freq = 262, int millisecs = 1000)
    {
        const int SAMPLES = SAMPLE_RATE * millisecs / 1000;
        std::clog << SAMPLES << std::endl;
        std::vector<uint8_t> samples (SAMPLES);
        for (int t = 0; t < SAMPLES; ++t)
            samples[t] = (sin (t * 2.0 * M_PI / SAMPLE_RATE * freq) + 1.0) * 128.0;
        snd_pcm_sframes_t frames = snd_pcm_writei (handle, &samples[0], samples.size ());
        std::clog << frames << std::endl;
        if (frames < 0)
            frames = snd_pcm_recover (handle, frames, 0);
        if (frames < 0)
            throw std::runtime_error (snd_strerror (frames));
    }
};

void play (int freq = 262, int millisecs = 1000)
{
    const int sr = 8000;
    const char *cmd = "aplay";
    const char *flags = "w";
    FILE *fp = popen (cmd, flags);
    if (!fp)
        throw std::runtime_error ("aplay failed");
    for (int t = 0; t < sr * millisecs / 1000; ++t)
    {
        uint8_t s = (sin (t * 2.0 * M_PI / sr * freq) + 1.0) * 128.0;
        fputc (s, fp);
    }
    pclose (fp);
}


}

#endif
