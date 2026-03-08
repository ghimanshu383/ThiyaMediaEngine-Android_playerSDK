//
// Created by ghima on 14-02-2026.
//
#include "GraphicsPipeline.h"
#include "VideoProcessor.h"
#include "compute/ComputeService.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_vulkan.h"
#include "imgui/imgui_impl_android.h"

namespace ve {
    GraphicsPipeline::GraphicsPipeline(GpuContext *context, SwapchainContext *swapchainContext)
            : m_gpuContext(context), m_swapchainContext(swapchainContext) {
        init();
    }

    void GraphicsPipeline::init() {
        m_frames.resize(MAX_FRAMES);
        for (int i = 0; i < MAX_FRAMES; i++) {
            m_frames[i].presentId = i;
        }
        create_render_pass();
        create_frame_buffers();
        create_fence_and_semaphores_and_allocate_command_buffer();
        setup_display_quad();

        create_imgui_descriptor_pool();
        create_ui_render_pass();
        create_ui_image_and_views_buffers();
        setup_imgui_context();
    }

    void GraphicsPipeline::create_render_pass() {
        VkAttachmentDescription colorAttachmentDescription{};
        colorAttachmentDescription.format = m_swapchainContext->surfaceFormat.format;
        colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;

        std::vector<VkAttachmentDescription> attachments{colorAttachmentDescription};

        VkAttachmentReference colorReference{};
        colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorReference.attachment = 0;

        std::vector<VkAttachmentReference> references{colorReference};
        VkSubpassDescription subpassOne{};
        subpassOne.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassOne.colorAttachmentCount = references.size();
        subpassOne.pColorAttachments = references.data();

        VkSubpassDependency dependencyOne{};
        dependencyOne.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencyOne.dstSubpass = 0;
        dependencyOne.srcAccessMask = 0;
        dependencyOne.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencyOne.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencyOne.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencyOne.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo renderPassCreateInfo{};
        renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCreateInfo.attachmentCount = attachments.size();
        renderPassCreateInfo.pAttachments = attachments.data();
        renderPassCreateInfo.subpassCount = 1;
        renderPassCreateInfo.pSubpasses = &subpassOne;
        renderPassCreateInfo.dependencyCount = 1;
        renderPassCreateInfo.pDependencies = &dependencyOne;


        VK_CHECK(
                vkCreateRenderPass(m_gpuContext->logicalDevice, &renderPassCreateInfo, nullptr,
                                   &m_render_pass),
                "Failed to create the render pass'");
        LOG_INFO("Render pass created successfully");

    }

    void GraphicsPipeline::create_frame_buffers() {
        m_frame_buffers.resize(m_swapchainContext->minImageCount);
        for (int i = 0; i < m_swapchainContext->minImageCount; i++) {
            std::vector<VkImageView> attachments{m_swapchainContext->swapchainImageViews[i]};
            VkFramebufferCreateInfo framebufferCreateInfo{};
            framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferCreateInfo.width = m_swapchainContext->windowExtents.width;
            framebufferCreateInfo.height = m_swapchainContext->windowExtents.height;
            framebufferCreateInfo.attachmentCount = attachments.size();
            framebufferCreateInfo.pAttachments = attachments.data();
            framebufferCreateInfo.renderPass = m_render_pass;
            framebufferCreateInfo.layers = 1;
            VK_CHECK(vkCreateFramebuffer(m_gpuContext->logicalDevice, &framebufferCreateInfo,
                                         nullptr,
                                         &m_frame_buffers[i]),
                     "Failed to create the frame buffers");
            LOG_INFO("Frame Buffers created Successfully");
        }
    }

