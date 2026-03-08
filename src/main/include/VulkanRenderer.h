//
// Created by ghima on 12-02-2026.
//

#ifndef OSFEATURENDKDEMO_VULKANGRAPHICS_H
#define OSFEATURENDKDEMO_VULKANGRAPHICS_H

#define VK_USE_PLATFORM_ANDROID_KHR

#include <vulkan/vulkan.h>
#include <android_native_app_glue.h>

#include "Util.h"
#include <vector>

namespace ve {
    class VulkanRenderer {
    private:
        GpuContext *m_gpuContext{};
        SwapchainContext *m_swapChainContext{};
        VkInstance m_instance{};
        VkSurfaceKHR m_android_surface{};
        VkPhysicalDevice m_physicalDevice{};
        VkDevice m_logicalDevice{};
        VkQueue m_graphics_queue{};
        VkQueue m_compute_queue{};
        VkQueue m_presentation_queue{};

        struct QueueFamilyIndices {
            int graphicsQueue = -1;
            int computeQueue = -1;
            int presentationQueue = -1;

            void reset() {
                graphicsQueue = -1;
                computeQueue = -1;
                presentationQueue = -1;
            }

            bool is_valid() {
                return graphicsQueue != -1 && presentationQueue != -1 && computeQueue != -1;
            }
        } m_queue_family_indices;

        void init();

        void create_instance();

        void create_surface();

        void select_device_and_create_logical_device();

        bool is_device_suitable(VkPhysicalDevice device);

        void create_logical_device(VkPhysicalDevice device);

        VkSurfaceFormatKHR choose_surface_format();

        VkPresentModeKHR choose_present_mode();

        void create_swapchain();

    public:
        VulkanRenderer(GpuContext *gpuContext, SwapchainContext *swapchainContext);

        void clean_up_swapchain_context();

        void create_swapchain_context();

        void create_command_pool();

        void shutdown_command_pool();
    };
}
#endif //OSFEATURENDKDEMO_VULKANGRAPHICS_H
