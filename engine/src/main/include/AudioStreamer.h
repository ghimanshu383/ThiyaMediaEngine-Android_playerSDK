//
// Created by ghima on 20-02-2026.
//

#ifndef OSFEATURENDKDEMO_AUDIOSTREAMER_H
#define OSFEATURENDKDEMO_AUDIOSTREAMER_H

#include "aaudio/AAudio.h"

namespace ve {
    class AudioStreamer {
        AAudioStreamBuilder *m_builder;
        AAudioStream *m_stream;
        int32_t m_sample_rate;
        int32_t m_channel_rate;
        bool is_streamer_ready = false;
        bool is_first_render =  true;
        int64_t audio_offset = -1;
        void init(int32_t pcmEncoding);

    public:
        AudioStreamer(int32_t sampleRate, int32_t channelRate, int32_t pcmEncoding);

        void write_pcm_to_stream(uint8_t *pcm, size_t outSize);

        int64_t get_audio_clock();
        int64_t get_audio_clock_frame_display_time(int64_t vidPtsNano);

        int64_t get_audio_start_offset();
        void shut_down();
    };
}

#endif //OSFEATURENDKDEMO_AUDIOSTREAMER_H
