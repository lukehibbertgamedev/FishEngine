
#pragma once
#include <vk_types.h>
#include <vk_descriptors.h>

#include <unordered_map>
#include <filesystem>

#include <glm/gtx/transform.hpp>


//forward declaration
class FishEngine;

namespace Component { struct Transform; }

namespace Fish {
    namespace Loader {

        struct Bounds {
            glm::vec3 origin;
            float sphereRadius;
            glm::vec3 extents;
        };

        struct GLTFMaterial {
            MaterialInstance data;
        };

        // Contains sub-meshes of the main mesh.
        struct GeoSurface {
            uint32_t startIndex;
            uint32_t count;
            Bounds bounds;
            std::shared_ptr<GLTFMaterial> material;
        };

        // Entire mesh asset containing a name and the mesh buffers with an array of 
        // GeoSurfaces for the sub-meshes that this full mesh will contain.
        struct MeshAsset {
            std::string name;

            std::vector<GeoSurface> surfaces;
            GPUMeshBuffers meshBuffers;
        };

        // See (https://vkguide.dev/docs/new_chapter_5/gltf_nodes/) for more.
        struct LoadedGLTF : public IRenderable {
        public:
            // storage for all the data on a given gltf file
            std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
            std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
            std::unordered_map<std::string, AllocatedImage> images;
            std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

            // nodes that dont have a parent, for iterating through the file in tree order
            std::vector<std::shared_ptr<Node>> topNodes;

            Fish::Component::Transform transform;

            std::vector<VkSampler> samplers;

            DescriptorAllocatorGrowable descriptorPool;

            AllocatedBuffer13 materialDataBuffer;

            FishEngine* creator;

            ~LoadedGLTF() { clearAll(); };

            void Update() 
            {
                glm::vec3 t(transform.position.x, transform.position.y, transform.position.z);           // Translation.
                glm::mat4 tm = glm::translate(glm::mat4(1.0f), t);                                       // Translation.
                glm::vec3 rx(1.0f, 0.0f, 0.0f);                                                          // Rotation.
                glm::vec3 ry(0.0f, 1.0f, 0.0f);                                                          // Rotation.
                glm::vec3 rz(0.0f, 0.0f, 1.0f);                                                          // Rotation.
                glm::mat4 rxm = glm::rotate(glm::mat4(1.0f), glm::radians(transform.rotation.x), rx);    // Rotation.
                glm::mat4 rym = glm::rotate(glm::mat4(1.0f), glm::radians(transform.rotation.y), ry);    // Rotation.
                glm::mat4 rzm = glm::rotate(glm::mat4(1.0f), glm::radians(transform.rotation.z), rz);    // Rotation.
                glm::mat4 rm = rzm * rym * rxm;                                                          // Rotation.
                glm::vec3 s(transform.scale.x, transform.scale.y, transform.scale.z);                    // Scale.
                glm::mat4 sm = glm::scale(glm::mat4(1.0f), s);                                           // Scale.
                glm::mat4 _final = tm * rm * sm;                                                         // Combined.

                // Set all node transforms. All children nodes must be multiplied by the new parent/world transformation matrix.
                for (auto& n : topNodes) {
                    Node& _n = *n;
                    for (auto& c : _n.children) {
                        Node& _c = *c;

                        _c.localTransformMatrix *= _final;
                    }
                    _n.worldTransformMatrix = _final;
                }
            }

            virtual void Draw(const glm::mat4& transformationMatrix, DrawContext& ctx)
            {
                // create renderables from the scenenodes
                for (auto& n : topNodes) {
                    n->Draw(transformationMatrix, ctx);
                }
            }

        private:

            void clearAll();
        };

        // ...
        std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(FishEngine* engine, std::string_view filePath);

    }
}