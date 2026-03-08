//
// Created by ghima on 17-02-2026.
//
#include <unistd.h>
#include "MediaDecoder.h"
#include "VideoProcessor.h"
#include "Util.h"

namespace ve {

    void MediaDecoder::start_decoder_thread(int fd) {
        m_fd = fd;
        m_decoderThread = std::thread([this]() -> void {
            m_image_listener = {
                    .context = this,
                    .onImageAvailable = &MediaDecoder::on_surface_image_available
            };
            m_stop_aud.store(false);
            m_stop_vid.store(false);
            m_live_images = 0;
            decode_loop();
        });
    }

    void MediaDecoder::on_surface_image_available(void *ctx, AImageReader *reader) {
        MediaDecoder *decoder = reinterpret_cast<MediaDecoder *>(ctx);
        HardwareBufferFrame frame{};
        media_status_t status = AImageReader_acquireNextImage(reader, &frame.image);
        if (status != AMEDIA_OK) {
            decoder->m_live_images--;
            return;
        }
        if (decoder->m_stop_vid.load()) {
            AImage_delete(frame.image);
            LOG_INFO("Releasing the buffers in the listener and existing");
            decoder->m_live_images--;
            return;
        }
        AImage_getHardwareBuffer(frame.image, &frame.buffer);
        if (frame.buffer) {
            AHardwareBuffer_acquire(frame.buffer);

            {
                std::lock_guard<std::mutex> lock{decoder->_pts_mutex};
                frame.pts = decoder->m_vid_pts_queue.front();
                decoder->m_vid_pts_queue.pop();
            }
            {
                std::lock_guard<std::mutex> lock{decoder->_codecMutex};
                if (!decoder->m_stop_vid.load()) {
                    decoder->m_buffer_queue.push(std::move(frame));
                } else {
                    AHardwareBuffer_release(frame.buffer);
                    AImage_delete(frame.image);
                    decoder->m_live_images--;
                    LOG_INFO("Releasing the frames");
                }

            }
        } else {
            AImage_delete(frame.image);
        }
    }

