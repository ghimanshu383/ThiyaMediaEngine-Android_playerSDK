//
// Created by ghima on 28-02-2026.
//

#ifndef OSFEATURENDKDEMO_LUMACHROMAIMAGEGENERATORCMP_H
#define OSFEATURENDKDEMO_LUMACHROMAIMAGEGENERATORCMP_H

#include "Util.h"

namespace ve {
    class LumaChromaImageGeneratorCmp {
    private:
        GpuContext *m_ctx;
        const char *m_shader_asset_name;
        VkPipeline m_compute_pipeline;
        VkPipelineLayout m_compute_pipeline_layout;
        VkDescriptorSet m_des_set;
        VkDescriptorSetLayout m_des_layout;
        VkDescriptorPool m_des_pool;
        VkSampler m_ycbcrSampler;
        VkSampler m_sampler;
        bool m_is_first_draw = true;

        void setup_descriptors();

        void setup_pipeline();

        void update_descriptors(VkImageView &hardwareImageView, VkImageView &lumaImageView,
                                VkImageView &chromaImageView);

    public:
        LumaChromaImageGeneratorCmp(GpuContext *context, const char *shaderAssetName);

        void record_compute(VkCommandBuffer &computeCommandBuffer, VkImageView &hardwareImage,
                            VkImageView &lumaImageView, VkImageView &chromaImageView,
                            ComputeInfoStruct &computeInfoStruct);

        void clean_up();
    };
}
#endif //OSFEATURENDKDEMO_LUMACHROMAIMAGEGENERATORCMP_H
