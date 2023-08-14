//
// Created by loulfy on 14/07/2023.
//

#ifndef LER_RES_HPP
#define LER_RES_HPP

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>
#include <assimp/scene.h>
#include <variant>
#include <functional>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <flecs.h>

#include "ler_sys.hpp"
#include "ler_img.hpp"
#include "ler_log.hpp"
#include "ler_dev.hpp"
#include "ler_psx.hpp"

#define LER_SHADER_COMMON
#include "ler_shader.hpp"

namespace ler
{
    template<typename ContainerType>
    void swapAndPop(ContainerType & container, size_t index)
    {
        // ensure that we're not attempting to access out of the bounds of the container.
        assert(index < container.size());

        //Swap the element with the back element, except in the case when we're the last element.
        if (index + 1 != container.size())
            std::swap(container[index], container.back());

        //Pop the back of the container, deleting our old element.
        container.pop_back();
    }

    class AssimpFileSystem : public FileSystem<AssimpFileSystem>
    {
    public:

        explicit AssimpFileSystem(const aiScene* scene) : aiScene(scene) {}
        Blob readFile(const fs::path& path) override;
        [[nodiscard]] bool exists(const fs::path& path) const override;
        void enumerates(std::vector<fs::path>& entries) override;
        [[nodiscard]] fs::file_time_type last_write_time(const fs::path& path) override;
        fs::path format_hint(const fs::path& path) override;
        static std::shared_ptr<IFileSystem> Create(const aiScene* scene) { return std::make_shared<AssimpFileSystem>(scene); }

    private:

        const aiScene* aiScene = nullptr;
    };

    physx::PxMeshScale convertToPxScale(const aiMatrix4x4& aiMatrix);
    //physx::PxTransform convertToPX(const aiMatrix4x4& aiMatrix);

    void transformBoundingBox(const glm::mat4& t, glm::vec3& min, glm::vec3& max);

    static void convertToPx(const aiMatrix4x4& aiMatrix, physx::PxTransform& transform, physx::PxMeshScale& scale)
    {
        aiVector3D scaling;
        aiVector3D position;
        aiQuaternion rotation;
        aiMatrix.Decompose(scaling, rotation, position);

        scale = physx::PxMeshScale(physx::PxVec3(scaling.x, scaling.y, scaling.z));

        rotation = rotation.Normalize();
        physx::PxVec3 translation(aiMatrix.a4, aiMatrix.b4, aiMatrix.c4);
        physx::PxQuat pxRotation(rotation.x, rotation.y, rotation.z, rotation.w);
        transform = physx::PxTransform(translation, pxRotation);
    }

    struct MeshInfo
    {
        uint32_t countIndex = 0;
        uint32_t firstIndex = 0;
        uint32_t countVertex = 0;
        int32_t firstVertex = 0;
        uint32_t materialId = 0;
        glm::vec3 bMin = glm::vec3(0.f);
        glm::vec3 bMax = glm::vec3(0.f);
        std::variant<std::monostate,physx::PxTriangleMesh*,physx::PxConvexMesh*> collision;
        std::string name;
    };

    using IndexedMesh = std::shared_ptr<MeshInfo>;

    std::array<glm::vec3, 8> createBox(const MeshInfo* mesh, glm::mat4 model);

    struct ShapeVisitor
    {
        using GeomType = std::variant<std::monostate,physx::PxTriangleMesh*,physx::PxConvexMesh*>;

        explicit ShapeVisitor(physx::PxRigidActor* _actor, physx::PxMeshScale _scale) : scale(std::move(_scale)), actor(_actor)
        {
            material = PxGetPhysics().createMaterial(0.5f, 0.5f, 0.6f);
        }

        ShapeVisitor(const IndexedMesh& mesh, aiMatrix4x4 _model)
        {
            indexedMesh = mesh;
            physx::PxTransform pxt;
            //scale = convertToPxScale(model);
            //auto pxt = convertToPX(model);

            auto dd = _model;
            model = glm::make_mat4(_model.Transpose()[0]);
            convertToPx(dd, pxt, scale);

            actor = PxGetPhysics().createRigidStatic(pxt);
            material = PxGetPhysics().createMaterial(0.5f, 0.5f, 0.6f);
        }