    void GraphicsPipeline::create_pipeline() {
        LOG_INFO("Window Extends %d %d", m_swapchainContext->windowExtents.width,
                 m_swapchainContext->windowExtents.height);
        VkShaderModule vertexShaderModule = create_shader_module(m_gpuContext,
                                                                 "default_hardware.vert.spv");
        VkShaderModule fragShaderModule = create_shader_module(m_gpuContext,
                                                               "default_hardware.frag.spv");

        VkPipelineShaderStageCreateInfo vertexStage{};
        vertexStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertexStage.module = vertexShaderModule;
        vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexStage.pName = "main";
        VkPipelineShaderStageCreateInfo fragStage{};
        fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragShaderModule;
        fragStage.pName = "main";

        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{vertexStage, fragStage};

        std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT,
                                                    VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
        dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateCreateInfo.dynamicStateCount = dynamicStates.size();
        dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
        inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;


        VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
        rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
        rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
        rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
        rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
        // rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;;
        rasterizationStateCreateInfo.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo{};
        pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        pipelineMultisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
        pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;


        VkPushConstantRange pushRange{};
        pushRange.size = sizeof(FrameInfo);
        pushRange.offset = 0;
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkPipelineLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutCreateInfo.pushConstantRangeCount = 1;
        layoutCreateInfo.pPushConstantRanges = &pushRange;
        layoutCreateInfo.setLayoutCount = 1;
        layoutCreateInfo.pSetLayouts = &m_des_layout;

        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindingDescription.stride = sizeof(Vertex);

        VkVertexInputAttributeDescription pos{};
        pos.binding = 0;
        pos.location = 0;
        pos.format = VK_FORMAT_R32G32B32_SFLOAT;
        pos.offset = offsetof(Vertex, pos);

        VkVertexInputAttributeDescription uv{};
        uv.binding = 0;
        uv.location = 1;
        uv.format = VK_FORMAT_R32G32_SFLOAT;
        uv.offset = offsetof(Vertex, uv);

        std::vector<VkVertexInputAttributeDescription> attributes{pos, uv};

        VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{};
        vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
        vertexInputStateCreateInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputStateCreateInfo.vertexAttributeDescriptionCount = attributes.size();
        vertexInputStateCreateInfo.pVertexAttributeDescriptions = attributes.data();

        VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
        viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        m_view_port = {0, 0, static_cast<float>(m_swapchainContext->windowExtents.width),
                       static_cast<float>(m_swapchainContext->windowExtents.height), 0, 1};
        m_scissors = {0, 0, m_swapchainContext->windowExtents};
        viewportStateCreateInfo.viewportCount = 1;
        viewportStateCreateInfo.pViewports = &m_view_port;
        viewportStateCreateInfo.scissorCount = 1;
        viewportStateCreateInfo.pScissors = &m_scissors;

        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState blendOne{};
        blendOne.blendEnable = VK_FALSE;
        blendOne.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
        colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
        colorBlendStateCreateInfo.attachmentCount = 1;
        colorBlendStateCreateInfo.pAttachments = &blendOne;


        VK_CHECK(vkCreatePipelineLayout(m_gpuContext->logicalDevice, &layoutCreateInfo, nullptr,
                                        &m_graphics_pipeline_layout),
                 "Failed to create the graphics pipeline layout");
        LOG_INFO("Pipeline Layout Created Successfully");

        VkGraphicsPipelineCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        createInfo.renderPass = m_render_pass;
        createInfo.subpass = 0;
        createInfo.layout = m_graphics_pipeline_layout;
        createInfo.stageCount = shaderStages.size();
        createInfo.pStages = shaderStages.data();
        createInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
        createInfo.pRasterizationState = &rasterizationStateCreateInfo;
        createInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
        createInfo.pDynamicState = &dynamicStateCreateInfo;
        createInfo.pVertexInputState = &vertexInputStateCreateInfo;
        createInfo.pViewportState = &viewportStateCreateInfo;
        createInfo.pColorBlendState = &colorBlendStateCreateInfo;
        createInfo.pDepthStencilState = &depthStencil;

        VK_CHECK(vkCreateGraphicsPipelines(m_gpuContext->logicalDevice, nullptr, 1, &createInfo,
                                           nullptr,
                                           &m_graphics_pipeline),
                 "Failed to create the graphics pipeline");
        LOG_INFO("Graphics Pipeline Created Successfully");
        vkDestroyShaderModule(m_gpuContext->logicalDevice, vertexShaderModule, nullptr);
        vkDestroyShaderModule(m_gpuContext->logicalDevice, fragShaderModule, nullptr);
    }

    void GraphicsPipeline::create_fence_and_semaphores_and_allocate_command_buffer() {
        VkSemaphoreCreateInfo semaphoreCreateInfo{};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (int i = 0; i < MAX_FRAMES; i++) {

            vkCreateSemaphore(m_gpuContext->logicalDevice, &semaphoreCreateInfo, nullptr,
                              &m_frames[i].next_image_semaphore);
            vkCreateSemaphore(m_gpuContext->logicalDevice, &semaphoreCreateInfo, nullptr,
                              &m_frames[i].render_finish_semaphore);
            vkCreateSemaphore(m_gpuContext->logicalDevice, &semaphoreCreateInfo, nullptr,
                              &m_frames[i].compute_semaphore);
            vkCreateFence(m_gpuContext->logicalDevice, &fenceCreateInfo, nullptr,
                          &m_frames[i].render_finish_fence);

            VkCommandBufferAllocateInfo allocateInfo{};
            allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocateInfo.commandBufferCount = 1;
            allocateInfo.commandPool = m_gpuContext->graphicsCommandPool;
            allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

            vkAllocateCommandBuffers(m_gpuContext->logicalDevice, &allocateInfo,
                                     &m_frames[i].command_buffer);
            VkCommandBufferAllocateInfo allocateInfoCompute{};
            allocateInfoCompute.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocateInfoCompute.commandBufferCount = 1;
            allocateInfoCompute.commandPool = m_gpuContext->computeCommandPool;
            allocateInfoCompute.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

            vkAllocateCommandBuffers(m_gpuContext->logicalDevice, &allocateInfoCompute,
                                     &m_frames[i].command_buffer_compute);
        }

        LOG_INFO("Semaphores - fence for each frame and command buffers are ready");
    }

