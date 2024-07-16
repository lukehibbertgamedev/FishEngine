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
    Fish::Resource::Mesh mesh = {};

    // Load lost empire.
    //mesh.load_from_obj("../../assets/lost_empire.obj");
    //upload_mesh(mesh);
    //m_Meshes["minecraft"] = mesh;

    mesh = {};
    mesh.load_from_obj("../../assets/Horse.obj");
    upload_mesh(mesh);
    m_Meshes["horse"] = mesh;

    mesh = {};
    mesh = create_default_triangle();
    upload_mesh(mesh);
    m_Meshes["triangle"] = mesh;

    mesh = {};
    mesh = create_default_quad();
    upload_mesh(mesh);
    m_Meshes["quad"] = mesh;

    mesh = {};
    mesh = create_default_cube();
    upload_mesh(mesh);
    m_Meshes["cube"] = mesh;

}

void Fish::ResourceManager::load_all_textures()
{

    // Load texture.
    {
        Fish::Resource::Texture texture = {};
        Fish::Loader::load_image_from_file(FishVulkanEngine::Get(), "../../assets/lost_empire-RGBA.png", texture.image);
        VkImageViewCreateInfo imageInfo = vkinit::imageview_create_info(VK_FORMAT_R8G8B8A8_SRGB, texture.image.image, VK_IMAGE_ASPECT_COLOR_BIT);
        vkCreateImageView(FishVulkanEngine::Get().GetDevice(), &imageInfo, nullptr, &texture.imageView);
        m_Textures["minecraft_texture"] = texture;
    }

    // Load next texture...
    {
        Fish::Resource::Texture texture = {};
        Fish::Loader::load_image_from_file(FishVulkanEngine::Get(), "../../assets/MaterialBrown.png", texture.image);
        VkImageViewCreateInfo imageInfo = vkinit::imageview_create_info(VK_FORMAT_R8G8B8A8_SRGB, texture.image.image, VK_IMAGE_ASPECT_COLOR_BIT);
        vkCreateImageView(FishVulkanEngine::Get().GetDevice(), &imageInfo, nullptr, &texture.imageView);
        m_Textures["horse_brown_texture"] = texture;
    }
}

void Fish::ResourceManager::load_scene()
{
    Fish::Resource::RenderObject obj = {};

    obj.pMesh = get_mesh_by_name("quad");
    obj.pMaterial = get_material_by_name("texturedmesh");
    obj.transformMatrix = glm::translate(glm::vec3{ 0,-10,0 });
    m_Scene.m_SceneObjects.push_back(obj);

    obj.pMesh = get_mesh_by_name("triangle");
    obj.pMaterial = get_material_by_name("texturedmesh");
    obj.transformMatrix = glm::translate(glm::vec3{ 5,-10,0 });
    m_Scene.m_SceneObjects.push_back(obj);

    obj.pMesh = get_mesh_by_name("cube");
    obj.pMaterial = get_material_by_name("texturedmesh");
    obj.transformMatrix = glm::translate(glm::vec3{ 0,0,0 });
    m_Scene.m_SceneObjects.push_back(obj);

    obj.pMesh = get_mesh_by_name("horse");
    obj.pMaterial = get_material_by_name("texturedmesh");
    obj.transformMatrix = glm::translate(glm::vec3{ 5,0,0 });
    m_Scene.m_SceneObjects.push_back(obj);     

    // Texture loading/sampling...

    //create a sampler for the texture
    VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);
    VkSampler blockySampler = {};
    vkCreateSampler(FishVulkanEngine::Get().GetDevice(), &samplerInfo, nullptr, &blockySampler);

    Fish::Resource::Material* texturedMat = get_material_by_name("texturedmesh");

    //allocate the descriptor set for single-texture to use on the material
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.pNext = nullptr;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorSetCount = 1;
    allocInfo.descriptorPool = FishVulkanEngine::Get().GetDescriptorPool();
    allocInfo.pSetLayouts = &FishVulkanEngine::Get().GetSingleTextureSetLayout();

    vkAllocateDescriptorSets(FishVulkanEngine::Get().GetDevice(), &allocInfo, &texturedMat->textureSet);

    // Write to the descriptor set so that it points to our texture
    VkDescriptorImageInfo imageBufferInfo = {};
    imageBufferInfo.sampler = blockySampler;
    imageBufferInfo.imageView = m_Textures["horse_brown_texture"].imageView;
    imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet textureImg = vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texturedMat->textureSet, &imageBufferInfo, 0);

    vkUpdateDescriptorSets(FishVulkanEngine::Get().GetDevice(), 1, &textureImg, 0, nullptr);
}

