//
// Created by ghima on 15-02-2026.
//

#include "VideoProcessor.h"
#include "GraphicsPipeline.h"

namespace ve {
    VideoProcessor *VideoProcessor::instance = nullptr;

    VideoProcessor *VideoProcessor::get_instance(android_app *app) {
        if (instance == nullptr) {
            instance = new VideoProcessor(app);
        }
        return instance;
    }

    VideoProcessor::VideoProcessor(android_app *app) {
        m_gpuContext = new GpuContext();
        m_swapchainContext = new SwapchainContext();
        m_gpuContext->app = app;
        m_gpuContext->window = app->window;
        m_renderer = new VulkanRenderer(m_gpuContext, m_swapchainContext);
    }

    void VideoProcessor::create_graphics_pipeline() {
        m_graphics_pipeline = new GraphicsPipeline(m_gpuContext, m_swapchainContext);
    }

    void VideoProcessor::on_term_window() {
        set_pause_render(true);
        vkDeviceWaitIdle(m_gpuContext->logicalDevice);
        m_graphics_pipeline->clean_up();
        delete m_graphics_pipeline;
        m_graphics_pipeline = nullptr;

        m_renderer->clean_up_swapchain_context();
        m_gpuContext->app = nullptr;
        m_gpuContext->window = nullptr;

        clean_up_buffer_resources();
        PFN_vkDestroySamplerYcbcrConversionKHR pfnVkDestroySamplerYcbcrConversion = (PFN_vkDestroySamplerYcbcrConversionKHR)
                vkGetDeviceProcAddr(m_gpuContext->logicalDevice,
                                    "vkDestroySamplerYcbcrConversionKHR");
        pfnVkDestroySamplerYcbcrConversion(m_gpuContext->logicalDevice, m_gpuContext->conversion,
                                           nullptr);
        m_gpuContext->conversion = VK_NULL_HANDLE;
        m_renderer->shutdown_command_pool();
        m_media_decoder->shut_down();
        delete m_media_decoder;
        m_media_decoder = nullptr;
    }

    void VideoProcessor::create_swapchain_graphics_context(android_app *app) {
        if (m_gpuContext->app == nullptr) {
            m_gpuContext->app = app;
            m_gpuContext->window = app->window;
        }
        if (m_gpuContext->graphicsCommandPool == VK_NULL_HANDLE ||
            m_gpuContext->computeCommandPool == VK_NULL_HANDLE) {
            m_renderer->create_command_pool();
        }
        m_buffer_resource.clear();
        m_gpuContext->windowWidth = ANativeWindow_getWidth(app->window);
        m_gpuContext->windowHeight = ANativeWindow_getHeight(app->window);
        m_renderer->create_swapchain_context();
        create_graphics_pipeline();
    }

    void VideoProcessor::recreate_swapchain_context() {
        vkDeviceWaitIdle(m_gpuContext->logicalDevice);
        m_renderer->clean_up_swapchain_context();
        m_renderer->create_swapchain_context();
    }

    void VideoProcessor::start_media_decoder(int fd) {
        if (m_media_decoder == nullptr) {
            m_media_decoder = new MediaDecoder();
        }
        m_media_decoder->start_decoder_thread(fd);
    }

    void VideoProcessor::play() {
        HardwareBufferFrame *frame = new HardwareBufferFrame();
        {
            std::lock_guard<std::mutex> lock{m_media_decoder->_codecMutex};
            if (m_media_decoder->get_buffer_queue().empty()) {
                return;
            }
            *frame = std::move(m_media_decoder->get_buffer_queue().front());
            m_media_decoder->get_buffer_queue().pop();
        }
        if (!frame->buffer) {
            return;
        }

        auto iter = m_buffer_resource.find(frame->buffer);
        AHardwareBuffer_Desc des;
        AHardwareBuffer_describe(frame->buffer, &des);
        if (iter == m_buffer_resource.end()) {
            // creating the hardware buffer resources for vulkan compute and render.
            AndroidHardwareBufferResource bufferResource{};
            bufferResource.width = des.width;
            bufferResource.height = des.height;
            setup_android_hardware_buffer_resource(frame->buffer, bufferResource);
            m_buffer_resource[frame->buffer] = bufferResource;
        } else {
            if (iter->second.width != des.width || iter->second.height != des.height) {
                LOG_INFO("Resource Miss/Flip! Recreating for %dx%d", des.width, des.height);
                vkDeviceWaitIdle(m_gpuContext->logicalDevice);
                clean_up_buffer_resource(*iter);
                m_buffer_resource.erase(iter);
                AndroidHardwareBufferResource bufferResource{};
                bufferResource.width = des.width;
                bufferResource.height = des.height;
                setup_android_hardware_buffer_resource(frame->buffer, bufferResource);
                m_buffer_resource[frame->buffer] = bufferResource;
            }
        }
        AHardwareBuffer *key = frame->buffer;
        m_graphics_pipeline->render(frame, m_buffer_resource[key]);
    }