    void MediaDecoder::decode_loop() {
        m_extractor = AMediaExtractor_new();
        if (!m_extractor) {
            LOG_ERROR("Failed to start the extractor ");
            return;
        }
        media_status_t status = AMediaExtractor_setDataSourceFd(m_extractor, m_fd, 0, LONG_MAX);
        if (status != AMEDIA_OK) {
            LOG_ERROR("Failed to set the extractor for fd %d", m_fd);
            return;
        }
        int trackCount = AMediaExtractor_getTrackCount(m_extractor);
        int videoTrack = -1;
        int audioTrack = -1;

        for (int i = 0; i < trackCount; i++) {
            AMediaFormat *format = AMediaExtractor_getTrackFormat(m_extractor, i);
            const char *mime = nullptr;
            AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime);
            if (strncmp(mime, "video/", 6) == 0) {
                videoTrack = i;
                m_video_format = format;
                break;
            }
            AMediaFormat_delete(format);
        }
        for (int i = 0; i < trackCount; i++) {
            AMediaFormat *format = AMediaExtractor_getTrackFormat(m_extractor, i);
            const char *mime = nullptr;
            AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime);
            if (strncmp(mime, "audio/", 6) == 0) {
                audioTrack = i;
                m_audio_format = format;
                break;
            }
            AMediaFormat_delete(format);
        }
        if (videoTrack != -1) {
            setup_vid_decoder(videoTrack);
            start_vid_streamer();
        } else {
            LOG_INFO("No Video Track Found for this media");
        }

        if (audioTrack != -1) {
            setup_aud_decoder(audioTrack);
            start_aud_streamer();
        } else {
            LOG_INFO("No Audio Track Found for this media");
        }
        if (!m_vid_decoder_ready) m_stop_vid.store(true);
        if (!m_aud_decoder_ready) m_stop_aud.store(true);

        // extracting the aimage from the decoder
        while (!m_stop_vid.load() || !m_stop_aud.load()) {
            int trackIndex = AMediaExtractor_getSampleTrackIndex(m_extractor);
            if (trackIndex == videoTrack && m_vid_decoder_ready) {
                feed_vid_decoder();
            }
            if (trackIndex == audioTrack && m_aud_decoder_ready) {
                feed_aud_decoder();
            }
            if (trackIndex == -1) {
                m_vid_extractor_finish.store(true);
                m_aud_extractor_finish.store(true);
                break;
            }
            AMediaExtractor_advance(m_extractor);
        }
    }

    void MediaDecoder::setup_vid_decoder(int videoTrack) {
        AMediaExtractor_selectTrack(m_extractor, videoTrack);
        AMediaFormat_getInt32(m_video_format, AMEDIAFORMAT_KEY_WIDTH, &m_frame_width);
        AMediaFormat_getInt32(m_video_format, AMEDIAFORMAT_KEY_HEIGHT, &m_frame_height);
        LOG_INFO("Format Width and Height %d x %d", m_frame_width, m_frame_height);

        // Creating the decoder
        media_status_t readerStatus = AImageReader_newWithUsage(m_frame_width, m_frame_height,
                                                                AIMAGE_FORMAT_YUV_420_888,
                                                                AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE,
                                                                m_max_images,
                                                                &m_image_reader);
        if (readerStatus != AMEDIA_OK) {
            LOG_ERROR("failed to create the image reader");
            return;
        }

        AImageReader_setImageListener(m_image_reader, &m_image_listener);

        AImageReader_getWindow(m_image_reader, &m_native_window);

        const char *mime = nullptr;
        AMediaFormat_getString(m_video_format, AMEDIAFORMAT_KEY_MIME, &mime);
        m_decoder_vid = AMediaCodec_createDecoderByType(mime);
        if (!m_decoder_vid) {
            LOG_ERROR("Failed to create the decoder for mime type %s", mime);
            return;
        }
        AMediaFormat_setInt32(m_video_format, AMEDIAFORMAT_KEY_COLOR_FORMAT,
                              AIMAGE_FORMAT_YUV_420_888);
        media_status_t cfg = AMediaCodec_configure(m_decoder_vid, m_video_format, m_native_window,
                                                   nullptr,
                                                   0);
        if (cfg != AMEDIA_OK) {
            LOG_INFO("Failed to configure the media codec");
            return;
        }
        if (AMediaCodec_start(m_decoder_vid) != AMEDIA_OK) {
            LOG_INFO("Failed to start the decoder");
            return;
        }
        LOG_INFO("The Codec is ready and started");
        m_vid_decoder_ready = true;
    }

    void MediaDecoder::feed_vid_decoder() {
        ssize_t sampleSize = AMediaExtractor_getSampleSize(m_extractor);
        if (sampleSize < 0) {
            m_vid_extractor_finish.store(true);
            return;
        }
        int64_t pts = AMediaExtractor_getSampleTime(m_extractor);
        std::unique_ptr<uint8_t[]> temp = std::make_unique<uint8_t[]>(sampleSize);

        AMediaExtractor_readSampleData(m_extractor, temp.get(), sampleSize);
        CodecVidFrame frame{std::move(temp), sampleSize, pts};
        {
            std::lock_guard<std::mutex> lock{_codecMutexVidStreamer};
            m_codec_vid_samples.push(std::move(frame));
            m_codec_streamer_cv.notify_one();
        }
    }

    void MediaDecoder::start_vid_streamer() {
        LOG_INFO("Starting the vid streamer");
        m_vid_streamer_thread = std::thread([this]() -> void {
            while (true) {
                if (m_stop_vid.load()) break;
                {
                    std::unique_lock<std::mutex> lock{_codecMutexVidStreamer};
                    m_codec_streamer_cv.wait(lock, [this]() -> bool {
                        return m_stop_vid.load() || !m_codec_vid_samples.empty() ||
                               m_vid_extractor_finish.load();
                    });
                }
                if (m_vid_extractor_finish.load() && m_codec_vid_samples.empty()) {
                    AMediaCodec_queueInputBuffer(m_decoder_vid, 0, 0, 0, 0,
                                                 AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                    break;
                }
                CodecVidFrame frame;
                {
                    std::lock_guard<std::mutex> lock{_codecMutexVidStreamer};
                    frame = std::move(m_codec_vid_samples.front());
                    m_codec_vid_samples.pop();
                }
                if (m_vid_first_render) {
                    m_vid_first_render = false;
                    m_vid_start_point = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count();
                }
                ssize_t inputIndex = AMediaCodec_dequeueInputBuffer(m_decoder_vid, 100000);

                if (inputIndex >= 0) {
                    size_t buffersize;
                    uint8_t *buffer = AMediaCodec_getInputBuffer(m_decoder_vid, inputIndex,
                                                                 &buffersize);
                    memcpy(buffer, frame.sample.get(), frame.sampleSize);
                    AMediaCodec_queueInputBuffer(m_decoder_vid, inputIndex, 0, frame.sampleSize,
                                                 frame.pts,
                                                 0);
                    AMediaCodecBufferInfo bufferInfo;
                    ssize_t outputIndex;
                    while ((outputIndex = AMediaCodec_dequeueOutputBuffer(m_decoder_vid,
                                                                          &bufferInfo, 0)) >= 0) {
                        {
                            std::lock_guard<std::mutex> lock{_pts_mutex};
                            m_vid_pts_queue.push(bufferInfo.presentationTimeUs);
                        }
                        {
                            std::unique_lock<std::mutex> lock{_codecMutex};
                            m_vid_cv.wait(lock, [this]() -> bool {
                                return m_stop_vid.load() || m_live_images < m_max_images - 1;
                            });
                            if (m_stop_vid.load()) break;
                            if (m_pause_render.load()) continue;
                        }
                        AMediaCodec_releaseOutputBuffer(m_decoder_vid, outputIndex, true);
                        m_live_images++;
                    }
                }
            }
            clean_up_vid_decoder();
        });
    }

    void MediaDecoder::setup_aud_decoder(int audioTrack) {
        AMediaExtractor_selectTrack(m_extractor, audioTrack);
        const char *mime = nullptr;
        AMediaFormat_getString(m_audio_format, AMEDIAFORMAT_KEY_MIME, &mime);
        m_decoder_aud = AMediaCodec_createDecoderByType(mime);
        if (!m_decoder_aud) {
            LOG_ERROR("Failed to create the decoder for audio %s", mime);
            return;
        }

        media_status_t status = AMediaCodec_configure(m_decoder_aud, m_audio_format, nullptr,
                                                      nullptr, 0);
        if (status != AMEDIA_OK) {
            LOG_ERROR("Failed to configure the decoder for audio");
            return;
        }
        AMediaCodec_start(m_decoder_aud);
        m_aud_decoder_ready = true;
        LOG_INFO("Audio Decoder started and ready");
        if (m_audio_streamer == nullptr) {
            int32_t sampleRate;
            int32_t channelCount;
            int32_t pcmEncoding;
            AMediaFormat_getInt32(m_audio_format, AMEDIAFORMAT_KEY_SAMPLE_RATE,
                                  &sampleRate);
            AMediaFormat_getInt32(m_audio_format, AMEDIAFORMAT_KEY_CHANNEL_COUNT,
                                  &channelCount);
            AMediaFormat_getInt32(m_audio_format, AMEDIAFORMAT_KEY_PCM_ENCODING,
                                  &pcmEncoding);
            m_audio_streamer = new AudioStreamer(sampleRate, channelCount, pcmEncoding);
            LOG_INFO("Audio Streamer is ready");
        }
    }

    void MediaDecoder::feed_aud_decoder() {
        if (m_stop_aud.load()) return;
        ssize_t inputIndex = AMediaCodec_dequeueInputBuffer(m_decoder_aud, 10000);
        if (inputIndex >= 0) {
            size_t bufferSize;
            uint8_t *inputBuffer = AMediaCodec_getInputBuffer(m_decoder_aud, inputIndex,
                                                              &bufferSize);
            ssize_t sampleSize = AMediaExtractor_readSampleData(m_extractor, inputBuffer,
                                                                bufferSize);
            if (sampleSize < 0) {
                AMediaCodec_queueInputBuffer(m_decoder_aud, inputIndex, 0, 0, 0,
                                             AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                m_stop_aud.store(true);
                return;
            }
            int64_t pts = AMediaExtractor_getSampleTime(m_extractor);
            AMediaCodec_queueInputBuffer(
                    m_decoder_aud,
                    inputIndex,
                    0,
                    sampleSize,
                    pts,
                    0
            );
            // getting the output for the audio
            while (true) {
                AMediaCodecBufferInfo bufferInfo;
                ssize_t outputIndex = AMediaCodec_dequeueOutputBuffer(m_decoder_aud, &bufferInfo,
                                                                      0);
                if (outputIndex >= 0) {
                    size_t outSize;
                    uint8_t *pcm = AMediaCodec_getOutputBuffer(m_decoder_aud, outputIndex,
                                                               &outSize);
                    if (m_audio_streamer) {
                        std::unique_ptr<uint8_t[]> p_pcm = std::make_unique<uint8_t[]>(outSize);
                        memcpy(p_pcm.get(), pcm, outSize);
                        AudioFrame frame{std::move(p_pcm), outSize};
                        {
                            std::lock_guard<std::mutex> lock{_codecMutexAud};
                            m_pcm_queue.push(std::move(frame));
                            m_aud_cv.notify_one();
                        }
                    }
                    AMediaCodec_releaseOutputBuffer(m_decoder_aud, outputIndex, false);
                } else {
                    break;
                }
            }
        }
    }

    void MediaDecoder::start_aud_streamer() {
        m_aud_streamer_thread = std::thread([this]() -> void {
            LOG_INFO("Starting the Audio Streamer");
            while (!m_stop_aud.load()) {
                {
                    std::unique_lock<std::mutex> lock{_codecMutexAud};
                    m_aud_cv.wait(lock, [this]() -> bool {
                        return m_stop_aud.load() || !m_pcm_queue.empty();
                    });
                }
                if (m_stop_aud.load()) break;
                AudioFrame frame;
                {

                    std::lock_guard<std::mutex> lock{_codecMutexAud};
                    frame = std::move(m_pcm_queue.front());
                    m_pcm_queue.pop();
                }
                m_audio_streamer->write_pcm_to_stream(frame.pcm.get(), frame.outSize);
                m_aud_cv.notify_one();
            }
            clean_up_aud_decoder();
        });
    }

    int64_t MediaDecoder::get_audio_clock() {
        return m_audio_streamer->get_audio_clock();
    }

    void MediaDecoder::shut_down() {
        m_stop_vid.store(true);
        m_stop_aud.store(true);
        m_vid_extractor_finish.store(true);
        m_aud_extractor_finish.store(true);

        m_aud_cv.notify_all();
        m_vid_cv.notify_all();
        m_codec_streamer_cv.notify_all();
        if (m_decoderThread.joinable()) {
            m_decoderThread.join();
        }
        if (m_vid_streamer_thread.joinable()) {
            m_vid_streamer_thread.join();
        }
        if (m_aud_streamer_thread.joinable()) {
            m_aud_streamer_thread.join();
        }
    }


    int64_t MediaDecoder::get_start_audio_offset() {
        if (m_audio_streamer != nullptr) {
            return m_audio_streamer->get_audio_start_offset();
        }
        return -1;
    }


    int64_t MediaDecoder::get_audio_clock_frame_display_time(int64_t vidPtsNano) {
        if (m_audio_streamer == nullptr) return -1;
        return m_audio_streamer->get_audio_clock_frame_display_time(vidPtsNano);
    }

    void MediaDecoder::clean_up_vid_decoder() {
        if (m_decoder_vid) {
            AMediaCodec_flush(m_decoder_vid);
            AMediaCodec_stop(m_decoder_vid);
            AMediaCodec_delete(m_decoder_vid);
            m_decoder_vid = nullptr;
            LOG_INFO("Decoder Stopped");
        }
        if (m_video_format) {
            AMediaFormat_delete(m_video_format);
            m_video_format = nullptr;
            LOG_INFO("Video Format deleted");
        }

        {
            std::lock_guard<std::mutex> lock{_codecMutex};
            LOG_INFO("The Buffer Size is %zu and live_images are %d", m_buffer_queue.size(),
                     m_live_images);
            while (!m_buffer_queue.empty()) {
                HardwareBufferFrame frame = std::move(m_buffer_queue.front());
                AHardwareBuffer_release(frame.buffer);
                AImage_delete(frame.image);
                m_live_images--;
                m_buffer_queue.pop();
            }
        }
        LOG_INFO("Hardware Buffer queue emptied");

        LOG_INFO("The used frames are %d", m_live_images);
        // To clean up the used frames in the listener if any.
        if (m_image_reader) {
            AImageReader_delete(m_image_reader);
            m_image_reader = nullptr;
            m_native_window = nullptr;
            LOG_INFO("Image Reader Deleted");
        }
        if (m_extractor) {
            AMediaExtractor_delete(m_extractor);
            m_extractor = nullptr;
        }
        if (m_fd >= 0) {
            close(m_fd);
            m_fd = -1;
            LOG_INFO("Fd Released");
        }
    }

    void MediaDecoder::clean_up_aud_decoder() {

        if (m_decoder_aud) {
            AMediaCodec_flush(m_decoder_aud);
            AMediaCodec_stop(m_decoder_aud);
            AMediaCodec_delete(m_decoder_aud);
            LOG_INFO("The Audio decoder stopped");
        }
        if (m_audio_format) {
            AMediaFormat_delete(m_audio_format);
            LOG_INFO("The Audio format deleted");
        }
        if (m_audio_streamer) {
            m_audio_streamer->shut_down();
            delete m_audio_streamer;
            m_audio_streamer = nullptr;
        }
        for (int i = 0; i < m_pcm_queue.size(); i++) {
            m_pcm_queue.pop();
        }
    }
}