Fish::Resource::Mesh Fish::ResourceManager::create_default_triangle()
{
    Fish::Resource::Mesh mesh;

    //make the array 3 vertices long
    mesh.vertices.resize(3);

    //vertex positions
    mesh.vertices[0].position = { 0.0f, 0.0f, 0.0f };
    mesh.vertices[1].position = { 1.0f, 0.0f, 0.0f };
    mesh.vertices[2].position = { 0.0f, 1.0f, 0.0f };

    //vertex colors, defaulted to white
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        mesh.vertices[i].colour = { 0.0f, 0.0f, 0.0f };
    }

    //we don't care about the vertex normals

    // indices
    mesh.indices.resize(3);
    mesh.indices = { 0, 1, 2 };

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

    //vertex colors, defaulted to white
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        mesh.vertices[i].colour = { 0.0f, 0.0f, 0.0f };
    } 

    //we don't care about the vertex normals

    // indices 
    mesh.indices.resize(6);
    mesh.indices = { 0, 1, 3, 0, 2, 3 };

    return mesh;
}

Fish::Resource::Mesh Fish::ResourceManager::create_default_cube()
{
    Fish::Resource::Mesh mesh;

    mesh.vertices.resize(36);

    // bottom face
    mesh.vertices[0].position = { 0.0f, 0.0f, 0.0f };
    mesh.vertices[1].position = { 1.0f, 0.0f, 0.0f };
    mesh.vertices[2].position = { 1.0f, 0.0f, 1.0f };
    mesh.vertices[3].position = { 0.0f, 0.0f, 1.0f };

   // right face 
    mesh.vertices[4].position = { 1.0f, 0.0f, 0.0f }; // point 1
    mesh.vertices[5].position = { 1.0f, 1.0f, 0.0f }; // point 1 raised on y
    mesh.vertices[6].position = { 1.0f, 1.0f, 1.0f }; // point 2 raised on y
    mesh.vertices[7].position = { 1.0f, 0.0f, 1.0f }; // point 2

    // back face
    mesh.vertices[8].position = { 0.0f, 0.0f, 0.0f }; // point 0
    mesh.vertices[9].position = { 0.0f, 1.0f, 0.0f }; // point 0 raised on y
    mesh.vertices[10].position = { 1.0f, 1.0f, 0.0f }; // point 1 raised on y
    mesh.vertices[11].position = { 1.0f, 0.0f, 0.0f }; // point 1

    // left face
    mesh.vertices[12].position = { 0.0f, 0.0f, 0.0f }; // point 0
    mesh.vertices[13].position = { 0.0f, 1.0f, 0.0f }; // point 0 raised on y
    mesh.vertices[14].position = { 0.0f, 1.0f, 1.0f }; // point 3 raised on y
    mesh.vertices[15].position = { 0.0f, 0.0f, 1.0f }; // point 3

    // front face
    mesh.vertices[16].position = { 0.0f, 0.0f, 1.0f }; // point 3
    mesh.vertices[17].position = { 0.0f, 1.0f, 1.0f }; // point 3 raised on y
    mesh.vertices[18].position = { 1.0f, 1.0f, 1.0f }; // point 2 raised on y
    mesh.vertices[19].position = { 1.0f, 0.0f, 1.0f }; // point 2

    // top face
    mesh.vertices[20].position = { 0.0f, 1.0f, 0.0f }; // point 0 raised on y
    mesh.vertices[21].position = { 1.0f, 1.0f, 0.0f }; // point 1 raised on y
    mesh.vertices[22].position = { 1.0f, 1.0f, 1.0f }; // point 2 raised on y
    mesh.vertices[23].position = { 0.0f, 1.0f, 1.0f }; // point 3 raised on y

    // indices 
    mesh.indices.resize(36);
    mesh.indices = {
        0,1,2, 2,3,0,
        4,5,6, 6,7,4,
        8,9,10, 10,11,8,
        12,13,14, 14,15,12,
        16,17,18, 18,19,16,
        20,21,22, 22,23,20
    };

    //vertex colors, defaulted to white
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        mesh.vertices[i].colour = { 0.0f, 0.0f, 0.0f };
    }

    return mesh;

    //mesh.indices = {
    //    // +-x
    //    0,1,4,	4,1,5,
    //    2,3,6,	6,3,7,
    //    // +-y
    //    1,0,2,	2,0,3,
    //    4,5,6,	4,6,7,
    //    // +-z
    //    2,5,1,	2,6,5,
    //    3,0,4,	3,4,7
    //};

    //vertex positions
    //glm::vec3 extents = { 1.f, 1.f, 1.f };
    //glm::vec3 corners[] =
    //{
    //    { -extents.x,  extents.y,  extents.z },		// V0 = -0.5,  0.5,  0.5
    //    { -extents.x,  extents.y, -extents.z },		// V1 = -0.5,  0.5, -0.5
    //    {  extents.x,  extents.y, -extents.z },		// V2 =  0.5,  0.5, -0.5
    //    {  extents.x,  extents.y,  extents.z },		// V3 =  0.5,  0.5,  0.5
    //    { -extents.x, -extents.y,  extents.z },		// V4 = -0.5, -0.5,  0.5
    //    { -extents.x, -extents.y, -extents.z },		// V5 = -0.5, -0.5, -0.5
    //    {  extents.x, -extents.y, -extents.z },		// V6 =  0.5, -0.5, -0.5
    //    {  extents.x, -extents.y,  extents.z }		// V7 =  0.5, -0.5,  0.5
    //};
    //for (size_t i = 0; i < mesh.vertices.size(); i++)
    //{
    //    mesh.vertices[i].position = corners[mesh.indices[i]];
    //    mesh.vertices[i].colour = { 0.f, 1.f, 0.0f }; //pure green
    //}

    //vertex normals
    //for (size_t i = 0; i < mesh.vertices.size(); i+=3)
    //{
    //    // Get verts of current tri
    //    glm::vec3 v0 = mesh.vertices[i + 0].position;
    //    glm::vec3 v1 = mesh.vertices[i + 1].position;
    //    glm::vec3 v2 = mesh.vertices[i + 2].position;

    //    // Calculate edges
    //    glm::vec3 edge0 = v0 - v1;
    //    glm::vec3 edge1 = v1 - v2;

    //    // Calculate cross product
    //    glm::vec3 crossPrd = glm::cross(edge0, edge1);
    //    // Normalise
    //    crossPrd = glm::normalize(crossPrd);

    //    // Set normals
    //    //mesh.vertices[i + 0].normal = crossPrd;
    //    //mesh.vertices[i + 1].normal = crossPrd;
    //    //mesh.vertices[i + 2].normal = crossPrd;
    //}
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
    VK_CHECK(vmaCreateBuffer(FishVulkanEngine::Get().GetAllocator(), &stagingBufferInfo, &vmaallocInfo, &stagingBuffer.buffer, &stagingBuffer.allocation, nullptr));

    //

    //copy vertex data
    void* vertexData;
    vmaMapMemory(FishVulkanEngine::Get().GetAllocator(), stagingBuffer.allocation, &vertexData);

    memcpy(vertexData, mesh.vertices.data(), mesh.vertices.size() * sizeof(Fish::Resource::Vertex));

    vmaUnmapMemory(FishVulkanEngine::Get().GetAllocator(), stagingBuffer.allocation);

    //

    //allocate index buffer
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
    VK_CHECK(vmaCreateBuffer(FishVulkanEngine::Get().GetAllocator(), &vertexBufferInfo, &vmaallocInfo, &mesh.vertexBuffer.buffer, &mesh.vertexBuffer.allocation, nullptr));

    //

    FishVulkanEngine::Get().immediate_submit([=](VkCommandBuffer cmd) {
        VkBufferCopy copy;
        copy.dstOffset = 0;
        copy.srcOffset = 0;
        copy.size = bufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, mesh.vertexBuffer.buffer, 1, &copy);
    });

    //copy index data
    void* indexData;
    vmaMapMemory(FishVulkanEngine::Get().GetAllocator(), stagingBuffer.allocation, &indexData);

    memcpy(indexData, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));

    vmaUnmapMemory(FishVulkanEngine::Get().GetAllocator(), stagingBuffer.allocation);

    //

    //allocate vertex buffer
    VkBufferCreateInfo indexBufferInfo = {};
    indexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    indexBufferInfo.pNext = nullptr;
    //this is the total size, in bytes, of the buffer we are allocating
    indexBufferInfo.size = bufferSize;
    //this buffer is going to be used as a Index Buffer
    indexBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    //let the VMA library know that this data should be GPU native
    vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    //allocate the buffer
    VK_CHECK(vmaCreateBuffer(FishVulkanEngine::Get().GetAllocator(), &indexBufferInfo, &vmaallocInfo, &mesh.indexBuffer.buffer, &mesh.indexBuffer.allocation, nullptr));

    //

    FishVulkanEngine::Get().immediate_submit([=](VkCommandBuffer cmd) {
        VkBufferCopy copy;
        copy.dstOffset = 0;
        copy.srcOffset = 0;
        copy.size = bufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, mesh.indexBuffer.buffer, 1, &copy);
    });

    //

    //add the destruction of mesh buffer to the deletion queue
    FishVulkanEngine::Get().GetDeletionQueue().push_function([=]() { vmaDestroyBuffer(FishVulkanEngine::Get().GetAllocator(), mesh.vertexBuffer.buffer, mesh.vertexBuffer.allocation); });
    FishVulkanEngine::Get().GetDeletionQueue().push_function([=]() { vmaDestroyBuffer(FishVulkanEngine::Get().GetAllocator(), mesh.indexBuffer.buffer, mesh.indexBuffer.allocation); });
    vmaDestroyBuffer(FishVulkanEngine::Get().GetAllocator(), stagingBuffer.buffer, stagingBuffer.allocation);
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

