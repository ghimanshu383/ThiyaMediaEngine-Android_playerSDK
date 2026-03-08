//
// Created by ghima on 12-02-2026.
//
#include "VulkanRenderer.h"
#include "imgui/imgui.h"
#include <vector>
#include <set>

namespace ve {
    VulkanRenderer::VulkanRenderer(GpuContext *gpuContext, SwapchainContext *swapchainContext)
            : m_gpuContext{gpuContext}, m_swapChainContext{swapchainContext} {
        init();
    }

    void VulkanRenderer::init() {
        if (m_gpuContext->window == nullptr) {
            LOG_ERROR("No Valid App window Found");
            return;
        }
        create_instance();
        create_surface();
        select_device_and_create_logical_device();
        create_command_pool();
    }

    void VulkanRenderer::create_instance() {
        VkApplicationInfo applicationInfo{};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.apiVersion = VK_API_VERSION_1_1;
        applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.pApplicationName = "VideoEngineHardwareApp";
        applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        applicationInfo.pEngineName = "VideoEngineHardware";

        std::vector<const char *> extensions{VK_KHR_SURFACE_EXTENSION_NAME,
                                             VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
                                             VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                                             VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME};
        std::vector<const char *> requiredLayers = {"VK_LAYER_KHRONOS_validation"};
        VkInstanceCreateInfo instanceCreateInfo{};
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.enabledLayerCount = requiredLayers.size();
        instanceCreateInfo.ppEnabledLayerNames = requiredLayers.data();
        instanceCreateInfo.enabledExtensionCount = extensions.size();
        instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

        VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance),
                 "Failed to create the instance for the application");
        m_gpuContext->instance = m_instance;
        LOG_INFO("Vulkan Instance Created Successfully");
    }

    void VulkanRenderer::create_surface() {
        VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo{};
        surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        surfaceCreateInfo.window = m_gpuContext->window;
        VK_CHECK(vkCreateAndroidSurfaceKHR(m_instance, &surfaceCreateInfo, nullptr,
                                           &m_android_surface),
                 "Failed to create any surface for the window");
        LOG_INFO("Window Surface Created Successfully");
        m_gpuContext->surface = m_android_surface;
    }

    void VulkanRenderer::select_device_and_create_logical_device() {
        uint32_t count = 0;
        std::vector<VkPhysicalDevice> devices{};
        vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
        devices.resize(count);
        vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

        auto iter = devices.begin();
        while (iter != devices.end()) {
            if (is_device_suitable(*iter)) {
                m_physicalDevice = *iter;
                break;
            }
        }
        if (m_physicalDevice == VK_NULL_HANDLE) {
            LOG_ERROR("No Suitable device found");
            return;
        }
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);
        LOG_INFO("Physical Device name %s", properties.deviceName);
        LOG_INFO("Queue Indices graphics %d compute %d presentation %d",
                 m_queue_family_indices.graphicsQueue, m_queue_family_indices.computeQueue,
                 m_queue_family_indices.presentationQueue);

        create_logical_device(m_physicalDevice);
        m_gpuContext->physicalDevice = m_physicalDevice;
        m_gpuContext->logicalDevice = m_logicalDevice;
        m_gpuContext->computeQueue = m_compute_queue;
        m_gpuContext->graphicsQueue = m_graphics_queue;
        m_gpuContext->presentationQueue = m_presentation_queue;
        m_gpuContext->computeQueueIndex = m_queue_family_indices.computeQueue;
        m_gpuContext->graphicsQueueIndex = m_queue_family_indices.graphicsQueue;
        m_gpuContext->presentationQueueIndex = m_queue_family_indices.presentationQueue;
        m_gpuContext->deviceName = std::string(properties.deviceName);
        m_gpuContext->gpuTimeStampPeriod = properties.limits.timestampPeriod;
        LOG_INFO("Device and Queue Configuration Successful");
    }

    bool VulkanRenderer::is_device_suitable(VkPhysicalDevice device) {
        m_queue_family_indices.reset();
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
        std::vector<VkQueueFamilyProperties> properties(count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.data());

        std::vector<VkQueueFamilyProperties>::iterator grapIter = std::find_if(properties.begin(),
                                                                               properties.end(),
                                                                               [](VkQueueFamilyProperties property) -> bool {
                                                                                   return property.queueFlags &
                                                                                          (VK_QUEUE_GRAPHICS_BIT |
                                                                                           VK_QUEUE_TRANSFER_BIT);
                                                                               });
        if (grapIter != properties.end()) {
            m_queue_family_indices.graphicsQueue = grapIter - properties.begin();
        }
        std::vector<VkQueueFamilyProperties>::iterator computeIter = std::find_if(
                properties.begin(), properties.end(),
                [](VkQueueFamilyProperties property) -> bool {
                    return property.queueFlags & VK_QUEUE_COMPUTE_BIT;
                });
        if (computeIter != properties.end()) {
            m_queue_family_indices.computeQueue = computeIter - properties.begin();
        }

        for (int i = 0; i < properties.size(); i++) {
            VkBool32 isPresent;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_android_surface, &isPresent);
            if (isPresent) {
                m_queue_family_indices.presentationQueue = i;
                break;
            }
        }
        return m_queue_family_indices.is_valid();
    }

    void VulkanRenderer::create_logical_device(VkPhysicalDevice device) {
        float priority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfo{};
        std::set<int32_t>
                sets{m_queue_family_indices.graphicsQueue, m_queue_family_indices.computeQueue,
                     m_queue_family_indices.presentationQueue};
        for (int32_t i: sets) {
            VkDeviceQueueCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            createInfo.queueCount = 1;
            createInfo.pQueuePriorities = &priority;
            createInfo.queueFamilyIndex = static_cast<uint32_t>(i);
            queueCreateInfo.push_back(createInfo);
        }

        std::vector<const char *> requiredExtensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME,
                VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
                VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
                VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
                VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
                VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
                VK_KHR_MAINTENANCE1_EXTENSION_NAME,
                VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME
        };

        std::vector<VkExtensionProperties> list;
        std::uint32_t count;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
        list.resize(count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &count, list.data());
        for (int i = 0; i < requiredExtensions.size(); i++) {
            auto iter = std::find_if(list.begin(), list.end(), [&](
                    VkExtensionProperties pr) -> bool {
                return std::strcmp(pr.extensionName, requiredExtensions[i]) == 0;
            });
            if (iter == list.end()) {
                LOG_ERROR("Extension Not found %s", requiredExtensions[i]);
            }
        }
        VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcrFeature{};
        ycbcrFeature.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES;
        ycbcrFeature.samplerYcbcrConversion = VK_TRUE;

        VkPhysicalDeviceFeatures2 features{};
        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features.pNext = &ycbcrFeature;

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.queueCreateInfoCount = queueCreateInfo.size();
        deviceCreateInfo.pQueueCreateInfos = queueCreateInfo.data();
        deviceCreateInfo.enabledExtensionCount = requiredExtensions.size();
        deviceCreateInfo.ppEnabledExtensionNames = requiredExtensions.data();
        deviceCreateInfo.pNext = &features;

        VK_CHECK(vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_logicalDevice),
                 "Failed to create logical device ");
        LOG_INFO("Logical Device Created Successfully");
        vkGetDeviceQueue(m_logicalDevice, (uint32_t) m_queue_family_indices.graphicsQueue, 0,
                         &m_graphics_queue);
        vkGetDeviceQueue(m_logicalDevice, (uint32_t) m_queue_family_indices.computeQueue, 0,
                         &m_compute_queue);
        vkGetDeviceQueue(m_logicalDevice, (uint32_t) m_queue_family_indices.presentationQueue, 0,
                         &m_presentation_queue);

    }

    VkSurfaceFormatKHR VulkanRenderer::choose_surface_format() {
        uint32_t count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_android_surface, &count, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_android_surface, &count,
                                             formats.data());

        if (formats[0].format == VK_FORMAT_UNDEFINED) {
            return {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        }
        return formats[0];
    }

    VkPresentModeKHR VulkanRenderer::choose_present_mode() {
        uint32_t count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_android_surface, &count,
                                                  nullptr);
        std::vector<VkPresentModeKHR> modes(count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_android_surface, &count,
                                                  modes.data());
        for (VkPresentModeKHR mode: modes) {
            if (mode == VK_PRESENT_MODE_FIFO_KHR) return mode;
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    void VulkanRenderer::create_swapchain() {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_android_surface,
                                                  &m_swapChainContext->surfaceCapabilities);
        m_swapChainContext->surfaceFormat = choose_surface_format();
        m_swapChainContext->presentMode = choose_present_mode();
        m_swapChainContext->minImageCount =
                m_swapChainContext->surfaceCapabilities.minImageCount + 1;

        if (m_swapChainContext->surfaceCapabilities.maxImageCount > 0 &&
            m_swapChainContext->minImageCount >
            m_swapChainContext->surfaceCapabilities.maxImageCount) {
            m_swapChainContext->minImageCount = m_swapChainContext->surfaceCapabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_android_surface;
        createInfo.oldSwapchain = VK_NULL_HANDLE;
        createInfo.imageFormat = m_swapChainContext->surfaceFormat.format;
        createInfo.imageColorSpace = m_swapChainContext->surfaceFormat.colorSpace;
        createInfo.presentMode = m_swapChainContext->presentMode;
        createInfo.minImageCount = m_swapChainContext->minImageCount;
        createInfo.imageExtent = m_swapChainContext->surfaceCapabilities.currentExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.imageUsage =
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        createInfo.preTransform = m_swapChainContext->surfaceCapabilities.currentTransform;

        if (m_queue_family_indices.graphicsQueue != m_queue_family_indices.presentationQueue) {
            std::array<uint32_t, 2> indices{
                    static_cast<uint32_t >(m_queue_family_indices.graphicsQueue),
                    static_cast<uint32_t >(m_queue_family_indices.presentationQueue)};
            createInfo.queueFamilyIndexCount = indices.size();
            createInfo.pQueueFamilyIndices = indices.data();
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        vkCreateSwapchainKHR(m_logicalDevice, &createInfo, nullptr, &m_swapChainContext->swapchain);
        LOG_INFO("Swap chain Created successfully");

        m_swapChainContext->swapchainImages.resize(m_swapChainContext->minImageCount);
        vkGetSwapchainImagesKHR(m_logicalDevice, m_swapChainContext->swapchain,
                                &m_swapChainContext->minImageCount,
                                m_swapChainContext->swapchainImages.data());
        m_swapChainContext->swapchainImageViews.resize(m_swapChainContext->minImageCount);
        for (int i = 0; i < m_swapChainContext->minImageCount; i++) {
            create_image_view(m_logicalDevice, m_swapChainContext->swapchainImages[i],
                              m_swapChainContext->swapchainImageViews[i],
                              m_swapChainContext->surfaceFormat.format);
        }
        LOG_INFO("Swapchain images and views configured correctly");
        m_swapChainContext->windowExtents = m_swapChainContext->surfaceCapabilities.currentExtent;
    }

    void VulkanRenderer::create_command_pool() {
        VkCommandPoolCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        createInfo.queueFamilyIndex = m_gpuContext->graphicsQueueIndex;
        createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK_CHECK(vkCreateCommandPool(m_gpuContext->logicalDevice, &createInfo, nullptr,
                                     &m_gpuContext->graphicsCommandPool),
                 "Failed to create the graphics command pool");
        VkCommandPoolCreateInfo createInfoCompute{};
        createInfoCompute.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        createInfoCompute.queueFamilyIndex = m_gpuContext->computeQueueIndex;
        createInfoCompute.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK_CHECK(vkCreateCommandPool(m_gpuContext->logicalDevice, &createInfoCompute, nullptr,
                                     &m_gpuContext->computeCommandPool),
                 "Failed to create the compute command pool");
        //setting up the query frame.
        uint32_t queryCount = MAX_FRAMES * 4;
        VkQueryPoolCreateInfo queryPoolCreateInfo{};
        queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolCreateInfo.queryCount = queryCount;
        VK_CHECK(vkCreateQueryPool(m_logicalDevice, &queryPoolCreateInfo, nullptr,
                                   &m_gpuContext->gpuQueryPool), "Failed to create the query pool");
    }

    void VulkanRenderer::clean_up_swapchain_context() {
        for (int i = 0; i < m_swapChainContext->swapchainImageViews.size(); i++) {
            vkDestroyImageView(m_logicalDevice, m_swapChainContext->swapchainImageViews[i],
                               nullptr);
        }
        vkDestroySwapchainKHR(m_logicalDevice, m_swapChainContext->swapchain, nullptr);
        m_swapChainContext->swapchain = VK_NULL_HANDLE;
        vkDestroySurfaceKHR(m_instance, m_android_surface, nullptr);
        m_android_surface = VK_NULL_HANDLE;
        m_gpuContext->surface = VK_NULL_HANDLE;
    }

    void VulkanRenderer::create_swapchain_context() {
        if (m_android_surface == VK_NULL_HANDLE) {
            create_surface();
        }
        if (m_swapChainContext->swapchain == VK_NULL_HANDLE) {
            create_swapchain();
        }
    }

    void VulkanRenderer::shutdown_command_pool() {
        vkDestroyCommandPool(m_gpuContext->logicalDevice, m_gpuContext->graphicsCommandPool,
                             nullptr);
        vkDestroyCommandPool(m_gpuContext->logicalDevice, m_gpuContext->computeCommandPool,
                             nullptr);
        vkDestroyQueryPool(m_gpuContext->logicalDevice, m_gpuContext->gpuQueryPool, nullptr);
        m_gpuContext->graphicsCommandPool = VK_NULL_HANDLE;
        m_gpuContext->computeCommandPool = VK_NULL_HANDLE;
        m_gpuContext->gpuQueryPool = VK_NULL_HANDLE;
    }
}