    void GraphicsPipeline::setup_display_quad() {
        std::vector<Vertex> vert{
                {{-1, -1, 0}, {0, 0}},
                {{-1, 1,  0}, {0, 1}},
                {{1,  1,  0}, {1, 1}},
                {{1,  -1, 0}, {1, 0}}
        };
        std::vector<std::uint32_t> indices{
                0, 1, 2,
                0, 2, 3
        };
        create_buffer(m_gpuContext, m_vertBufferStaging, m_vertBufferMemoryStaging,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      vert.size() * sizeof(Vertex));
        create_buffer(m_gpuContext, m_vertBuffer, m_vertBufferMemory,
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vert.size() * sizeof(Vertex));
        create_buffer(m_gpuContext, m_indexBufferStaging, m_indexBufferMemoryStaging,
                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                      indices.size() * sizeof(uint32_t));
        create_buffer(m_gpuContext, m_indexBuffer, m_indexBufferMemory,
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indices.size() * sizeof(uint32_t));

        void *data;
        vkMapMemory(m_gpuContext->logicalDevice, m_vertBufferMemoryStaging, 0,
                    vert.size() * sizeof(Vertex), 0, &data);
        memcpy(data, vert.data(), vert.size() * sizeof(Vertex));
        vkUnmapMemory(m_gpuContext->logicalDevice, m_vertBufferMemoryStaging);
        copy_buffer_to_buffer(m_gpuContext, m_vertBufferStaging, m_vertBuffer,
                              vert.size() * sizeof(Vertex));

        vkMapMemory(m_gpuContext->logicalDevice, m_indexBufferMemoryStaging, 0,
                    indices.size() * sizeof(uint32_t), 0, &data);
        memcpy(data, indices.data(), indices.size() * sizeof(uint32_t));
        vkUnmapMemory(m_gpuContext->logicalDevice, m_indexBufferMemoryStaging);
        copy_buffer_to_buffer(m_gpuContext, m_indexBufferStaging, m_indexBuffer,
                              indices.size() * sizeof(uint32_t));

        vkDestroyBuffer(m_gpuContext->logicalDevice, m_vertBufferStaging, nullptr);
        vkFreeMemory(m_gpuContext->logicalDevice, m_vertBufferMemoryStaging, nullptr);
        vkDestroyBuffer(m_gpuContext->logicalDevice, m_indexBufferStaging, nullptr);
        vkFreeMemory(m_gpuContext->logicalDevice, m_indexBufferMemoryStaging, nullptr);

        LOG_INFO("Display Quad Created Successfully");
    }