    void VideoProcessor::setup_android_hardware_buffer_resource(AHardwareBuffer *hardwareBuffer,
                                                                AndroidHardwareBufferResource &resource) {
        VkAndroidHardwareBufferPropertiesANDROID bufferProperties{};
        formatProperties.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID;
        bufferProperties.sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID;
        bufferProperties.pNext = &formatProperties;

        PFN_vkGetAndroidHardwareBufferPropertiesANDROID pfnVkGetAndroidHardwareBufferPropertiesAndroid = (PFN_vkGetAndroidHardwareBufferPropertiesANDROID)
                vkGetDeviceProcAddr(m_gpuContext->logicalDevice,
                                    "vkGetAndroidHardwareBufferPropertiesANDROID");
        pfnVkGetAndroidHardwareBufferPropertiesAndroid(m_gpuContext->logicalDevice, hardwareBuffer,
                                                       &bufferProperties);

        LOG_INFO("Properties created successfully");

        if (m_gpuContext->conversion == VK_NULL_HANDLE) {
            create_y_cbcr_conversion_handler(m_gpuContext->logicalDevice, m_gpuContext->conversion,
                                             formatProperties);
        }

        create_image_external_buffer(m_gpuContext, hardwareBuffer, resource.image, resource.memory,
                                     bufferProperties, formatProperties,
                                     resource.width,
                                     resource.height);
        create_image_view_hardware_image(m_gpuContext->logicalDevice, resource.image,
                                         resource.imageView, m_gpuContext->conversion);

        std::string lumaName = "Luma Image " + std::to_string(m_buffer_resource.size());
        create_image(m_gpuContext, resource.lumaImage, resource.lumaImageMemory,
                     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_FORMAT_R8_UNORM,
                     resource.width, resource.height,
                     lumaName.c_str());
        create_image_view(m_gpuContext->logicalDevice, resource.lumaImage, resource.lumaImageView,
                          VK_FORMAT_R8_UNORM);
        std::string chromaName = "Chroma Image " + std::to_string(m_buffer_resource.size());
        create_image(m_gpuContext, resource.chromaImage, resource.chromaImageMemory,
                     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     VK_FORMAT_R8G8_UNORM, (resource.width >> 1),
                     (resource.height >> 1), chromaName.c_str());
        create_image_view(m_gpuContext->logicalDevice, resource.chromaImage,
                          resource.chromaImageView, VK_FORMAT_R8G8_UNORM);
        VkCommandBuffer commandBuffer = start_command_buffer(m_gpuContext);
        record_transition_image(commandBuffer, resource.image, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        record_transition_image(commandBuffer, resource.lumaImage, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_GENERAL, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        record_transition_image(commandBuffer, resource.chromaImage, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_GENERAL, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        submit_command_buffer(m_gpuContext, commandBuffer);

    }

    void VideoProcessor::clean_up_buffer_resources() {
        auto iter = m_buffer_resource.begin();
        while (iter != m_buffer_resource.end()) {
            vkDestroyImageView(m_gpuContext->logicalDevice, iter->second.imageView, nullptr);
            vkDestroyImage(m_gpuContext->logicalDevice, iter->second.image, nullptr);
            vkFreeMemory(m_gpuContext->logicalDevice, iter->second.memory, nullptr);

            vkDestroyImageView(m_gpuContext->logicalDevice, iter->second.lumaImageView, nullptr);
            vkDestroyImage(m_gpuContext->logicalDevice, iter->second.lumaImage, nullptr);
            vkFreeMemory(m_gpuContext->logicalDevice, iter->second.lumaImageMemory, nullptr);

            vkDestroyImageView(m_gpuContext->logicalDevice, iter->second.chromaImageView, nullptr);
            vkDestroyImage(m_gpuContext->logicalDevice, iter->second.chromaImage, nullptr);
            vkFreeMemory(m_gpuContext->logicalDevice, iter->second.chromaImageMemory, nullptr);
            iter = m_buffer_resource.erase(iter);
        }
    }

    void VideoProcessor::clean_up_buffer_resource(
            const std::pair<AHardwareBuffer *, AndroidHardwareBufferResource> &pair) {
        vkDestroyImageView(m_gpuContext->logicalDevice, pair.second.imageView, nullptr);
        vkDestroyImage(m_gpuContext->logicalDevice, pair.second.image, nullptr);
        vkFreeMemory(m_gpuContext->logicalDevice, pair.second.memory, nullptr);

        vkDestroyImageView(m_gpuContext->logicalDevice, pair.second.lumaImageView, nullptr);
        vkDestroyImage(m_gpuContext->logicalDevice, pair.second.lumaImage, nullptr);
        vkFreeMemory(m_gpuContext->logicalDevice, pair.second.lumaImageMemory, nullptr);

        vkDestroyImageView(m_gpuContext->logicalDevice, pair.second.chromaImageView, nullptr);
        vkDestroyImage(m_gpuContext->logicalDevice, pair.second.chromaImage, nullptr);
        vkFreeMemory(m_gpuContext->logicalDevice, pair.second.chromaImageMemory, nullptr);
    }

}