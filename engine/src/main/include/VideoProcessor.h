//
// Created by ghima on 15-02-2026.
//

#ifndef OSFEATURENDKDEMO_VIDEOPROCESSOR_H
#define OSFEATURENDKDEMO_VIDEOPROCESSOR_H

#include "android_native_app_glue.h"
#include "Util.h"
#include "VulkanRenderer.h"
#include "GraphicsPipeline.h"
#include "MediaDecoder.h"
#include "glm/ext/matrix_transform.hpp"

namespace ve {
    class VideoProcessor {
    private:
        VideoProcessor(android_app *app);

        static VideoProcessor *instance;
        GpuContext *m_gpuContext = nullptr;
        SwapchainContext *m_swapchainContext = nullptr;
        VulkanRenderer *m_renderer = nullptr;
        GraphicsPipeline *m_graphics_pipeline = nullptr;
        MediaDecoder *m_media_decoder = nullptr;
        std::unordered_map<AHardwareBuffer *, AndroidHardwareBufferResource> m_buffer_resource{};
        VkAndroidHardwareBufferFormatPropertiesANDROID formatProperties{};

        std::chrono::steady_clock::time_point startPoint;
        bool firstRender = true;

        void setup_android_hardware_buffer_resource(AHardwareBuffer *hardwareBuffer,
                                                    AndroidHardwareBufferResource &resource);

        void clean_up_buffer_resources();

        void
        clean_up_buffer_resource(const std::pair<AHardwareBuffer *, AndroidHardwareBufferResource>& pair);

    public:
        static VideoProcessor *get_instance(android_app *app);

        void create_graphics_pipeline();

        void on_term_window();

        void create_swapchain_graphics_context(android_app *app);

        void recreate_swapchain_context();

        void start_media_decoder(int fd);

        void play();

        GpuContext *get_gpu_context() {
            return m_gpuContext;
        }

        int64_t get_audio_clock() {
            return m_media_decoder->get_audio_clock();
        }

        int64_t get_audio_clock_frame_display_time(int64_t vidPtsNano) {
            return m_media_decoder->get_audio_clock_frame_display_time(vidPtsNano);
        }

        int64_t get_vid_start_point() {
            return m_media_decoder->get_vid_start_point();
        }


        int32_t get_frame_width() {
            return m_media_decoder->get_frame_width();
        }

        int32_t get_frame_height() {
            return m_media_decoder->get_frame_height();
        }

        FrameInfo get_frame_info() {
            FrameInfo frameInfo;
            get_scale_parameters(frameInfo);
            frameInfo.width = get_frame_width();
            frameInfo.height = get_frame_height();
            return frameInfo;
        }


        void dec_live_images_count() {
            m_media_decoder->dec_live_images_count();
        }

        void dec_live_Images_count_with_notify() {
            m_media_decoder->dec_live_images_count_with_notify();
        }

        void set_pause_render(bool val){
            m_media_decoder->set_pause_render(val);
        }

        void get_scale_parameters(FrameInfo &frameInfo) {
            float videoAspect =
                    (float) (VideoProcessor::get_instance(nullptr)->get_frame_width()) /
                    (VideoProcessor::get_instance(nullptr)->get_frame_height());
            float surfaceAspect =
                    (float) (m_swapchainContext->windowExtents.width) /
                    (m_swapchainContext->windowExtents.height);
            LOG_INFO("The aspect %f, %f", videoAspect, surfaceAspect);
            if (videoAspect > surfaceAspect) {
                float yScale = surfaceAspect / videoAspect;
                glm::vec2 scale{1, yScale};
                glm::mat4 mat{1};
                frameInfo.scale = scale;
                frameInfo.rotation = mat;
            } else {
                float xScale = videoAspect / surfaceAspect;
                glm::vec2 scale{xScale, 1};
                glm::mat4 mat{1};
                mat = glm::rotate(mat, glm::radians(90.f), glm::vec3{0, 0, 1});
                frameInfo.scale = scale;
                frameInfo.rotation = mat;
            }
        }

    };
}
#endif //OSFEATURENDKDEMO_VIDEOPROCESSOR_H