    bool GraphicsPipeline::begin_frame(HardwareBufferFrame *currFrame,
                                       AndroidHardwareBufferResource resource,
                                       FrameInfo &info) {
        if (currFrame == nullptr || m_frames.empty()) return false;

        m_curr_frame = m_frame_count % MAX_FRAMES;
        uint32_t queryBase = m_curr_frame * 4;

        vkWaitForFences(m_gpuContext->logicalDevice, 1, &m_frames[m_curr_frame].render_finish_fence,
                        VK_TRUE, UINT64_MAX);
        vkResetFences(m_gpuContext->logicalDevice, 1, &m_frames[m_curr_frame].render_finish_fence);
        VkResult result = vkGetQueryPoolResults(m_gpuContext->logicalDevice,
                                                m_gpuContext->gpuQueryPool, queryBase, 4,
                                                sizeof(m_timeStamps), m_timeStamps,
                                                sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

        double computeTime =
                ((m_timeStamps[1] - m_timeStamps[0]) * m_gpuContext->gpuTimeStampPeriod) / 1e6;
        double graphicsTime =
                ((m_timeStamps[3] - m_timeStamps[2]) * m_gpuContext->gpuTimeStampPeriod) / 1e6;
        m_gpu_time[0] = computeTime;
        m_gpu_time[1] = graphicsTime;

        VkResult acquireResult = vkAcquireNextImageKHR(m_gpuContext->logicalDevice,
                                                       m_swapchainContext->swapchain,
                                                       UINT64_MAX,
                                                       m_frames[m_curr_frame].next_image_semaphore,
                                                       nullptr, &m_curr_img_index);
        if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR ||
            acquireResult == VK_ERROR_SURFACE_LOST_KHR) {
            LOG_INFO("The window out of date KHR");
            on_window_rotation();
            return false;
        }
        if (m_curr_frame >= m_frames.size()) return false;

        if (m_frames[m_curr_frame].hbf != nullptr) {
            LOG_INFO("Freeing up %p %p", m_frames[m_curr_frame].hbf->buffer,
                     m_frames[m_curr_frame].hbf->image);
            AHardwareBuffer_release(m_frames[m_curr_frame].hbf->buffer);
            AImage_delete(m_frames[m_curr_frame].hbf->image);
            VideoProcessor::get_instance(nullptr)->dec_live_Images_count_with_notify();

            delete m_frames[m_curr_frame].hbf;
            m_frames[m_curr_frame].hbf = nullptr;
            m_frames[m_curr_frame].presentId = m_frame_count;
        }
        m_frames[m_curr_frame].hbf = std::move(currFrame);
        LOG_INFO("The Recent buff and img %p %p", m_frames[m_curr_frame].hbf->buffer,
                 m_frames[m_curr_frame].hbf->image);

        // Adding the compute state at this place for the different queues ( diff graphics and compute queue);
        if (ComputeService::get_instance(m_gpuContext)->get_compute_registered_count() > 0 &&
            m_gpuContext->computeQueueIndex != m_gpuContext->graphicsQueueIndex) {
            handle_compute_path_for_different_queues(resource);
        }

        VkCommandBuffer buffer = m_frames[m_curr_frame].command_buffer;
        vkResetCommandBuffer(buffer, 0);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(buffer, &beginInfo);
        info.computePath =
                ComputeService::get_instance(m_gpuContext)->get_compute_registered_count() > 0 ? 1
                                                                                               : 0;
        //Adding the ui layers
        add_ui_frame(buffer, info);
        // Configuring the command buffer for the compute query pool.
        // Checking for the gpu level timestamps for compute and graphics.
        vkCmdResetQueryPool(buffer, m_gpuContext->gpuQueryPool, queryBase, 4);
        // Transition the images after it is done, the luma chroma images shall always be in the layout general at this stage.
        if (ComputeService::get_instance(m_gpuContext)->get_compute_registered_count() > 0) {
            if (m_gpuContext->graphicsQueueIndex != m_gpuContext->computeQueueIndex) {
                record_transition_image(buffer, resource.lumaImage, VK_IMAGE_ASPECT_COLOR_BIT,
                                        VK_IMAGE_LAYOUT_GENERAL,
                                        VK_IMAGE_LAYOUT_GENERAL,
                                        0,
                                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                        VK_ACCESS_SHADER_READ_BIT,
                                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                        m_gpuContext->computeQueueIndex,
                                        m_gpuContext->graphicsQueueIndex);
                record_transition_image(buffer, resource.chromaImage, VK_IMAGE_ASPECT_COLOR_BIT,
                                        VK_IMAGE_LAYOUT_GENERAL,
                                        VK_IMAGE_LAYOUT_GENERAL,
                                        0,
                                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                        VK_ACCESS_SHADER_READ_BIT,
                                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                        m_gpuContext->computeQueueIndex,
                                        m_gpuContext->graphicsQueueIndex);
            } else {
                vkCmdWriteTimestamp(buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                    m_gpuContext->gpuQueryPool, queryBase + 0);
                ComputeInfoStruct infoStruct{0, 0};
                ComputeService::get_instance(m_gpuContext)->record_compute(buffer,
                                                                           resource.imageView,
                                                                           resource.lumaImageView,
                                                                           resource.chromaImageView,
                                                                           infoStruct);
                //  memory barriers for the luma and chroma images..
                record_transition_image(buffer, resource.lumaImage, VK_IMAGE_ASPECT_COLOR_BIT,
                                        VK_IMAGE_LAYOUT_GENERAL,
                                        VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT,
                                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                        VK_ACCESS_SHADER_READ_BIT,
                                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
                record_transition_image(buffer, resource.chromaImage, VK_IMAGE_ASPECT_COLOR_BIT,
                                        VK_IMAGE_LAYOUT_GENERAL,
                                        VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT,
                                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                        VK_ACCESS_SHADER_READ_BIT,
                                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
                vkCmdWriteTimestamp(buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                    m_gpuContext->gpuQueryPool, queryBase + 1);
            }

        }
        VkClearValue clearValue{};
        clearValue.color = {{.2f, 0.2f, 0.2f, 1.0f}};
        VkRenderPassBeginInfo renderPassBeginInfo{};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = m_render_pass;
        renderPassBeginInfo.framebuffer = m_frame_buffers[m_curr_img_index];
        renderPassBeginInfo.renderArea = {0, 0, m_swapchainContext->windowExtents};
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearValue;

        VkRect2D scissors = {0, 0, m_swapchainContext->windowExtents};
        VkViewport viewport{
                0, 0,
                (float) m_swapchainContext->windowExtents.width,
                (float) m_swapchainContext->windowExtents.height,
                0, 1
        };

        vkCmdWriteTimestamp(
                buffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                m_gpuContext->gpuQueryPool,
                queryBase + 2
        );
        vkCmdBeginRenderPass(buffer, &renderPassBeginInfo,
                             VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          m_graphics_pipeline);
        vkCmdSetViewport(buffer, 0, 1, &viewport);
        vkCmdSetScissor(buffer, 0, 1, &scissors);
        return true;
    }

    void GraphicsPipeline::draw(AndroidHardwareBufferResource &resource, FrameInfo &info) {
        VkCommandBuffer buffer = m_frames[m_curr_frame].command_buffer;
        update_descriptors(resource, m_curr_frame);

        VkDeviceSize offset{};
        vkCmdBindVertexBuffers(buffer, 0, 1, &m_vertBuffer, &offset);
        vkCmdBindIndexBuffer(buffer, m_indexBuffer, 0,
                             VK_INDEX_TYPE_UINT32);
        vkCmdPushConstants(buffer, m_graphics_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(FrameInfo), &info);
        vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_graphics_pipeline_layout, 0, 1, &m_set[m_curr_frame], 0,
                                nullptr);
        vkCmdDrawIndexed(buffer, 6, 1, 0, 0, 0);
    }

    void GraphicsPipeline::end_frame() {
        VkCommandBuffer buffer = m_frames[m_curr_frame].command_buffer;
        uint32_t queryBase = m_curr_frame * 4;
        vkCmdWriteTimestamp(
                buffer,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                m_gpuContext->gpuQueryPool,
                queryBase + 3
        );
        vkCmdEndRenderPass(buffer);
        vkEndCommandBuffer(buffer);

        std::vector<VkPipelineStageFlags> stageMask{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        std::vector<VkSemaphore> semaphores{m_frames[m_curr_frame].next_image_semaphore};
        if (ComputeService::get_instance(m_gpuContext)->get_compute_registered_count() > 0 &&
            m_gpuContext->graphicsQueueIndex != m_gpuContext->computeQueueIndex) {
            stageMask.push_back(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
            semaphores.push_back(m_frames[m_curr_frame].compute_semaphore);
        }
        VkSubmitInfo renderSubmitInfo{};
        renderSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        renderSubmitInfo.commandBufferCount = 1;
        renderSubmitInfo.pCommandBuffers = &buffer;
        renderSubmitInfo.waitSemaphoreCount = semaphores.size();
        renderSubmitInfo.pWaitSemaphores = semaphores.data();
        renderSubmitInfo.signalSemaphoreCount = 1;
        renderSubmitInfo.pSignalSemaphores = &m_frames[m_curr_frame].render_finish_semaphore;
        renderSubmitInfo.pWaitDstStageMask = stageMask.data();

        VkResult renderResult = vkQueueSubmit(m_gpuContext->graphicsQueue, 1, &renderSubmitInfo,
                                              m_frames[m_curr_frame].render_finish_fence);
        if (renderResult != VK_SUCCESS) {
            LOG_INFO("The render result is not ok");
        }

        int64_t timeToPresent;
        int64_t audioFrameTime = VideoProcessor::get_instance(
                nullptr)->get_audio_clock_frame_display_time(
                m_frames[m_curr_frame].hbf->pts * 1000L);
        if (audioFrameTime == -1) {
            timeToPresent = VideoProcessor::get_instance(nullptr)->get_vid_start_point() +
                            m_frames[m_curr_frame].hbf->pts * 1000L;
        } else {
            timeToPresent = audioFrameTime;
        }
        VkPresentTimeGOOGLE presentTimeGoogle{};
        presentTimeGoogle.presentID = m_frames[m_curr_frame].presentId;
        presentTimeGoogle.desiredPresentTime = timeToPresent;
        VkPresentTimesInfoGOOGLE presentTimeInfo{};
        presentTimeInfo.sType = VK_STRUCTURE_TYPE_PRESENT_TIMES_INFO_GOOGLE;
        presentTimeInfo.swapchainCount = 1;
        presentTimeInfo.pTimes = &presentTimeGoogle;

        VkPresentInfoKHR presentInfoKhr{};
        presentInfoKhr.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfoKhr.swapchainCount = 1;
        presentInfoKhr.pSwapchains = &m_swapchainContext->swapchain;
        presentInfoKhr.waitSemaphoreCount = 1;
        presentInfoKhr.pWaitSemaphores = &m_frames[m_curr_frame].render_finish_semaphore;
        presentInfoKhr.pImageIndices = &m_curr_img_index;
        presentInfoKhr.pNext = &presentTimeInfo;

        VkResult result = vkQueuePresentKHR(m_gpuContext->presentationQueue, &presentInfoKhr);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            LOG_ERROR("Recreating the Swapchain");
            on_window_rotation();
            return;
        }
    }

    void
    GraphicsPipeline::render(HardwareBufferFrame *frame,
                             AndroidHardwareBufferResource &resource) {
        if (m_is_first_render) {
            m_is_first_render = false;
            create_sampler(m_gpuContext->logicalDevice, m_default_sampler);
            create_hardware_image_yuv_sampler(m_gpuContext->logicalDevice, m_hardware_image_sampler,
                                              m_gpuContext->conversion);
            set_up_buffer_descriptors();
            create_pipeline();
            ComputeService::get_instance(m_gpuContext)->init();
        }
        FrameInfo info = VideoProcessor::get_instance(nullptr)->get_frame_info();
        if (begin_frame(frame, resource, info)) {
            draw(resource, info);
            end_frame();
        };
        m_frame_count++;
    }

    void GraphicsPipeline::clean_up() {
        for (VideoFrames &frame: m_frames) {
            vkDestroySemaphore(m_gpuContext->logicalDevice, frame.render_finish_semaphore, nullptr);
            vkDestroySemaphore(m_gpuContext->logicalDevice, frame.next_image_semaphore, nullptr);
            vkDestroyFence(m_gpuContext->logicalDevice, frame.render_finish_fence, nullptr);
            vkDestroySemaphore(m_gpuContext->logicalDevice, frame.compute_semaphore, nullptr);
            if (frame.hbf != nullptr) {
                AHardwareBuffer *buf = frame.hbf->buffer;
                AImage *img = frame.hbf->image;
                LOG_INFO("Cleaning up: %p, %p", buf, img);
                AHardwareBuffer_release(buf);
                AImage_delete(img);
                VideoProcessor::get_instance(nullptr)->dec_live_images_count();
                delete frame.hbf;
                frame.hbf = nullptr;
            }
        }

        vkDestroySampler(m_gpuContext->logicalDevice, m_hardware_image_sampler, nullptr);
        vkDestroySampler(m_gpuContext->logicalDevice, m_default_sampler, nullptr);

        vkDestroyDescriptorPool(m_gpuContext->logicalDevice, m_pool, nullptr);
        vkDestroyDescriptorSetLayout(m_gpuContext->logicalDevice, m_des_layout, nullptr);

        vkDestroyPipeline(m_gpuContext->logicalDevice, m_graphics_pipeline, nullptr);
        vkDestroyPipelineLayout(m_gpuContext->logicalDevice, m_graphics_pipeline_layout, nullptr);

        for (int i = 0; i < m_frame_buffers.size(); i++) {
            vkDestroyFramebuffer(m_gpuContext->logicalDevice, m_frame_buffers[i], nullptr);
        }

        vkDestroyRenderPass(m_gpuContext->logicalDevice, m_render_pass, nullptr);
        vkDestroyBuffer(m_gpuContext->logicalDevice, m_vertBuffer, nullptr);
        vkFreeMemory(m_gpuContext->logicalDevice, m_vertBufferMemory, nullptr);
        vkDestroyBuffer(m_gpuContext->logicalDevice, m_indexBuffer, nullptr);
        vkFreeMemory(m_gpuContext->logicalDevice, m_indexBufferMemory, nullptr);
        if (ComputeService::get_instance(m_gpuContext)->get_compute_registered_count() > 0) {
            ComputeService::get_instance(m_gpuContext)->clean_up();
        }
        clean_up_ui_context();
    }

    void GraphicsPipeline::on_window_rotation() {
        vkDeviceWaitIdle(m_gpuContext->logicalDevice);
        for (int i = 0; i < m_frame_buffers.size(); i++) {
            vkDestroyFramebuffer(m_gpuContext->logicalDevice, m_frame_buffers[i], nullptr);
        }
        VideoProcessor::get_instance(nullptr)->recreate_swapchain_context();
        create_frame_buffers();

        ImGui_ImplVulkan_Shutdown();
        for (int i = 0; i < m_swapchainContext->minImageCount; i++) {
            vkDestroyFramebuffer(m_gpuContext->logicalDevice, m_ui_frame_buffers[i], nullptr);
            vkDestroyImageView(m_gpuContext->logicalDevice, m_ui_image_views[i], nullptr);
            vkDestroyImage(m_gpuContext->logicalDevice, m_ui_images[i], nullptr);
            vkFreeMemory(m_gpuContext->logicalDevice, m_ui_images_memory[i], nullptr);
        }

        create_ui_image_and_views_buffers();
        setup_imgui_context();
    }

    void GraphicsPipeline::set_up_buffer_descriptors() {
        VkDescriptorSetLayoutBinding hardwareImageBinding{};
        hardwareImageBinding.binding = 0;
        hardwareImageBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        hardwareImageBinding.descriptorCount = 1;
        hardwareImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        hardwareImageBinding.pImmutableSamplers = &m_hardware_image_sampler;

        VkDescriptorSetLayoutBinding lumaChromaBinding{};
        lumaChromaBinding.binding = 1;
        lumaChromaBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        lumaChromaBinding.descriptorCount = 2;
        lumaChromaBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        lumaChromaBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding uiBinding{};
        uiBinding.binding = 2;
        uiBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        uiBinding.descriptorCount = 1;
        uiBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        uiBinding.pImmutableSamplers = nullptr;

        std::array<VkDescriptorSetLayoutBinding, 3> bindings{hardwareImageBinding,
                                                             lumaChromaBinding, uiBinding};
        VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCreateInfo.bindingCount = bindings.size();
        layoutCreateInfo.pBindings = bindings.data();

        vkCreateDescriptorSetLayout(m_gpuContext->logicalDevice, &layoutCreateInfo, nullptr,
                                    &m_des_layout);
        LOG_INFO("Hardware luma chroma descriptor layout created successfully");

        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 4 * MAX_FRAMES;

        VkDescriptorPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.poolSizeCount = 1;
        poolCreateInfo.pPoolSizes = &poolSize;
        poolCreateInfo.maxSets = MAX_FRAMES;
        vkCreateDescriptorPool(m_gpuContext->logicalDevice, &poolCreateInfo, nullptr, &m_pool);

        LOG_INFO("Hardware and luma Chroma des pool created successfully");

        m_set.resize(MAX_FRAMES);
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES, m_des_layout);

        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = m_pool;
        allocateInfo.descriptorSetCount = MAX_FRAMES;
        allocateInfo.pSetLayouts = layouts.data();

        vkAllocateDescriptorSets(m_gpuContext->logicalDevice, &allocateInfo, m_set.data());


        LOG_INFO("Hardware luma chroma descriptor set created successfully");
    }

    void
    GraphicsPipeline::update_descriptors(AndroidHardwareBufferResource &resource, int currFrame) {
        VkDescriptorImageInfo hardwareImageInfo{};
        hardwareImageInfo.sampler = VK_NULL_HANDLE;
        hardwareImageInfo.imageView = resource.imageView;
        hardwareImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet hardwareWrite{};
        hardwareWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        hardwareWrite.dstBinding = 0;
        hardwareWrite.dstSet = m_set[currFrame];
        hardwareWrite.descriptorCount = 1;
        hardwareWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        hardwareWrite.dstArrayElement = 0;
        hardwareWrite.pImageInfo = &hardwareImageInfo;

        VkDescriptorImageInfo lumaImageInfo{};
        lumaImageInfo.sampler = m_default_sampler;
        lumaImageInfo.imageView = resource.lumaImageView;
        lumaImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorImageInfo chromaImageInfo{};
        chromaImageInfo.sampler = m_default_sampler;
        chromaImageInfo.imageView = resource.chromaImageView;
        chromaImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        std::array<VkDescriptorImageInfo, 2> lumaChromaInfo{lumaImageInfo, chromaImageInfo};
        VkWriteDescriptorSet lumaChromaWrite{};
        lumaChromaWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        lumaChromaWrite.descriptorCount = lumaChromaInfo.size();
        lumaChromaWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        lumaChromaWrite.dstArrayElement = 0;
        lumaChromaWrite.dstBinding = 1;
        lumaChromaWrite.dstSet = m_set[currFrame];
        lumaChromaWrite.pImageInfo = lumaChromaInfo.data();

        VkDescriptorImageInfo uiImage{};
        uiImage.imageView = m_ui_image_views[m_curr_img_index];
        uiImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        uiImage.sampler = m_default_sampler;

        VkWriteDescriptorSet writeUI{};
        writeUI.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeUI.dstSet = m_set[currFrame];
        writeUI.descriptorCount = 1;
        writeUI.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeUI.dstArrayElement = 0;
        writeUI.dstBinding = 2;
        writeUI.pImageInfo = &uiImage;

        std::array<VkWriteDescriptorSet, 3> writes{hardwareWrite, lumaChromaWrite, writeUI};

        vkUpdateDescriptorSets(m_gpuContext->logicalDevice, writes.size(), writes.data(), 0,
                               nullptr);
    }

    void GraphicsPipeline::setup_imgui_context() {
        ImGui_ImplVulkan_InitInfo init_info{};
        init_info.Instance = m_gpuContext->instance;
        init_info.PhysicalDevice = m_gpuContext->physicalDevice;
        init_info.Device = m_gpuContext->logicalDevice;
        init_info.QueueFamily = m_gpuContext->graphicsQueueIndex;
        init_info.Queue = m_gpuContext->graphicsQueue;
        init_info.DescriptorPool = m_imgui_des_pool;
        init_info.MinImageCount = m_swapchainContext->minImageCount;
        init_info.ImageCount = m_swapchainContext->minImageCount;
        init_info.RenderPass = m_ui_render_pass;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        ImGui_ImplVulkan_Init(&init_info);
    }

    void GraphicsPipeline::create_imgui_descriptor_pool() {
        VkDescriptorPoolSize pool_sizes[] =
                {
                        {VK_DESCRIPTOR_TYPE_SAMPLER,                10},
                        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10},
                        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          10},
                        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          10},
                        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   10},
                        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   10},
                        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         10},
                        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         10},
                        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10},
                        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 10},
                        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       10}
                };

        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 10 * 11;
        pool_info.poolSizeCount = 11;
        pool_info.pPoolSizes = pool_sizes;

        vkCreateDescriptorPool(m_gpuContext->logicalDevice, &pool_info, nullptr, &m_imgui_des_pool);

        //  IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplAndroid_Init(m_gpuContext->window);
    }

    void GraphicsPipeline::create_ui_render_pass() {
        VkAttachmentDescription colorAttachmentDescription{};
        colorAttachmentDescription.format = VK_FORMAT_R8G8B8A8_UNORM;
        colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;

        VkAttachmentReference colorRef{};
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorRef.attachment = 0;

        VkSubpassDescription passOne{};
        passOne.colorAttachmentCount = 1;
        passOne.pColorAttachments = &colorRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;

        dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo uiRenderpassCreateInfo{};
        uiRenderpassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        uiRenderpassCreateInfo.attachmentCount = 1;
        uiRenderpassCreateInfo.pAttachments = &colorAttachmentDescription;
        uiRenderpassCreateInfo.subpassCount = 1;
        uiRenderpassCreateInfo.pSubpasses = &passOne;
        uiRenderpassCreateInfo.dependencyCount = 1;
        uiRenderpassCreateInfo.pDependencies = &dependency;

        VK_CHECK(vkCreateRenderPass(m_gpuContext->logicalDevice, &uiRenderpassCreateInfo, nullptr,
                                    &m_ui_render_pass),
                 "Failed to create the ui render pass");

    }

    void GraphicsPipeline::create_ui_image_and_views_buffers() {
        m_ui_images.resize(m_swapchainContext->minImageCount);
        m_ui_image_views.resize(m_swapchainContext->minImageCount);
        m_ui_images_memory.resize(m_swapchainContext->minImageCount);
        m_ui_frame_buffers.resize(m_swapchainContext->minImageCount);
        for (int i = 0; i < m_swapchainContext->minImageCount; i++) {
            create_image(m_gpuContext, m_ui_images[i], m_ui_images_memory[i],
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_FORMAT_R8G8B8A8_UNORM,
                         m_swapchainContext->windowExtents.width,
                         m_swapchainContext->windowExtents.height,
                         "Imgui Ui Image");
            create_image_view(m_gpuContext->logicalDevice, m_ui_images[i], m_ui_image_views[i],
                              VK_FORMAT_R8G8B8A8_UNORM);

            VkFramebufferCreateInfo framebufferCreateInfo{};
            framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferCreateInfo.width = m_swapchainContext->windowExtents.width;
            framebufferCreateInfo.height = m_swapchainContext->windowExtents.height;
            framebufferCreateInfo.renderPass = m_ui_render_pass;
            framebufferCreateInfo.attachmentCount = 1;
            framebufferCreateInfo.layers = 1;
            framebufferCreateInfo.pAttachments = &m_ui_image_views[i];
            VK_CHECK(vkCreateFramebuffer(m_gpuContext->logicalDevice, &framebufferCreateInfo,
                                         nullptr,
                                         &m_ui_frame_buffers[i]),
                     "Failed to create the ui frame buffer");
            LOG_INFO("UI Frame Buffers and Images created");
        }

    }

    void GraphicsPipeline::add_ui_frame(VkCommandBuffer &buffer, FrameInfo &info) {
        //Recording the ui commands the buffer is already started in the layer.

        if (ImGui::GetCurrentContext() == nullptr) {
            LOG_ERROR("ImGui Context is NULL before NewFrame!");
            return;
        }
        add_ui_frame_stats(info);

        VkClearValue value{};
        value.color = {{0, 0, 0, 0}};
        VkRenderPassBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.renderPass = m_ui_render_pass;
        beginInfo.clearValueCount = 1;
        beginInfo.renderArea = {{0, 0}, m_swapchainContext->windowExtents};
        beginInfo.pClearValues = &value;
        beginInfo.framebuffer = m_ui_frame_buffers[m_curr_img_index];
        vkCmdBeginRenderPass(buffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), buffer);
        vkCmdEndRenderPass(buffer);

        record_transition_image(buffer, m_ui_images[m_curr_img_index], VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    }


    void GraphicsPipeline::add_ui_frame_stats(FrameInfo &info) {
        ImGuiIO &io = ImGui::GetIO();
        io.FontGlobalScale = 1.5f;

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplAndroid_NewFrame();

        ImGuiStyle &style = ImGui::GetStyle();
        style.WindowRounding = 8.0f;
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.1f, 0.1f, 0.1f, 0.7f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.2f, 0.4f, 0.8f, 1.0f);

        ImGui::NewFrame();

        ImGui::SetNextWindowSize(ImVec2(450.0f, 0), ImGuiCond_Always);
