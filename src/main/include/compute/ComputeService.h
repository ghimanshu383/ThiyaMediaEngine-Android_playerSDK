//
// Created by ghima on 01-03-2026.
//

#ifndef OSFEATURENDKDEMO_COMPUTESERVICE_H
#define OSFEATURENDKDEMO_COMPUTESERVICE_H

#include "Util.h"

namespace ve {
    class ComputeService {
    private:
        static ComputeService *m_instance;
        int32_t m_compute_registered_count = 0;
        GpuContext *m_ctx;

        class LumaChromaImageGeneratorCmp *m_lumaChromaImageGeneratorCmp;

        ComputeService() = default;

    public:
        static ComputeService *get_instance(GpuContext *ctx);

        void init();

        void record_compute(VkCommandBuffer &buffer, VkImageView &hardwareImage,
                            VkImageView &lumaImageView, VkImageView &chromaImageView,
                            ComputeInfoStruct &infoStruct);

        void submit_compute(VkCommandBuffer &buffer, VkSemaphore &computeSemaphore);

        void clean_up();

        int32_t get_compute_registered_count() {
            return m_compute_registered_count;
        }

        void enabled_compute(bool isComputeTestEnable) {
            if (isComputeTestEnable) {
                m_compute_registered_count = 1;
            } else {
                m_compute_registered_count = 0;
            }
        }
    };
}
#endif //OSFEATURENDKDEMO_COMPUTESERVICE_H