        void visit()
        {
            std::visit(*this, indexedMesh->collision);
        }

        void operator()(physx::PxConvexMesh* mesh) const
        {
            physx::PxConvexMeshGeometry geometry(mesh, scale);
            physx::PxRigidActorExt::createExclusiveShape(*actor, geometry, *material);
        }
        void operator()(physx::PxTriangleMesh* mesh) const
        {
            physx::PxTriangleMeshGeometry geometry(mesh, scale);
            physx::PxRigidActorExt::createExclusiveShape(*actor, geometry, *material);
        }

        void operator()(std::monostate)
        {
            glm::vec3 min = indexedMesh->bMin;
            glm::vec3 max = indexedMesh->bMax;
            glm::vec3 ss(scale.scale.x, scale.scale.y, scale.scale.z);
            min*= ss;
            max*= ss;
            glm::vec3 box = max - min;

            min = indexedMesh->bMin;
            max = indexedMesh->bMax;
            min = glm::vec3(model * glm::vec4(min, 1.f));
            max = glm::vec3(model * glm::vec4(max, 1.f));
            glm::vec3 c = (min+max)/2.f;

            physx::PxTransform pxt(physx::PxVec3(c.x, c.y, c.z));
            actor = PxGetPhysics().createRigidStatic(pxt);
            physx::PxBoxGeometry geometry(box.x, box.y, box.z);
            if(!geometry.isValid())
                log::info("lol");
            physx::PxRigidActorExt::createExclusiveShape(*actor, geometry, *material);
        }

        glm::mat4 model;
        IndexedMesh indexedMesh;
        physx::PxMeshScale scale;
        physx::PxRigidActor* actor = nullptr;
        physx::PxMaterial* material = nullptr;
    };

    static physx::PxTransform convertGlmToPx(const glm::mat4 model)
    {
        // Extract the translation vector from glm::mat4
        glm::vec3 translation = glm::vec3(model[3]);

        // Extract the rotation quaternion from glm::mat4
        glm::quat rotation = glm::quat_cast(model);
        rotation = normalize(rotation);

        // Convert the rotation quaternion to PhysX PxQuat
        physx::PxQuat pxRotation(rotation.x, rotation.y, rotation.z, rotation.w);

        // Create the PxTransform
        return physx::PxTransform(physx::PxVec3(translation.x, translation.y, translation.z), pxRotation);
    }

    struct transform {
        glm::mat4 localPos = glm::mat4(1.f);
        glm::mat4 worldPos = glm::mat4(1.f);
    };

    struct mesh {
        glm::uint meshIndex = 0;
    };

    struct dirty {

    };

    struct physic {
        physx::PxRigidActor* actor;
    };

    struct UserData
    {
        flecs::entity e;
    };

    class RenderMeshList
    {
    public:

        void allocate(const LerDevicePtr& device)
        {
            m_staging = device->createBuffer(C16Mio, vk::BufferUsageFlagBits(), true);
            m_lines.reserve(5000);
        }

        void addLine(const glm::vec3& p1, const glm::vec3& p2)
        {
            m_lines.push_back(p1);
            m_lines.push_back(p2);
        }

        void addPerDraw(mesh& m, transform& t, const MeshInfo* info)
        {
            glm::vec3 min = info->bMin;
            glm::vec3 max = info->bMax;
            transformBoundingBox(t.worldPos, min, max);

            glm::vec3 c = (info->bMin+info->bMax)/2.f;
            float radius = glm::max(length(info->bMax - c), length(info->bMin - c));

            m_mapping.emplace_back(m.meshIndex);
            m_instances.emplace_back(t.worldPos, min, max, info->materialId, m.meshIndex);

            auto pts = createBox(info, t.worldPos);
            addLine(pts[0], pts[1]);
            addLine(pts[2], pts[3]);
            addLine(pts[4], pts[5]);
            addLine(pts[6], pts[7]);

            addLine(pts[0], pts[2]);
            addLine(pts[1], pts[3]);
            addLine(pts[4], pts[6]);
            addLine(pts[5], pts[7]);

            addLine(pts[0], pts[4]);
            addLine(pts[1], pts[5]);
            addLine(pts[2], pts[6]);
            addLine(pts[3], pts[7]);
        }

