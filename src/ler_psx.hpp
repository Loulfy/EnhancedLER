//
// Created by loulfy on 29/07/2023.
//

#ifndef LER_PSX_HPP
#define LER_PSX_HPP

#include <variant>
#include <PxConfig.h>
#include <PxPhysicsAPI.h>
#include <extensions/PxDefaultSimulationFilterShader.h>

#include <utility>

namespace ler
{
    class PXInitializer
    {
    public:

        PXInitializer();
        ~PXInitializer();

        [[nodiscard]] static physx::PxTriangleMesh* BuildTriangleMesh(const physx::PxTriangleMeshDesc& meshDesc);
        [[nodiscard]] static physx::PxConvexMesh* BuildConvexMesh(const physx::PxConvexMeshDesc& meshDesc);

        [[nodiscard]] physx::PxScene* getScene() const { return m_scene; }
        void simulate();

    private:

        physx::PxFoundation* m_foundation;
        physx::PxPvd* m_pvd;
        static physx::PxPhysics* m_physics;
        static physx::PxCooking* m_cooking;
        physx::PxScene* m_scene;

    };
}

#endif //LER_PSX_HPP
