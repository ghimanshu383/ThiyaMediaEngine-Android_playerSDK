//
// Created by ghima on 28-02-2026.
//
#include "compute/LumaChromaImageGeneratorCmp.h"
#include "VideoProcessor.h"

namespace ve {
    LumaChromaImageGeneratorCmp::LumaChromaImageGeneratorCmp(GpuContext *context,
                                                             const char *shaderAssetName) : m_ctx{
            context}, m_shader_asset_name{shaderAssetName} {
    }

    void LumaChromaImageGeneratorCmp::setup_descriptors() {
        VkDescriptorSetLayoutBinding bindingHardwareImage{};
        bindingHardwareImage.binding = 0;
        bindingHardwareImage.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindingHardwareImage.descriptorCount = 1;
        bindingHardwareImage.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindingHardwareImage.pImmutableSamplers = &m_ycbcrSampler;

        VkDescriptorSetLayoutBinding bindingLumaImage{};
        bindingLumaImage.binding = 1;
        bindingLumaImage.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindingLumaImage.descriptorCount = 1;
        bindingLumaImage.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindingLumaImage.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding bindingChromaImage{};
        bindingChromaImage.binding = 2;
        bindingChromaImage.descriptorCount = 1;
        bindingChromaImage.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindingChromaImage.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        bindingChromaImage.pImmutableSamplers = nullptr;

        std::array<VkDescriptorSetLayoutBinding, 3> bindings{bindingHardwareImage, bindingLumaImage,
                                                             bindingChromaImage};
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCreateInfo.bindingCount = bindings.size();
        layoutCreateInfo.pBindings = bindings.data();

        VK_CHECK(vkCreateDescriptorSetLayout(m_ctx->logicalDevice, &layoutCreateInfo, nullptr,
                                             &m_des_layout),
                 "Failed to create the des layout for the luma chroma generator");
        LOG_INFO("Luma and chroma set layout create successfully");
        VkDescriptorPoolSize sizeOne{};
        sizeOne.descriptorCount = 1;
        sizeOne.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        VkDescriptorPoolSize sizeTwo{};
        sizeTwo.descriptorCount = 2;
        sizeTwo.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        std::array<VkDescriptorPoolSize, 2> poolSizes{sizeOne, sizeTwo};
        VkDescriptorPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.poolSizeCount = poolSizes.size();
        poolCreateInfo.pPoolSizes = poolSizes.data();
        poolCreateInfo.maxSets = 1;
        poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        VK_CHECK(
                vkCreateDescriptorPool(m_ctx->logicalDevice, &poolCreateInfo, nullptr, &m_des_pool),
                "Failed to create the descriptor pool for the luma chroma generator");
        LOG_INFO("Descriptor Pool created for the luma chroma generator");
        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.pSetLayouts = &m_des_layout;
        allocateInfo.descriptorSetCount = 1;
        allocateInfo.descriptorPool = m_des_pool;
        vkAllocateDescriptorSets(m_ctx->logicalDevice, &allocateInfo, &m_des_set);
        LOG_INFO("The Descriptor sets allocate successfully for luma chroma generators");
    }

    void LumaChromaImageGeneratorCmp::setup_pipeline() {
        VkShaderModule module = create_shader_module(m_ctx, m_shader_asset_name);
        VkPipelineShaderStageCreateInfo computeStageCreateInfo{};
        computeStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        computeStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        computeStageCreateInfo.module = module;
        computeStageCreateInfo.pName = "main";

        VkPushConstantRange pushRange{};
        pushRange.size = sizeof(ComputeInfoStruct);
        pushRange.offset = 0;
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkPipelineLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutCreateInfo.pushConstantRangeCount = 1;
        layoutCreateInfo.pPushConstantRanges = &pushRange;
        layoutCreateInfo.setLayoutCount = 1;
        layoutCreateInfo.pSetLayouts = &m_des_layout;

        VkResult result = vkCreatePipelineLayout(m_ctx->logicalDevice, &layoutCreateInfo,
                                                 nullptr, &m_compute_pipeline_layout);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to create the pipeline layout for LumaChromaGenerator");
            return;
        }

        VkComputePipelineCreateInfo computePipelineCreateInfo{};
        computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineCreateInfo.basePipelineIndex = 0;
        computePipelineCreateInfo.stage = computeStageCreateInfo;
        computePipelineCreateInfo.layout = m_compute_pipeline_layout;

