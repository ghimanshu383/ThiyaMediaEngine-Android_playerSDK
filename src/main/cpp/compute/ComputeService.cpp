//
// Created by ghima on 01-03-2026.
//
#include "compute/ComputeService.h"
#include "compute/LumaChromaImageGeneratorCmp.h"

namespace ve {
    ComputeService *ComputeService::m_instance = nullptr;

    ComputeService *ComputeService::get_instance(GpuContext *ctx) {
        if (m_instance == nullptr) {
            m_instance = new ComputeService();
            m_instance->m_ctx = ctx;
        }
        return m_instance;
    }

    void ComputeService::init() {
        m_lumaChromaImageGeneratorCmp = new LumaChromaImageGeneratorCmp(m_ctx,
                                                                        "luma_chroma_gen.comp.spv");
    }

    void ComputeService::record_compute(VkCommandBuffer &buffer, VkImageView &hardwareImage,
                                        VkImageView &lumaImageView, VkImageView &chromaImageView,
                                        ComputeInfoStruct &infoStruct) {
        m_lumaChromaImageGeneratorCmp->record_compute(buffer, hardwareImage, lumaImageView,
                                                      chromaImageView, infoStruct);
    }

    void ComputeService::submit_compute(VkCommandBuffer &buffer, VkSemaphore &computeSemaphore) {
        vkEndCommandBuffer(buffer);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &buffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &computeSemaphore;

        vkQueueSubmit(m_ctx->computeQueue, 1, &submitInfo, nullptr);
    }

    void ComputeService::clean_up() {
        if (m_lumaChromaImageGeneratorCmp) {
            m_lumaChromaImageGeneratorCmp->clean_up();
            delete m_lumaChromaImageGeneratorCmp;
        }
    }
}