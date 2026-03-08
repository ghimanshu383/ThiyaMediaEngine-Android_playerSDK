//
// Created by ghima on 12-02-2026.
//

#ifndef OSFEATURENDKDEMO_UTIL_H
#define OSFEATURENDKDEMO_UTIL_H

#include <android/log.h>
#include <android/asset_manager.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <string>
#include <media/NdkImage.h>
#include <android_native_app_glue.h>

#define LOG_TAG "CORE_NATIVE_VIDEO_HARDWARE"
#define LOG_INFO(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOG_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define VK_CHECK(val, ...)      if(val != VK_SUCCESS) { \
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__); \
    std::exit(EXIT_FAILURE);                                                    \
    return;\
}

constexpr int MAX_FRAMES = 2;
const int MAX_AUD_FRAMES = 2;
const int64_t SYNC_THRESHOLD = 40 * 1000;

struct Vertex {
    glm::vec3 pos;
    glm::vec2 uv;
};

struct FrameInfo {
    glm::mat4 rotation;
    glm::vec2 scale;
    int32_t width;
    int32_t height;
    int32_t computePath;
};

struct ComputeInfoStruct {
    int bright;
    int contrast;
};

struct AndroidHardwareBufferResource {
    int width;
    int height;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkImage lumaImage = VK_NULL_HANDLE;
    VkImageView lumaImageView = VK_NULL_HANDLE;
    VkDeviceMemory lumaImageMemory = VK_NULL_HANDLE;
    VkImage chromaImage = VK_NULL_HANDLE;
    VkImageView chromaImageView = VK_NULL_HANDLE;
    VkDeviceMemory chromaImageMemory = VK_NULL_HANDLE;
};
struct HardwareBufferFrame {
    AImage *image = nullptr;
    AHardwareBuffer *buffer = nullptr;
    int64_t pts = 0;
    int64_t audioClock = 0;
};


struct AudioFrame {
    std::unique_ptr<uint8_t[]> pcm;
    size_t outSize;
    int64_t pts;
};
struct GpuContext {
    android_app *app;
    ANativeWindow *window;
    VkInstance instance;
    VkPhysicalDevice physicalDevice{};
    VkDevice logicalDevice{};
    VkSurfaceKHR surface;
    VkQueue graphicsQueue;
    VkQueue computeQueue;
    VkQueue presentationQueue;
    int32_t swapChainImageCount;
    int32_t graphicsQueueIndex;
    int32_t computeQueueIndex;
    int32_t presentationQueueIndex;
    int32_t windowWidth;
    int32_t windowHeight;
    VkCommandPool graphicsCommandPool;
    VkCommandPool computeCommandPool;
    VkSamplerYcbcrConversion conversion = VK_NULL_HANDLE;
    std::string deviceName;
    float gpuTimeStampPeriod;
    VkQueryPool gpuQueryPool;
};
struct CodecVidFrame {
    std::unique_ptr<uint8_t[]> sample;
    ssize_t sampleSize;
    int64_t pts;
};
struct VideoFrames {
    VkSemaphore next_image_semaphore = VK_NULL_HANDLE;
    VkSemaphore render_finish_semaphore = VK_NULL_HANDLE;
    VkFence render_finish_fence = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer_compute = VK_NULL_HANDLE;
    VkSemaphore compute_semaphore = VK_NULL_HANDLE;
    HardwareBufferFrame *hbf;
    uint64_t presentId;
};
struct SwapchainContext {
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VkSurfaceFormatKHR surfaceFormat;
    VkPresentModeKHR presentMode;
    uint32_t minImageCount;
    VkExtent2D windowExtents;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkSwapchainKHR swapchain;
};

inline int32_t find_memory_index(GpuContext *context, uint32_t memoryTypeBits,
                                 VkMemoryPropertyFlags requiredFlags) {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(context->physicalDevice, &memoryProperties);
    for (int i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((memoryTypeBits & (1 << i)) &&
            ((memoryProperties.memoryTypes[i].propertyFlags & requiredFlags) == requiredFlags)) {
            return i;
        }
    }
    return -1;
}

inline int32_t find_memory_index_without_flags(GpuContext *context, uint32_t memoryTypeBits) {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(context->physicalDevice, &memoryProperties);
    for (int i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((memoryTypeBits & (1 << i))) {
            return i;
        }
    }
    return -1;
}

