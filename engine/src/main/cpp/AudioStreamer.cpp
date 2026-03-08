//
// Created by ghima on 20-02-2026.
//
#include "AudioStreamer.h"
#include "Util.h"

namespace ve {
    AudioStreamer::AudioStreamer(int32_t sampleRate, int32_t channelRate, int32_t pcmEncoding)
            : m_sample_rate{
            sampleRate}, m_channel_rate{channelRate} {
        init(pcmEncoding);
    }

    void AudioStreamer::init(int32_t pcmEncoding) {
        aaudio_result_t result = AAudio_createStreamBuilder(&m_builder);
        if (result != AAUDIO_OK) {
            LOG_ERROR("failed to create the stream builder");
            return;
        }
        aaudio_format_t aaudioFormat;

        if (pcmEncoding == 2) {
            aaudioFormat = AAUDIO_FORMAT_PCM_I16;
        } else if (pcmEncoding == 4) {
            aaudioFormat = AAUDIO_FORMAT_PCM_FLOAT;
        } else {
            LOG_ERROR("Unsupported PCM encoding, fallback to I16");
            aaudioFormat = AAUDIO_FORMAT_PCM_I16;
        }
        AAudioStreamBuilder_setDirection(m_builder, AAUDIO_DIRECTION_OUTPUT);
        AAudioStreamBuilder_setSampleRate(m_builder, m_sample_rate);
        AAudioStreamBuilder_setChannelCount(m_builder, m_channel_rate);
        AAudioStreamBuilder_setFormat(m_builder, aaudioFormat);
        AAudioStreamBuilder_setPerformanceMode(m_builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
        AAudioStreamBuilder_openStream(m_builder, &m_stream);
        AAudioStream_requestStart(m_stream);
        is_streamer_ready = true;
    }

    void AudioStreamer::write_pcm_to_stream(uint8_t *pcm, size_t outSize) {
        int bytePerSample = 2;
        int frameSize = bytePerSample * m_channel_rate;
        int frames = outSize / frameSize;
        if (is_streamer_ready) {
            AAudioStream_write(m_stream, pcm, frames, 100000000);
            if (audio_offset < 0) {
                int64_t clock = get_audio_clock();
                if (clock >= 0) {
                    audio_offset = clock;
                }
            }
        }
    }

    void AudioStreamer::shut_down() {
        AAudioStream_close(m_stream);
        AAudioStreamBuilder_delete(m_builder);
    }

    int64_t AudioStreamer::get_audio_clock() {
        {
            int64_t framePos;
            int64_t timeNanoSec;
            aaudio_result_t result = AAudioStream_getTimestamp(
                    m_stream,
                    CLOCK_MONOTONIC,
                    &framePos,
                    &timeNanoSec
            );
            if (result != AAUDIO_OK) return -1;
            return (framePos * 1 * 1000 * 1000LL) / m_sample_rate;
        }
    }

    int64_t AudioStreamer::get_audio_clock_frame_display_time(int64_t vidPtsNano) {
        int64_t framePos;
        int64_t audioTimeNano;
        aaudio_result_t result = AAudioStream_getTimestamp(
                m_stream,
                CLOCK_MONOTONIC,
                &framePos,
                &audioTimeNano
        );
        if (result != AAUDIO_OK) return 0;
        int64_t audioPosNano = (framePos * 1000000000LL) / m_sample_rate;
        int64_t audioDelay = vidPtsNano - audioPosNano;
        return audioTimeNano + audioDelay;
    }

    int64_t AudioStreamer::get_audio_start_offset() {
        return audio_offset;
    }
}