        VK_CHECK(vkCreateComputePipelines(m_ctx->logicalDevice, nullptr, 1,
                                          &computePipelineCreateInfo,
                                          nullptr, &m_compute_pipeline),
                 "Failed to create the compute pipeline for LumaChromaGenerator");
        LOG_INFO("The Pipeline for the compute luma chroma generated successfully");
        vkDestroyShaderModule(m_ctx->logicalDevice, module, nullptr);
    }

    void LumaChromaImageGeneratorCmp::update_descriptors(VkImageView &hardwareImageView,
                                                         VkImageView &lumaImageView,
                                                         VkImageView &chromaImageView) {
        VkDescriptorImageInfo hardwareImageInfo{};
        hardwareImageInfo.sampler = nullptr;
        hardwareImageInfo.imageView = hardwareImageView;
        hardwareImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo lumaImageInfo{};
        lumaImageInfo.sampler = nullptr;
        lumaImageInfo.imageView = lumaImageView;
        lumaImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorImageInfo chromaImageInfo{};
        chromaImageInfo.sampler = nullptr;
        chromaImageInfo.imageView = chromaImageView;
        chromaImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet hardwareWrite{};
        hardwareWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        hardwareWrite.descriptorCount = 1;
        hardwareWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        hardwareWrite.dstBinding = 0;
        hardwareWrite.dstArrayElement = 0;
        hardwareWrite.dstSet = m_des_set;
        hardwareWrite.pImageInfo = &hardwareImageInfo;

        VkWriteDescriptorSet lumaWrite{};
        lumaWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lumaWrite.descriptorCount = 1;
        lumaWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        lumaWrite.dstBinding = 1;
        lumaWrite.dstSet = m_des_set;
        lumaWrite.dstArrayElement = 0;
        lumaWrite.pImageInfo = &lumaImageInfo;

        VkWriteDescriptorSet chromaWrite{};
        chromaWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        chromaWrite.descriptorCount = 1;
        chromaWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        chromaWrite.dstBinding = 2;
        chromaWrite.dstSet = m_des_set;
        chromaWrite.dstArrayElement = 0;
        chromaWrite.pImageInfo = &chromaImageInfo;

        std::array<VkWriteDescriptorSet, 3> writes{hardwareWrite, lumaWrite, chromaWrite};
        vkUpdateDescriptorSets(m_ctx->logicalDevice, writes.size(), writes.data(), 0, nullptr);

    }

    void LumaChromaImageGeneratorCmp::record_compute(VkCommandBuffer &computeCommandBuffer,
                                                     VkImageView &hardwareImage,
                                                     VkImageView &lumaImageView,
                                                     VkImageView &chromaImageView,
                                                     ComputeInfoStruct &computeInfoStruct) {
        if (m_is_first_draw) {
            m_is_first_draw = false;
            create_hardware_image_yuv_sampler(m_ctx->logicalDevice, m_ycbcrSampler,
                                              m_ctx->conversion);
            create_sampler(m_ctx->logicalDevice, m_sampler);
            setup_descriptors();
            setup_pipeline();
        }
        vkCmdBindPipeline(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_compute_pipeline);
        update_descriptors(hardwareImage, lumaImageView, chromaImageView);
        vkCmdPushConstants(computeCommandBuffer, m_compute_pipeline_layout,
                           VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputeInfoStruct), &computeInfoStruct);
        vkCmdBindDescriptorSets(computeCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                m_compute_pipeline_layout, 0, 1, &m_des_set, 0,
                                nullptr);
        vkCmdDispatch(computeCommandBuffer,
                      (VideoProcessor::get_instance(nullptr)->get_frame_width() + 15) / 16,
                      (VideoProcessor::get_instance(nullptr)->get_frame_height() + 15) / 16,
                      1);
    }

    void LumaChromaImageGeneratorCmp::clean_up() {
        vkDestroyDescriptorPool(m_ctx->logicalDevice, m_des_pool, nullptr);
        vkDestroyDescriptorSetLayout(m_ctx->logicalDevice, m_des_layout, nullptr);
        vkDestroyPipelineLayout(m_ctx->logicalDevice, m_compute_pipeline_layout, nullptr);
        vkDestroyPipeline(m_ctx->logicalDevice, m_compute_pipeline, nullptr);
        vkDestroySampler(m_ctx->logicalDevice, m_sampler, nullptr);
        vkDestroySampler(m_ctx->logicalDevice, m_ycbcrSampler, nullptr);
    }
}