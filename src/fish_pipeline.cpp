#include "fish_pipeline.h"

#include <vk_initializers.h>

#include <fstream> // For load_shader_module function.


namespace Fish {
    namespace Pipeline {

        void Builder::clear()
        {
            // clear all of the structs we need back to 0 with their correct sType
            _inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
            _rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
            _colorBlendAttachment = {};
            _multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
            _pipelineLayout = {};
            _depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
            _renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
            _shaderStages.clear();
        }

        VkPipeline Builder::build_pipeline(VkDevice device)
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
            pipelineInfo.pNext = &_renderInfo; // connect the renderInfo to the pNext extension mechanism
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
            if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
                fmt::println("failed to create pipeline");
                return VK_NULL_HANDLE; // failed to create graphics pipeline
            }
            else {
                return newPipeline;
            }
        }

        void Builder::set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
        {
            _shaderStages.clear();

            _shaderStages.push_back(
                vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));

            _shaderStages.push_back(
                vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));
        }

        void Builder::set_input_topology(VkPrimitiveTopology topology)
        {
            _inputAssembly.topology = topology;
            _inputAssembly.primitiveRestartEnable = VK_FALSE; // tutorial is not going to use primitive restart
        }

        void Builder::set_polygon_mode(VkPolygonMode mode)
        {
            _rasterizer.polygonMode = mode;
            _rasterizer.lineWidth = 1.f;
        }

        void Builder::set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace)
        {
            _rasterizer.cullMode = cullMode;
            _rasterizer.frontFace = frontFace;
        }

        void Builder::set_multisampling_none()
        {
            _multisampling.sampleShadingEnable = VK_FALSE;            
            _multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // multisampling defaulted to no multisampling (1 sample per pixel)
            _multisampling.minSampleShading = 1.0f;
            _multisampling.pSampleMask = nullptr;            
            _multisampling.alphaToCoverageEnable = VK_FALSE; // no alpha to coverage either
            _multisampling.alphaToOneEnable = VK_FALSE;
        }

        void Builder::disable_blending()
        {
            // default write mask
            _colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;            
            _colorBlendAttachment.blendEnable = VK_FALSE;
        }

        void Builder::enable_blending_additive()
        {
            _colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            _colorBlendAttachment.blendEnable = VK_TRUE;
            _colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            _colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            _colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            _colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            _colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            _colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        }

        void Builder::enable_blending_alphablend()
        {
            _colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            _colorBlendAttachment.blendEnable = VK_TRUE;
            _colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            _colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            _colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            _colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            _colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            _colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        }

        void Builder::set_color_attachment_format(VkFormat format)
        {
            _colorAttachmentformat = format;
            _renderInfo.colorAttachmentCount = 1;
            _renderInfo.pColorAttachmentFormats = &_colorAttachmentformat;
        }

        void Builder::set_depth_format(VkFormat format)
        {
            _renderInfo.depthAttachmentFormat = format;
        }

        void Builder::disable_depthtest()
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

        void Builder::enable_depthtest(bool depthWriteEnable, VkCompareOp op)
        {
            _depthStencil.depthTestEnable = VK_TRUE;
            _depthStencil.depthWriteEnable = depthWriteEnable;
            _depthStencil.depthCompareOp = op;
            _depthStencil.depthBoundsTestEnable = VK_FALSE;
            _depthStencil.stencilTestEnable = VK_FALSE;
            _depthStencil.front = {};
            _depthStencil.back = {};
            _depthStencil.minDepthBounds = 0.f;
            _depthStencil.maxDepthBounds = 1.f;
        }
        
        // ------------------------------------------------------------------------------------------------------------------------------------------------------------------- //

        bool load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outShaderModule)
        {
            // Attempt to open the file, and set our file cursor to the end of the file.
            std::ifstream file(filePath, std::ios::ate | std::ios::binary);

            // Fail if we cannot open the file.
            if (!file.is_open()) {
                return false;
            }

            // Calculate the size of the file by looking up the location of the file cursor
            // and since our cursor is at the end, it gives us the size directly in bytes.
            size_t fileSize = (size_t)file.tellg();

            // Spir-V expects a buffer to be of uint32_t so reserve a vector big enough for the entire file.
            std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

            // Return our file cursor to the beginning of the file to allow us to read through it.
            file.seekg(0);

            // Read through the entire file and load that data into our buffer.
            file.read((char*)buffer.data(), fileSize);

            // After loading all our data, we can close the file.
            file.close();

            // Create a new shader module, using the buffer we loaded.
            VkShaderModuleCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.pNext = nullptr;
            createInfo.codeSize = buffer.size() * sizeof(uint32_t); // In bytes.
            createInfo.pCode = buffer.data();

            // Error check the creation of the shader module.
            VkShaderModule shaderModule;
            if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
                return false;
            }

            *outShaderModule = shaderModule;
            return true;
        }
    }
}