inline void
create_image_external_buffer(GpuContext *context, AHardwareBuffer *buffer, VkImage &image,
                             VkDeviceMemory &imageMemory,
                             VkAndroidHardwareBufferPropertiesANDROID bufferProperties,
                             VkAndroidHardwareBufferFormatPropertiesANDROID formatProperties,
                             uint32_t width, uint32_t height) {
    LOG_INFO("Creating the external buffer image");
    LOG_INFO("externalFormat = %lu", formatProperties.externalFormat);
    VkExternalFormatANDROID externalFormat{};
    externalFormat.sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID;
    externalFormat.externalFormat = formatProperties.externalFormat;

    VkExternalMemoryImageCreateInfo externalMemoryImageCreateInfo{};
    externalMemoryImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    externalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID;
    externalMemoryImageCreateInfo.pNext = &externalFormat;

    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.format = VK_FORMAT_UNDEFINED;
    imageCreateInfo.extent = {width, height, 1};
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.flags = VK_IMAGE_CREATE_ALIAS_BIT;
    imageCreateInfo.pNext = &externalMemoryImageCreateInfo;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

    vkCreateImage(context->logicalDevice, &imageCreateInfo, nullptr, &image);
    LOG_INFO("External Image created successfully now binding the memory");

    VkMemoryDedicatedAllocateInfo dedicatedAllocateInfo{};
    dedicatedAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicatedAllocateInfo.image = image;

    VkImportAndroidHardwareBufferInfoANDROID importInfo{};
    importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
    importInfo.buffer = buffer;
    importInfo.pNext = &dedicatedAllocateInfo;

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext = &importInfo;
    allocateInfo.allocationSize = bufferProperties.allocationSize;
    allocateInfo.memoryTypeIndex = find_memory_index_without_flags(context,
                                                                   bufferProperties.memoryTypeBits);
    vkAllocateMemory(context->logicalDevice, &allocateInfo, nullptr, &imageMemory);
    VkDebugUtilsObjectNameInfoEXT name{};
    name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    name.objectType = VK_OBJECT_TYPE_DEVICE_MEMORY;
    name.objectHandle = (uint64_t) imageMemory;
    name.pObjectName = "VideoFrameMemory";

    PFN_vkSetDebugUtilsObjectNameEXT setName = (PFN_vkSetDebugUtilsObjectNameEXT) vkGetDeviceProcAddr(
            context->logicalDevice, "vkSetDebugUtilsObjectNameEXT");
    setName(context->logicalDevice, &name);

    vkBindImageMemory(context->logicalDevice, image, imageMemory, 0);
    LOG_INFO("Image Create and Memory bound successfully");
}

inline void
create_y_cbcr_conversion_handler(VkDevice logicalDevice, VkSamplerYcbcrConversion &conversion,
                                 VkAndroidHardwareBufferFormatPropertiesANDROID formatProperties) {
    VkExternalFormatANDROID externalFormatAndroid{};
    externalFormatAndroid.sType = VK_STRUCTURE_TYPE_EXTERNAL_FORMAT_ANDROID;
    externalFormatAndroid.externalFormat = formatProperties.externalFormat;
    externalFormatAndroid.pNext = nullptr;


    VkSamplerYcbcrConversionCreateInfo conversionCreateInfo{};
    conversionCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO;
    conversionCreateInfo.pNext = &externalFormatAndroid;
    conversionCreateInfo.format = VK_FORMAT_UNDEFINED;
    conversionCreateInfo.ycbcrModel = formatProperties.suggestedYcbcrModel;
    conversionCreateInfo.ycbcrRange = formatProperties.suggestedYcbcrRange;
    conversionCreateInfo.xChromaOffset = formatProperties.suggestedXChromaOffset;
    conversionCreateInfo.yChromaOffset = formatProperties.suggestedYChromaOffset;
    conversionCreateInfo.chromaFilter = VK_FILTER_LINEAR;
    conversionCreateInfo.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY
    };
    conversionCreateInfo.forceExplicitReconstruction = VK_FALSE;

    LOG_INFO("Creating the YCBCR conversion");
    PFN_vkCreateSamplerYcbcrConversionKHR pfnVkCreateSamplerYcbcrConversion = (PFN_vkCreateSamplerYcbcrConversionKHR)
            vkGetDeviceProcAddr(logicalDevice, "vkCreateSamplerYcbcrConversionKHR");
    if (pfnVkCreateSamplerYcbcrConversion == VK_NULL_HANDLE || logicalDevice == VK_NULL_HANDLE) {
        LOG_INFO("Function pointer is null handle");
    }
    pfnVkCreateSamplerYcbcrConversion(
            logicalDevice,
            &conversionCreateInfo,
            nullptr,
            &conversion);
    LOG_INFO("YCBCR conversion created successfully");
}

