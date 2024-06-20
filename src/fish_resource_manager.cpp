#include "fish_resource_manager.h"
#include <loader.h>
#include <vk_initializers.h>
#include <glm/gtx/transform.hpp>



void Fish::ResourceManager::init()
{
	load_all_textures();

	load_all_meshes();

	load_scene();
}

void Fish::ResourceManager::load_all_meshes()
{
    Fish::Resource::Mesh mesh;

    // Load lost empire.
    mesh.load_from_obj("../../assets/lost_empire.obj");
    upload_mesh(mesh);
    m_Meshes["empire"] = mesh;

    // Load smooth monkey.
    mesh.load_from_obj("../../assets/monkey_smooth.obj");
    upload_mesh(mesh);
    m_Meshes["monkey"] = mesh;
}

void Fish::ResourceManager::load_all_textures()
{
    Fish::Resource::Texture texture;

    // Load lost empire.
    Fish::Loader::load_image_from_file(VulkanEngine::Get(), "../../assets/lost_empire-RGBA.png", texture.image);
    VkImageViewCreateInfo imageInfo = vkinit::imageview_create_info(VK_FORMAT_R8G8B8A8_SRGB, texture.image.image, VK_IMAGE_ASPECT_COLOR_BIT);
    vkCreateImageView(VulkanEngine::Get().GetDevice(), &imageInfo, nullptr, &texture.imageView);
    m_Textures["empire_diffuse"] = texture;

    // Load next texture...
}

void Fish::ResourceManager::load_scene()
{
    Fish::Resource::RenderObject monkey;
    monkey.pMesh = get_mesh_by_name("monkey");
    monkey.pMaterial = get_material_by_name("defaultmesh");
    monkey.transformMatrix = glm::mat4{ 1.0f };
    m_Scene.GetSceneObjects().push_back(monkey);

    // We create 1 monkey, add it as the first thing to the renderables array, 
    // and then we create a lot of triangles in a grid, and put them around the monkey.

    for (int x = -20; x <= 20; x++) {
        for (int y = -20; y <= 20; y++) {

            Fish::Resource::RenderObject triangle;
            triangle.pMesh = get_mesh_by_name("triangle");
            triangle.pMaterial = get_material_by_name("defaultmesh");
            glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, glm::vec3(x, 0, y));
            glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, glm::vec3(0.2, 0.2, 0.2));
            triangle.transformMatrix = translation * scale;

            m_Scene.GetSceneObjects().push_back(triangle);
        }
    }

    Fish::Resource::RenderObject map;
    map.pMesh = get_mesh_by_name("empire");
    map.pMaterial = get_material_by_name("texturedmesh");
    map.transformMatrix = glm::translate(glm::vec3{ 5,-10,0 });
    m_Scene.GetSceneObjects().push_back(map);

    //

    //create a sampler for the texture
    VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);

    VkSampler blockySampler;
    vkCreateSampler(VulkanEngine::Get().GetDevice(), &samplerInfo, nullptr, &blockySampler);

    Fish::Resource::Material* texturedMat = get_material_by_name("texturedmesh");

    //allocate the descriptor set for single-texture to use on the material
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext = nullptr;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorSetCount = 1;
    allocInfo.descriptorPool = VulkanEngine::Get().GetDescriptorPool();
    allocInfo.pSetLayouts = &VulkanEngine::Get().GetSingleTextureSetLayout();

    vkAllocateDescriptorSets(VulkanEngine::Get().GetDevice(), &allocInfo, &texturedMat->textureSet);

    //write to the descriptor set so that it points to our empire_diffuse texture
    VkDescriptorImageInfo imageBufferInfo;
    imageBufferInfo.sampler = blockySampler;
    imageBufferInfo.imageView = m_Textures["empire_diffuse"].imageView;
    imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet texture1 = vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texturedMat->textureSet, &imageBufferInfo, 0);

    vkUpdateDescriptorSets(VulkanEngine::Get().GetDevice(), 1, &texture1, 0, nullptr);
}

