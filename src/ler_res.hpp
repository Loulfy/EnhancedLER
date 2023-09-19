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

    static physx::PxTransform convertGlmToPx(const glm::mat4 model)
    {
        // Extract the translation vector from glm::mat4
        auto translation = glm::vec3(model[3]);

        // Extract the rotation quaternion from glm::mat4
        glm::quat rotation = glm::quat_cast(model);
        rotation = normalize(rotation);

        // Convert the rotation quaternion to PhysX PxQuat
        physx::PxQuat pxRotation(rotation.x, rotation.y, rotation.z, rotation.w);

        // Create the PxTransform
        return physx::PxTransform(physx::PxVec3(translation.x, translation.y, translation.z), pxRotation);
    }

    struct CTransform
    {
        glm::mat4 model = glm::mat4(1.f);
    };

    struct CMesh
    {
        glm::uint meshIndex = 0;
        glm::vec3 min = glm::vec3(0.f);
        glm::vec3 max = glm::vec3(0.f);
        glm::vec4 bounds = glm::vec4(0.f);
    };

    struct CMaterial
    {
        glm::uint materialId = 0;
    };

    struct CInstance
    {
        glm::uint instanceId = 0;
    };

    struct dirty
    {

    };

    struct CPhysic
    {
        physx::PxRigidActor* actor;
    };

    struct UserData
    {
        flecs::entity e;
    };
}

#endif //LER_RES_HPP