        void flush(LerDevicePtr& device, BufferPtr& buffer, BufferPtr& aabb)
        {
            if(m_instances.empty())
                return;

            uint32_t byteSize = sizeof(Instance)*m_instances.size();
            m_staging->uploadFromMemory(m_instances.data(), byteSize);
            CommandPtr cmd = device->createCommand();
            cmd->copyBuffer(m_staging, buffer, byteSize);
            device->submitAndWait(cmd);

            byteSize = m_lines.size()*sizeof(glm::vec3);
            m_staging->uploadFromMemory(m_lines.data(), byteSize);
            cmd = device->createCommand();
            cmd->copyBuffer(m_staging, aabb, byteSize);
            device->submitAndWait(cmd);
        }

        void clear()
        {
            m_lines.clear();
            m_mapping.clear();
            m_instances.clear();
        }

        [[nodiscard]] std::vector<uint32_t> instances() const { return m_mapping; }
        [[nodiscard]] uint32_t getLineCount() const { return m_lines.size(); }

    private:

        BufferPtr m_staging;
        std::vector<uint32_t> m_mapping;
        std::vector<Instance> m_instances;
        std::vector<glm::vec3> m_lines;
    };

    struct BatchedMesh
    {
        BatchedMesh();
        ~BatchedMesh();

        BufferPtr indexBuffer;
        BufferPtr vertexBuffer;
        BufferPtr normalBuffer;
        BufferPtr tangentBuffer;
        BufferPtr texcoordBuffer;
        BufferPtr instanceBuffer;
        BufferPtr materialBuffer;
        BufferPtr meshletBuffer;
        BufferPtr commandBuffer;
        BufferPtr aabbBuffer;
        std::array<IndexedMesh,2048> meshes;
        std::atomic_uint32_t meshCount = 0;
        std::atomic_uint32_t indexCount = 0;
        std::atomic_uint32_t vertexCount = 0;
        std::atomic_uint32_t drawCount = 0;

        void allocate(const LerDevicePtr& device);
        void append(LerDevicePtr& device, FsTag tag, const fs::path& path);

        void registerSubmission(uint64_t submissionId, const TexturePool::Require& key);
        void loadSceneGraph(flecs::world& world);
        void receive(const Queue::CommandCompleteEvent& e);

        bool checkUpdate(LerDevicePtr& device);

        RenderMeshList renderList;

    private:

        TexturePoolPtr m_textures;
        Assimp::Importer m_importer;
        std::vector<std::pair<uint64_t,TexturePool::Require>> m_submissions;
        static void processMeshes(LerDevicePtr& device, FsTag tag, const fs::path& path, Assimp::Importer* imp, BatchedMesh* batch);
        static void loadNode(flecs::world* world, aiNode* aiNode, uint32_t meshOffset, BatchedMesh& batch);
    };

    class IRenderPass
    {
    public:

        virtual ~IRenderPass() = default;
        virtual void init(LerDevicePtr& device, GLFWwindow* window) {}
        virtual void resize(LerDevicePtr& device, vk::Extent2D extent) {}
        virtual void onSceneChange(LerDevicePtr& device, BatchedMesh& batch) {}
        virtual void prepare(LerDevicePtr& device) {}
        virtual void render(LerDevicePtr& device, FrameWindow& frame, BatchedMesh& batch, const CameraParam& camera, SceneParam& scene) = 0;
        virtual void clean() {}
    };
}

#endif //LER_RES_HPP
