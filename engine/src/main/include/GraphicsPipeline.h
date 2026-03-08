//
// Created by ghima on 14-02-2026.
//

#ifndef OSFEATURENDKDEMO_GRAPHICSPIPELINE_H
#define OSFEATURENDKDEMO_GRAPHICSPIPELINE_H

#include <vulkan/vulkan.h>
#include "Util.h"

namespace ve {
    class GraphicsPipeline {
    private:
        GpuContext *m_gpuContext = nullptr;
        SwapchainContext *m_swapchainContext = nullptr;
        std::vector<VkFramebuffer> m_frame_buffers{};
        VkRenderPass m_render_pass{};
        VkPipeline m_graphics_pipeline{};
        VkPipelineLayout m_graphics_pipeline_layout{};
        VkViewport m_view_port{};
        VkRect2D m_scissors{};

        VkBuffer m_vertBuffer{};
        VkBuffer m_vertBufferStaging{};
        VkDeviceMemory m_vertBufferMemory{};
        VkDeviceMemory m_vertBufferMemoryStaging{};
        VkBuffer m_indexBuffer{};
        VkBuffer m_indexBufferStaging{};
        VkDeviceMemory m_indexBufferMemory{};
        VkDeviceMemory m_indexBufferMemoryStaging{};
        uint32_t m_curr_img_index = 0;
        std::vector<VideoFrames> m_frames{};
        int m_curr_frame = 0;
        int m_frame_count = 0;
        VkDescriptorPool m_imgui_des_pool{};

        void init();

        void create_frame_buffers();

        void create_render_pass();

        void create_pipeline();

        void setup_display_quad();

        void handle_compute_path_for_different_queues(AndroidHardwareBufferResource &resource);

        void create_fence_and_semaphores_and_allocate_command_buffer();

        bool
        begin_frame(HardwareBufferFrame *currFrame, AndroidHardwareBufferResource, FrameInfo &info);

        void draw(AndroidHardwareBufferResource &resource, FrameInfo &frameInfo);

        void end_frame();

        void on_window_rotation();

        VkDescriptorSetLayout m_des_layout{};
        VkDescriptorPool m_pool{};
        std::vector<VkDescriptorSet> m_set{};
        VkSampler m_hardware_image_sampler{};
        VkSampler m_default_sampler{};
        bool m_is_first_render = true;
        std::chrono::steady_clock::time_point m_present_start;
        uint64_t m_timeStamps[4] = {0};
        double m_gpu_time[2] = {0};

        void set_up_buffer_descriptors();

        void update_descriptors(AndroidHardwareBufferResource &resource, int currFrame);

        // Adding another render pass for imgui.
        VkRenderPass m_ui_render_pass{};
        std::vector<VkImage> m_ui_images{};
        std::vector<VkFramebuffer> m_ui_frame_buffers{};
        std::vector<VkDeviceMemory> m_ui_images_memory{};
        std::vector<VkImageView> m_ui_image_views{};


        void create_ui_render_pass();

        void create_ui_image_and_views_buffers();

        void create_imgui_descriptor_pool();

        void clean_up_ui_context();

        void setup_imgui_context();

        void add_ui_frame_stats(FrameInfo &info);

        void add_ui_frame(VkCommandBuffer &buffer, FrameInfo &info);

    public:
        GraphicsPipeline(GpuContext *context, SwapchainContext *swapchainContext);

        void
        render(HardwareBufferFrame *frame, AndroidHardwareBufferResource &resource);

        void clean_up();
    };
}
#endif //OSFEATURENDKDEMO_GRAPHICSPIPELINE_H
