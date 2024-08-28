#include "fish_pipeline.h"
#include <vk_initializers.h>

void Fish::PipelineBuilder13::clear()
{
    // clear all of the structs we need back to 0 with their correct stype

    _inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };

    _rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };

    _colorBlendAttachment = {};

    _multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };

    _pipelineLayout = {};

    _depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

    _renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

    _shaderStages.clear();
}

VkPipeline Fish::PipelineBuilder13::build_pipeline(VkDevice device)
{
    // make viewport state from our stored viewport and scissor.
    // at the moment we wont support multiple viewports or scissors
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;

    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // setup dummy color blending. We arent using transparent objects yet
    // the blending is just "no blend", but we do write to the color attachment
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext = nullptr;

    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &_colorBlendAttachment;

    // completely clear VertexInputStateCreateInfo, as we have no need for it
    VkPipelineVertexInputStateCreateInfo _vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

    // build the actual pipeline
    // we now use all of the info structs we have been writing into into this one
    // to create the pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    // connect the renderInfo to the pNext extension mechanism
    pipelineInfo.pNext = &_renderInfo;

    pipelineInfo.stageCount = (uint32_t)_shaderStages.size();
    pipelineInfo.pStages = _shaderStages.data();
    pipelineInfo.pVertexInputState = &_vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &_inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &_rasterizer;
    pipelineInfo.pMultisampleState = &_multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &_depthStencil;
    pipelineInfo.layout = _pipelineLayout;

    // Dynamic state configuration
    VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dynamicInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicInfo.pDynamicStates = &state[0];
    dynamicInfo.dynamicStateCount = 2;

    pipelineInfo.pDynamicState = &dynamicInfo;

    // Actually create and return
    // its easy to error out on create graphics pipeline, so we handle it a bit
    // better than the common VK_CHECK case
    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
        nullptr, &newPipeline)
        != VK_SUCCESS) {
        fmt::println("failed to create pipeline");
        return VK_NULL_HANDLE; // failed to create graphics pipeline
    }
    else {
        return newPipeline;
    }
}

void Fish::PipelineBuilder13::set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
    _shaderStages.clear();

    _shaderStages.push_back(
        vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));

    _shaderStages.push_back(
        vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));
}

void Fish::PipelineBuilder13::set_input_topology(VkPrimitiveTopology topology)
{
    _inputAssembly.topology = topology;
    // we are not going to use primitive restart on the entire tutorial so leave
    // it on false
    _inputAssembly.primitiveRestartEnable = VK_FALSE;
}

void Fish::PipelineBuilder13::set_polygon_mode(VkPolygonMode mode)
{
    _rasterizer.polygonMode = mode;
    _rasterizer.lineWidth = 1.f;
}

void Fish::PipelineBuilder13::set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace)
{
    _rasterizer.cullMode = cullMode;
    _rasterizer.frontFace = frontFace;
}

void Fish::PipelineBuilder13::set_multisampling_none()
{
    _multisampling.sampleShadingEnable = VK_FALSE;
    // multisampling defaulted to no multisampling (1 sample per pixel)
    _multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    _multisampling.minSampleShading = 1.0f;
    _multisampling.pSampleMask = nullptr;
    // no alpha to coverage either
    _multisampling.alphaToCoverageEnable = VK_FALSE;
    _multisampling.alphaToOneEnable = VK_FALSE;
}

void Fish::PipelineBuilder13::disable_blending()
{
    // default write mask
    _colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    // no blending
    _colorBlendAttachment.blendEnable = VK_FALSE;
}

void Fish::PipelineBuilder13::set_color_attachment_format(VkFormat format)
{
    _colorAttachmentformat = format;
    // connect the format to the renderInfo  structure
    _renderInfo.colorAttachmentCount = 1;
    _renderInfo.pColorAttachmentFormats = &_colorAttachmentformat;
}

void Fish::PipelineBuilder13::set_depth_format(VkFormat format)
{
    _renderInfo.depthAttachmentFormat = format;
}

void Fish::PipelineBuilder13::disable_depthtest()
{
    _depthStencil.depthTestEnable = VK_FALSE;
    _depthStencil.depthWriteEnable = VK_FALSE;
    _depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    _depthStencil.depthBoundsTestEnable = VK_FALSE;
    _depthStencil.stencilTestEnable = VK_FALSE;
    _depthStencil.front = {};
    _depthStencil.back = {};
    _depthStencil.minDepthBounds = 0.f;
    _depthStencil.maxDepthBounds = 1.f;
}