inline void
create_image_view_hardware_image(VkDevice device, VkImage &image, VkImageView &imageView,
                                 VkSamplerYcbcrConversion &conversion) {

    VkSamplerYcbcrConversionInfo conversionInfo{};
    conversionInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO;
    conversionInfo.pNext = nullptr;
    conversionInfo.conversion = conversion;

    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.pNext = &conversionInfo;
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = VK_FORMAT_UNDEFINED;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;

    LOG_INFO("Creating the hard image image view");
    VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &imageView),
             "Failed to create the image view");
    LOG_INFO("Hardware image view created successfully");
}

inline void
create_image(GpuContext *context, VkImage &image, VkDeviceMemory& memory, VkImageUsageFlags usage,
             VkMemoryPropertyFlags memoryUsage,
             VkFormat format, uint32_t width, uint32_t height, const char *imageName) {
    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.format = format;
    imageCreateInfo.usage = usage;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.extent = {width, height, 1};
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VK_CHECK(vkCreateImage(context->logicalDevice, &imageCreateInfo, nullptr, &image),
             "Failed to create the image");
    LOG_INFO("%s image created successfully", imageName);
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(context->logicalDevice, image, &requirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = find_memory_index(context, requirements.memoryTypeBits,
                                                     memoryUsage);

    VK_CHECK(vkAllocateMemory(context->logicalDevice, &allocateInfo, nullptr, &memory),
             "Failed to allocate the memory for the image");
    LOG_INFO("The Memory allocate successfully for the Image %s", imageName);

    VkDebugUtilsObjectNameInfoEXT name{};
    name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    name.objectType = VK_OBJECT_TYPE_DEVICE_MEMORY;
    name.objectHandle = (uint64_t) memory;
    name.pObjectName = imageName;
    PFN_vkSetDebugUtilsObjectNameEXT setName = (PFN_vkSetDebugUtilsObjectNameEXT) vkGetDeviceProcAddr(
            context->logicalDevice, "vkSetDebugUtilsObjectNameEXT");
    setName(context->logicalDevice, &name);
    vkBindImageMemory(context->logicalDevice, image, memory, 0);
}

inline void
create_image_view(VkDevice device, VkImage &image, VkImageView &imageView, VkFormat format) {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;

    VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &imageView),
             "Failed to create the image view");
}


inline void create_buffer(GpuContext *context, VkBuffer &buffer, VkDeviceMemory &memory,
                          VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags memoryFlags,
                          VkDeviceSize size) {
    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = bufferUsage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(context->logicalDevice, &bufferCreateInfo, nullptr, &buffer),
             "Failed to create the buffer");
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(context->logicalDevice, buffer, &requirements);
    int32_t memoryIndex = find_memory_index(context, requirements.memoryTypeBits, memoryFlags);
    if (memoryIndex == -1) {
        LOG_INFO("failed to get any specified memory for the asset");
        std::exit(EXIT_FAILURE);
    }
    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.memoryTypeIndex = memoryIndex;
    allocateInfo.allocationSize = requirements.size;

    VK_CHECK(vkAllocateMemory(context->logicalDevice, &allocateInfo, nullptr, &memory),
             "Failed to allocate the memory for the asset");
    VkDebugUtilsObjectNameInfoEXT name{};
    name.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    name.objectType = VK_OBJECT_TYPE_DEVICE_MEMORY;
    name.objectHandle = (uint64_t) memory;
    name.pObjectName = "BufferMemory";

    PFN_vkSetDebugUtilsObjectNameEXT setName = (PFN_vkSetDebugUtilsObjectNameEXT) vkGetDeviceProcAddr(
            context->logicalDevice, "vkSetDebugUtilsObjectNameEXT");
    setName(context->logicalDevice, &name);
    vkBindBufferMemory(context->logicalDevice, buffer, memory, 0);

}

inline VkCommandBuffer start_command_buffer(GpuContext *context) {
    VkCommandBuffer commandBuffer{};
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    allocateInfo.commandPool = context->graphicsCommandPool;
    vkAllocateCommandBuffers(context->logicalDevice, &allocateInfo, &commandBuffer);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}

inline void submit_command_buffer(GpuContext *ctx, VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);
    VkFence fence{};
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(ctx->logicalDevice, &fenceCreateInfo, nullptr, &fence);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkQueueSubmit(ctx->graphicsQueue, 1, &submitInfo, fence);
    vkWaitForFences(ctx->logicalDevice, 1, &fence, VK_TRUE, UINT32_MAX);
    vkResetFences(ctx->logicalDevice, 1, &fence);
    vkDestroyFence(ctx->logicalDevice, fence, nullptr);

}

