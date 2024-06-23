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
    //mesh.load_from_obj("../../assets/lost_empire.obj");
    //upload_mesh(mesh);
    //m_Meshes["minecraft"] = mesh;

    mesh.load_from_obj("../../assets/Horse.obj");
    upload_mesh(mesh);
    m_Meshes["horse"] = mesh;

    mesh = create_default_triangle();
    upload_mesh(mesh);
    m_Meshes["triangle"] = mesh;

    //

    mesh = create_default_quad();
    upload_mesh(mesh);
    m_Meshes["quad"] = mesh;

}

void Fish::ResourceManager::load_all_textures()
{

    // Load horse texture.
    {
        Fish::Resource::Texture texture = {};
        Fish::Loader::load_image_from_file(VulkanEngine::Get(), "../../assets/lost_empire-RGBA.png", texture.image);
        VkImageViewCreateInfo imageInfo = vkinit::imageview_create_info(VK_FORMAT_R8G8B8A8_SRGB, texture.image.image, VK_IMAGE_ASPECT_COLOR_BIT);
        vkCreateImageView(VulkanEngine::Get().GetDevice(), &imageInfo, nullptr, &texture.imageView);
        m_Textures["minecraft_texture"] = texture;
    }

    // Load next texture...
    {
        Fish::Resource::Texture texture = {};
        Fish::Loader::load_image_from_file(VulkanEngine::Get(), "../../assets/MaterialBrown.png", texture.image);
        VkImageViewCreateInfo imageInfo = vkinit::imageview_create_info(VK_FORMAT_R8G8B8A8_SRGB, texture.image.image, VK_IMAGE_ASPECT_COLOR_BIT);
        vkCreateImageView(VulkanEngine::Get().GetDevice(), &imageInfo, nullptr, &texture.imageView);
        m_Textures["horse_brown_texture"] = texture;
    }
}

void Fish::ResourceManager::load_scene()
{
    Fish::Resource::RenderObject obj = {};

    // Load the little horsey.
    obj.pMesh = get_mesh_by_name("horse");
    obj.pMaterial = get_material_by_name("texturedmesh");
    obj.transformMatrix = glm::translate(glm::vec3{ 5,-10,0 });
    m_Scene.m_SceneObjects.push_back(obj);

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
    imageBufferInfo.imageView = m_Textures["horse_brown_texture"].imageView;
    imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet texture1 = vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texturedMat->textureSet, &imageBufferInfo, 0);

    vkUpdateDescriptorSets(VulkanEngine::Get().GetDevice(), 1, &texture1, 0, nullptr);
}

Fish::Resource::Mesh Fish::ResourceManager::create_default_triangle()
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

Fish::Resource::Mesh Fish::ResourceManager::create_default_quad()
{
    Fish::Resource::Mesh mesh;

    //make the array 4 vertices long
    mesh.vertices.resize(4);

    //vertex positions
    mesh.vertices[0].position = { -1.f, 1.f, 0.0f };
    mesh.vertices[1].position = { -1.f, -1.f, 0.0f };
    mesh.vertices[2].position = { 1.f,1.f, 0.0f };
    mesh.vertices[3].position = { 1.f,-1.f, 0.0f };

    //vertex colors, all green
    mesh.vertices[0].colour = { 1.f, 0.f, 0.0f }; //pure green
    mesh.vertices[1].colour = { 0.f, 1.f, 0.0f }; //pure green
    mesh.vertices[2].colour = { 0.f, 1.f, 0.0f }; //pure green
    mesh.vertices[3].colour = { 1.f, 0.f, 0.0f }; //pure green

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