Fish::Resource::Mesh& Fish::ResourceManager::create_default_triangle()
{
    Fish::Resource::Mesh mesh;

    //make the array 3 vertices long
    mesh.vertices.resize(3);

    //vertex positions
    mesh.vertices[0].position = { 1.f, 1.f, 0.0f };
    mesh.vertices[1].position = { -1.f, 1.f, 0.0f };
    mesh.vertices[2].position = { 0.f,-1.f, 0.0f };

    //vertex colors, all green
    mesh.vertices[0].colour = { 0.f, 1.f, 0.0f }; //pure green
    mesh.vertices[1].colour = { 0.f, 1.f, 0.0f }; //pure green
    mesh.vertices[2].colour = { 0.f, 1.f, 0.0f }; //pure green

    //we don't care about the vertex normals

    return mesh;
}

Fish::Resource::Mesh* Fish::ResourceManager::get_mesh_by_name(const std::string& name)
{
    auto it = m_Meshes.find(name);
    if (it == m_Meshes.end()) {
        return nullptr;
    }
    else {
        return &(*it).second;
    }
}

void Fish::ResourceManager::upload_mesh(Fish::Resource::Mesh& mesh)
{
    const size_t bufferSize = mesh.vertices.size() * sizeof(Fish::Resource::Vertex);

    //allocate staging buffer
    VkBufferCreateInfo stagingBufferInfo = {};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.pNext = nullptr;

    stagingBufferInfo.size = bufferSize;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    //let the VMA library know that this data should be on CPU RAM
    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    AllocatedBuffer stagingBuffer;

    //allocate the buffer
    VK_CHECK(vmaCreateBuffer(VulkanEngine::Get().GetAllocator(), &stagingBufferInfo, &vmaallocInfo, &stagingBuffer.buffer, &stagingBuffer.allocation, nullptr));

    //

    //copy vertex data
    void* data;
    vmaMapMemory(VulkanEngine::Get().GetAllocator(), stagingBuffer.allocation, &data);

    memcpy(data, mesh.vertices.data(), mesh.vertices.size() * sizeof(Fish::Resource::Vertex));

    vmaUnmapMemory(VulkanEngine::Get().GetAllocator(), stagingBuffer.allocation);

    //

    //allocate vertex buffer
    VkBufferCreateInfo vertexBufferInfo = {};
    vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexBufferInfo.pNext = nullptr;
    //this is the total size, in bytes, of the buffer we are allocating
    vertexBufferInfo.size = bufferSize;
    //this buffer is going to be used as a Vertex Buffer
    vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    //let the VMA library know that this data should be GPU native
    vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    //allocate the buffer
    VK_CHECK(vmaCreateBuffer(VulkanEngine::Get().GetAllocator(), &vertexBufferInfo, &vmaallocInfo, &mesh.vertexBuffer.buffer, &mesh.vertexBuffer.allocation, nullptr));

    //

    VulkanEngine::Get().immediate_submit([=](VkCommandBuffer cmd) {
        VkBufferCopy copy;
        copy.dstOffset = 0;
        copy.srcOffset = 0;
        copy.size = bufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, mesh.vertexBuffer.buffer, 1, &copy);
    });

    //

    //add the destruction of mesh buffer to the deletion queue
    VulkanEngine::Get().GetDeletionQueue().push_function([=]() { vmaDestroyBuffer(VulkanEngine::Get().GetAllocator(), mesh.vertexBuffer.buffer, mesh.vertexBuffer.allocation); });
    vmaDestroyBuffer(VulkanEngine::Get().GetAllocator(), stagingBuffer.buffer, stagingBuffer.allocation);
}

Fish::Resource::Material* Fish::ResourceManager::create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name)
{
    Fish::Resource::Material mat;
    mat.pipeline = pipeline;
    mat.pipelineLayout = layout;
    m_Materials[name] = mat;
    return &m_Materials[name];
}

Fish::Resource::Material* Fish::ResourceManager::get_material_by_name(const std::string& name)
{
    auto it = m_Materials.find(name);
    if (it == m_Materials.end()) {
        return nullptr;
    }
    else {
        return &(*it).second;
    }
}
