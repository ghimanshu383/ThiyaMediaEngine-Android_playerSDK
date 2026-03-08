//
// Created by ghima on 17-02-2026.
//

#ifndef OSFEATURENDKDEMO_MEDIADECODER_H
#define OSFEATURENDKDEMO_MEDIADECODER_H

#include <thread>

#include "media/NdkImage.h"
#include "media/NdkImageReader.h"
#include "media/NdkMediaCodec.h"
#include "media/NdkMediaExtractor.h"
#include "stdint.h"
#include "queue"
#include "mutex"
#include "Util.h"
#include "AudioStreamer.h"

namespace ve {
    class MediaDecoder {

        AMediaExtractor *m_extractor = nullptr;
        AMediaCodec *m_decoder_vid = nullptr;
        AMediaCodec *m_decoder_aud = nullptr;
        AMediaFormat *m_video_format = nullptr;
        AMediaFormat *m_audio_format = nullptr;
        ANativeWindow *m_native_window = nullptr;
        AImageReader *m_image_reader = nullptr;
        std::int32_t m_max_images = 8;
        std::int32_t m_live_images = 0;
        AImageReader_ImageListener m_image_listener{};
        int m_fd = -1;

        std::thread m_decoderThread;
        std::thread m_aud_streamer_thread;
        std::thread m_vid_streamer_thread;
        std::condition_variable m_aud_cv{};
        std::condition_variable m_vid_cv{};
        std::atomic<bool> m_stop_vid{false};
        std::atomic<bool> m_stop_aud{false};
        std::atomic<bool> m_vid_extractor_finish{false};
        std::atomic<bool> m_aud_extractor_finish{false};
        std::atomic<bool> m_pause_render {false};
        std::int32_t m_frame_width = 0;
        std::int32_t m_frame_height = 0;
        bool m_vid_decoder_ready = false;
        bool m_aud_decoder_ready = false;
        AudioStreamer *m_audio_streamer = nullptr;
        std::queue<int64_t> m_vid_pts_queue{};
        std::mutex _pts_mutex;
        std::queue<HardwareBufferFrame> m_buffer_queue;
        std::queue<AudioFrame> m_pcm_queue;
        std::queue<CodecVidFrame> m_codec_vid_samples;
        std::condition_variable m_codec_streamer_cv;

        int64_t m_vid_start_point = 0;
        bool m_vid_first_render = true;

        static void on_surface_image_available(void *ctx, AImageReader *reader);

        void decode_loop();

        void setup_vid_decoder(int mediaTrack);

        void feed_vid_decoder();

        void start_vid_streamer();

        void setup_aud_decoder(int audioTrack);

        void start_aud_streamer();

        void feed_aud_decoder();

        void clean_up_vid_decoder();

        void clean_up_aud_decoder();


    public:
        std::mutex _codecMutex;
        std::mutex _codecMutexAud{};
        std::mutex _codecMutexVidStreamer{};

        void start_decoder_thread(int fd);

        void shut_down();

        int64_t get_audio_clock();

        int64_t get_start_audio_offset();

        int64_t get_audio_clock_frame_display_time(int64_t vidPtsNano);

        std::queue<HardwareBufferFrame> &get_buffer_queue() { return m_buffer_queue; };

        bool is_audio_queue_empty() { return m_pcm_queue.empty(); }

        std::int32_t get_frame_width() { return m_frame_width; };

        std::int32_t get_frame_height() { return m_frame_height; }

        void dec_live_images_count() {
            std::lock_guard<std::mutex> lock{_codecMutex};
            m_live_images--;
        }

        void dec_live_images_count_with_notify() {
            {
                std::lock_guard<std::mutex> lock{_codecMutex};
                m_live_images--;
            }
            m_vid_cv.notify_all();
        }
        void set_pause_render(bool val) {
            m_pause_render.store(val);
        }
        int64_t get_vid_start_point() { return m_vid_start_point; }
    };
}
#endif //OSFEATURENDKDEMO_MEDIADECODER_H
