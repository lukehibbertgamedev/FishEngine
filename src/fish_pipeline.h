#pragma once

// name: fish_pipeline.h
// desc: An abstraction of the pipeline creation process.
// desc: Most of the code is modified from the Vulkan Guide by vblanco20-1 on Git.
// auth: Luke Hibbert

#include <vk_types.h>

namespace Fish {
    namespace Pipeline {

        // An abstraction of the pipeline creation process to make things a little easier.
        // Holds all configurable data for a single pipeline.
        class Builder {
        public:
            Builder() { clear(); }                                                          // Constructor which zero-initialises all info structures.
            void clear();                                                                   // Clear all info structures back to their original structure type (zero-initialise them).

            VkPipeline build_pipeline(VkDevice device);                                     // Use all configured settings to create our pipeline.

            void set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);   // Clear and bind shader module parameters to the pipeline.
            void set_input_topology(VkPrimitiveTopology topology);                          // Set pipeline topology.
            void set_polygon_mode(VkPolygonMode mode);                                      // Set pipeline polygon mode.
            void set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace);            // Set pipeline cull mode.
            void set_multisampling_none();                                                  // Configure multisampling settings to ignore multisampling.
            void disable_blending();                                                        // Configure blending settings to ignore blending.
            void enable_blending_additive();                                                // Configure blending settings to enable additive-style blending.
            void enable_blending_alphablend();                                              // Configure blending settings to enable alpha-blending.
            void set_color_attachment_format(VkFormat format);                              // Connect the format to the VkPipelineRenderingCreateInfo structure.
            void set_depth_format(VkFormat format);                                         // Set the depth format member.
            void disable_depthtest();                                                       // Configure depth settings to ignore depth testing.
            void enable_depthtest(bool depthWriteEnable, VkCompareOp op);                   // Configure depth settings to enable depth testing.
            
            std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;                     // Container of shader stage create info structures.
            VkPipelineInputAssemblyStateCreateInfo _inputAssembly;                          // Input assembly configuration.
            VkPipelineRasterizationStateCreateInfo _rasterizer;                             // Rasterizer configuration.
            VkPipelineColorBlendAttachmentState _colorBlendAttachment;                      // Colour blend attachment state configuration.
            VkPipelineMultisampleStateCreateInfo _multisampling;                            // Multisampling configuration.
            VkPipelineLayout _pipelineLayout;                                               // Pipeline layout for this pipeline to be built from.
            VkPipelineDepthStencilStateCreateInfo _depthStencil;                            // Depth stencil configuration.
            VkPipelineRenderingCreateInfo _renderInfo;                                      // Pipeline rendering configuration.
            VkFormat _colorAttachmentformat;                                                // Colour attachment format to use with the colour blending attachment state.
        };

        // Read the filepath and load its data into a buffer which can then be used to create the outShaderModule.
        // Returns true on success and false on failure.
        bool load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
    }
}
