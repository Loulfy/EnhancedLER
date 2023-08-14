//
// Created by loulfy on 29/07/2023.
//

#include "ler_psx.hpp"
#include "ler_log.hpp"

namespace ler
{
    using namespace physx;

    physx::PxPhysics* PXInitializer::m_physics = nullptr;
    physx::PxCooking* PXInitializer::m_cooking = nullptr;

    PXInitializer::PXInitializer()
    {
        static PxDefaultErrorCallback gDefaultErrorCallback;
        static PxDefaultAllocator gDefaultAllocatorCallback;

        m_foundation = PxCreateFoundation(PX_PHYSICS_VERSION, gDefaultAllocatorCallback, gDefaultErrorCallback);
        if (!m_foundation)
            log::exit("Error initializing the PhysX foundation.");

        m_pvd = PxCreatePvd(*m_foundation);
        PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 1000);
        if(m_pvd->connect(*transport,PxPvdInstrumentationFlag::eDEBUG))
            log::info("Connected to PVD");

        // Create the Physics SDK
        m_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_foundation, PxTolerancesScale(), true, m_pvd);
        if (!m_physics)
            log::exit("Error initializing the PhysX SDK.");

        // Create the cooking extension
        m_cooking = PxCreateCooking(PX_PHYSICS_VERSION, *m_foundation, PxCookingParams(m_physics->getTolerancesScale()));
        if (!m_cooking)
            log::exit("PxCreateCooking failed!");

        // Create the scene
        PxSceneDesc sceneDesc(m_physics->getTolerancesScale());
        sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f); // Gravity along the negative Y-axis
        sceneDesc.filterShader = &PxDefaultSimulationFilterShader;
        static auto* mCpuDispatcher = PxDefaultCpuDispatcherCreate(4);
        if(!mCpuDispatcher)
            log::exit("PxDefaultCpuDispatcherCreate failed!");
        sceneDesc.cpuDispatcher = mCpuDispatcher;
        m_scene = m_physics->createScene(sceneDesc);
        if (!m_scene)
            log::exit("Error creating the PhysX scene.");

        //m_scene->getScenePvdClient()->setScenePvdFlags(PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS | PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES | PxPvdSceneFlag::eTRANSMIT_CONTACTS);
    }

    PXInitializer::~PXInitializer()
    {
        m_cooking->release();
        m_physics->release();
        m_pvd->release();
        m_foundation->release();
    }

    PxTriangleMesh* PXInitializer::BuildTriangleMesh(const physx::PxTriangleMeshDesc& meshDesc)
    {
        if(m_cooking == nullptr)
            return nullptr;
        return m_cooking->createTriangleMesh(meshDesc, m_physics->getPhysicsInsertionCallback());
    }

    PxConvexMesh* PXInitializer::BuildConvexMesh(const physx::PxConvexMeshDesc& meshDesc)
    {
        if(m_cooking == nullptr)
            return nullptr;
        return m_cooking->createConvexMesh(meshDesc, m_physics->getPhysicsInsertionCallback());
    }

    void PXInitializer::simulate()
    {
        for (int i = 0; i < 100; ++i)
        {
            m_scene->simulate(1.0f / 60.0f); // Simulate for 1/60th of a second
            m_scene->fetchResults(true); // Block until simulation is complete
        }
    }
}