//void Fish::ResourceManager::sample_texture(const std::string& name, VkSamplerCreateInfo samplerInfo, VkSampler blockySampler, VkDescriptorSetAllocateInfo allocInfo, VkWriteDescriptorSet texture1)
//{
//    //create a sampler for the texture
//    samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);
//
//    vkCreateSampler(FishVulkanEngine::Get().GetDevice(), &samplerInfo, nullptr, &blockySampler);
//
//    Fish::Resource::Material* texturedMat = get_material_by_name("texturedmesh");
//
//    //allocate the descriptor set for single-texture to use on the material
//    allocInfo = {};
//    allocInfo.pNext = nullptr;
//    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
//    allocInfo.descriptorSetCount = 1;
//    allocInfo.descriptorPool = FishVulkanEngine::Get().GetDescriptorPool();
//    allocInfo.pSetLayouts = &FishVulkanEngine::Get().GetSingleTextureSetLayout();
//
//    vkAllocateDescriptorSets(FishVulkanEngine::Get().GetDevice(), &allocInfo, &texturedMat->textureSet);
//
//    //write to the descriptor set so that it points to our texture
//    VkDescriptorImageInfo imageBufferInfo = {};
//    imageBufferInfo.sampler = blockySampler;
//    imageBufferInfo.imageView = m_Textures[name].imageView;
//    imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//
//    texture1 = vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texturedMat->textureSet, &imageBufferInfo, 0);
//
//    vkUpdateDescriptorSets(FishVulkanEngine::Get().GetDevice(), 1, &texture1, 0, nullptr);
//}