inline void submit_to_queue(GpuContext *context, VkCommandBuffer &commandBuffer) {
    vkEndCommandBuffer(commandBuffer);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkFence submitFence{};
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(context->logicalDevice, &fenceCreateInfo, nullptr, &submitFence);
    vkQueueSubmit(context->graphicsQueue, 1, &submitInfo, submitFence);
    vkWaitForFences(context->logicalDevice, 1, &submitFence, VK_TRUE, UINT64_MAX);
    vkResetFences(context->logicalDevice, 1, &submitFence);
    vkDestroyFence(context->logicalDevice, submitFence, nullptr);
}

inline void copy_buffer_to_buffer(GpuContext *context, VkBuffer &srcBuffer, VkBuffer &dstBuffer,
                                  VkDeviceSize size) {
    VkCommandBuffer commandBuffer = start_command_buffer(context);

    VkBufferCopy region{};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &region);
    submit_to_queue(context, commandBuffer);
}

inline void
read_shader_file_from_asset(GpuContext *ctx, const char *assetPath, std::vector<uint8_t> &code) {
    AAssetManager *assetManager = ctx->app->activity->assetManager;
    AAsset *asset = AAssetManager_open(assetManager, assetPath, AASSET_MODE_UNKNOWN);
    int length = AAsset_getLength(asset);
    code.resize(length);
    AAsset_read(asset, code.data(), length);
}

inline VkShaderModule create_shader_module(GpuContext *ctx, const char *shaderPath) {
    std::vector<uint8_t> shaderCode{};
    read_shader_file_from_asset(ctx, shaderPath, shaderCode);
    VkShaderModuleCreateInfo shaderModuleCreateInfo{};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = static_cast<uint32_t >(shaderCode.size());
    shaderModuleCreateInfo.pCode = reinterpret_cast<uint32_t *>(shaderCode.data());

    VkShaderModule shaderModule{};
    VkResult result = vkCreateShaderModule(ctx->logicalDevice,
                                           &shaderModuleCreateInfo,
                                           nullptr, &shaderModule);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create the shader Module");
    } else {
        LOG_INFO("Shader Module created successfully");
    }
    return shaderModule;
}

inline void
create_hardware_image_yuv_sampler(VkDevice logicalDevice, VkSampler &sampler,
                                  VkSamplerYcbcrConversion &conversion) {

    VkSamplerYcbcrConversionInfo conversionInfo{};
    conversionInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO;
    conversionInfo.conversion = conversion;

    VkSamplerCreateInfo samplerCreateInfo{};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.pNext = &conversionInfo;
    samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
    samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VK_CHECK(vkCreateSampler(logicalDevice, &samplerCreateInfo, nullptr, &sampler),
             "failed to create the sampler");
    LOG_INFO("Hardware sampler created successfully");
}

inline void create_sampler(VkDevice logicalDevice, VkSampler &sampler) {
    VkSamplerCreateInfo samplerCreateInfo{};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
    samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VK_CHECK(vkCreateSampler(logicalDevice, &samplerCreateInfo, nullptr, &sampler),
             "failed to create the sampler");

}

inline void record_transition_image(VkCommandBuffer commandBuffer, VkImage image,
                                    VkImageAspectFlags aspectFlags, VkImageLayout oldLayout,
                                    VkImageLayout newLayout, VkAccessFlags srcAccess,
                                    VkPipelineStageFlags srcStage,
                                    VkAccessFlags dstAccess, VkPipelineStageFlags dstStage,
                                    uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                    uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.aspectMask = aspectFlags;
    barrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
    barrier.dstQueueFamilyIndex = dstQueueFamilyIndex;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);
}

inline void
transition_image_layout(GpuContext *ctx, VkImage image,
                        VkImageAspectFlags aspectFlags, VkImageLayout oldLayout,
                        VkImageLayout newLayout, VkAccessFlags srcAccess,
                        VkPipelineStageFlags srcStage,
                        VkAccessFlags dstAccess, VkPipelineStageFlags dstStage,
                        uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                        uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.aspectMask = aspectFlags;
    barrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
    barrier.dstQueueFamilyIndex = dstQueueFamilyIndex;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    VkCommandBuffer commandBuffer = start_command_buffer(ctx);
    vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);
    submit_command_buffer(ctx, commandBuffer);
}

#endif //OSFEATURENDKDEMO_UTIL_H