//        float dynamicWidth = io.DisplaySize.x * 0.40f;
//        ImGui::SetNextWindowSize(ImVec2(dynamicWidth, 0), ImGuiCond_Always);

        ImGui::Begin("Thiya Media Engine", nullptr,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Performance");
        ImGui::Separator();
        float fps = ImGui::GetIO().Framerate;
        ImGui::Text("FPS %.2f", fps);
        float frame_time = fps > 0.0f ? 1000.0f / fps : 0.0f;
        ImGui::Text("Frame Time %.3f", frame_time);

        ImGui::Separator();
        static bool isCompute = false;
        ImGui::Text("Video");
        ImGui::BulletText("Resolution :  %d x %d", info.width, info.height);
        if (ImGui::Checkbox("Enable Compute Pipeline", &isCompute)) {
            ComputeService::get_instance(m_gpuContext)->enabled_compute(isCompute);
        }
        ImGui::BulletText("Compute: %s", info.computePath ? "ON" : "OFF");

        ImGui::Separator();
        ImGui::Text("GPU");
        ImGui::BulletText("Device %s", m_gpuContext->deviceName.c_str());
        ImGui::BulletText("Compute Time: %.3f", m_gpu_time[0]);
        ImGui::BulletText("Graphics Time: %.3f", m_gpu_time[1]);
        ImGui::BulletText("Graphics Queue: %d", m_gpuContext->graphicsQueueIndex);
        ImGui::BulletText("Compute Queue: %d", m_gpuContext->computeQueueIndex);
        ImGui::End();

        ImGui::Render();
    }

    void GraphicsPipeline::handle_compute_path_for_different_queues(
            AndroidHardwareBufferResource &resource) {
        VkCommandBuffer computeBuffer = m_frames[m_curr_frame].command_buffer_compute;
        vkResetCommandBuffer(computeBuffer, 0);
        VkCommandBufferBeginInfo beginComputeInfo{};
        beginComputeInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(computeBuffer, &beginComputeInfo);

        ComputeInfoStruct infoStruct{0, 0};
        ComputeService::get_instance(m_gpuContext)->record_compute(computeBuffer,
                                                                   resource.imageView,
                                                                   resource.lumaImageView,
                                                                   resource.chromaImageView,
                                                                   infoStruct);
        record_transition_image(computeBuffer, resource.lumaImage, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                                VK_ACCESS_SHADER_WRITE_BIT,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                0, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                m_gpuContext->computeQueueIndex,
                                m_gpuContext->graphicsQueueIndex);
        record_transition_image(computeBuffer, resource.chromaImage, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                                VK_ACCESS_SHADER_WRITE_BIT,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                0, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                m_gpuContext->computeQueueIndex,
                                m_gpuContext->graphicsQueueIndex);
        ComputeService::get_instance(m_gpuContext)->submit_compute(computeBuffer,
                                                                   m_frames[m_curr_frame].compute_semaphore);
    }

    void GraphicsPipeline::clean_up_ui_context() {
        ImGui_ImplVulkan_Shutdown();
        for (int i = 0; i < m_swapchainContext->minImageCount; i++) {
            vkDestroyFramebuffer(m_gpuContext->logicalDevice, m_ui_frame_buffers[i], nullptr);
            vkDestroyImageView(m_gpuContext->logicalDevice, m_ui_image_views[i], nullptr);
            vkDestroyImage(m_gpuContext->logicalDevice, m_ui_images[i], nullptr);
            vkFreeMemory(m_gpuContext->logicalDevice, m_ui_images_memory[i], nullptr);
        }
        vkDestroyRenderPass(m_gpuContext->logicalDevice, m_ui_render_pass, nullptr);
        vkDestroyDescriptorPool(m_gpuContext->logicalDevice, m_imgui_des_pool, nullptr);
        ImGui_ImplAndroid_Shutdown();
        ImGui::DestroyContext();
    }
}