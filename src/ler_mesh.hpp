//
// Created by loulfy on 30/08/2023.
//

#ifndef LER_MESH_HPP
#define LER_MESH_HPP

#include "ler_dev.hpp"
#include "ler_res.hpp"

#include <meshoptimizer.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

namespace ler
{
    struct MeshInfo
    {
        uint32_t countIndex = 0;
        uint32_t firstIndex = 0;
        uint32_t countVertex = 0;
        int32_t firstVertex = 0;
        uint32_t materialId = 0;
        glm::vec3 bMin = glm::vec3(0.f);
        glm::vec3 bMax = glm::vec3(0.f);
        glm::vec4 bounds = glm::vec4(0.f);
        std::string name;
        std::variant<std::monostate,physx::PxTriangleMesh*,physx::PxConvexMesh*> collision;
    };

    using IndexedMesh = std::shared_ptr<MeshInfo>;

    class SceneBuffers
    {
    public:

        friend class SceneImporter;
        void allocate(const LerDevicePtr& device);
        void bind(const CommandPtr& cmd, bool prePass) const;

        static constexpr uint32_t kMaxMesh = 2048;
        static constexpr uint32_t kMaxBufferSize = C64Mio;
        static constexpr uint32_t kShaderGroupSizeNV = 32;
        static constexpr uint32_t kMaxVerticesPerMeshlet = 64;
        static constexpr uint32_t kMaxTrianglesPerMeshlet = 124;

        [[nodiscard]] const BufferPtr& getMaterialBuffer() const;
        [[nodiscard]] const BufferPtr& getMeshletBuffer() const;
        [[nodiscard]] IndexedMesh getMeshInfo(uint32_t meshId) const;

    private:

        /*
         * 0 : vertex
         * 1 : normal
         * 2 : tangent
         * 3 : texcoord
         * 4 : material
         * 5 : meshlet
         */

        BufferPtr m_indexBuffer;
        std::array<BufferPtr, 4> m_vertexBuffers;
        std::array<IndexedMesh, kMaxMesh> meshes;
        std::array<BufferPtr, 2> m_staticBuffers;

        std::atomic_uint32_t drawCount = 0;
        std::atomic_uint32_t meshCount = 0;
        std::atomic_uint32_t indexCount = 0;
        std::atomic_uint32_t vertexCount = 0;
        std::atomic_uint32_t materialCount = 0;
    };

    class PhysicBuilder
    {
    public:

        explicit PhysicBuilder(const aiMatrix4x4& model) : m_model(model) {}

        void operator()(std::monostate);
        void operator()(physx::PxConvexMesh* mesh);
        void operator()(physx::PxTriangleMesh* mesh);

        physx::PxRigidActor* actor = nullptr;

    private:

        physx::PxMeshScale create();
        aiMatrix4x4 m_model;
    };

    class SceneImporter
    {
    public:

        struct SceneSubmission
        {
            Assimp::Importer importer;
            SceneBuffers* scene = nullptr;
            TexturePool::Require dependency;
            uint64_t submissionId = UINT64_MAX;
        };

        using SceneSubmissionPtr = std::shared_ptr<SceneSubmission>;

        static bool LoadScene(const LerDevicePtr& device, SceneBuffers& scene, FsTag tag, const fs::path& path);
        static std::vector<SceneBuffers*> PollUpdate(const LerDevicePtr& device, flecs::world& world);

    protected:

        static std::vector<SceneSubmissionPtr> m_submissions;
        static void processMeshes(const LerDevicePtr& device, FsTag tag, const fs::path& path, const SceneSubmissionPtr& submission);
        static void processSceneGraph(const SceneSubmissionPtr& submission, flecs::world& world);
        static void processSceneNode(flecs::world& world, aiNode* aiNode, uint32_t meshOffset, SceneBuffers* scene);

        static void testy(aiMesh* mesh);
        static Meshly buildMeshlet(const meshopt_Meshlet& meshlet, const meshopt_Bounds& bounds);
        static glm::vec4 calculateMeshBounds(aiMesh* mesh);
    };

    struct SubmitScene
    {
        CommandPtr command;
        SceneImporter::SceneSubmissionPtr submission;
    };

    using AsyncRequest = std::variant<SubmitTexture, SubmitScene>;
}

#endif //LER_MESH_HPP
