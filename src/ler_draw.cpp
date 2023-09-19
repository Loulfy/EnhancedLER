//
// Created by loulfy on 09/09/2023.
//

#include "ler_draw.hpp"

namespace ler
{
    void RenderSceneList::allocate(const LerDevicePtr& device)
    {
        using bu = vk::BufferUsageFlagBits;
        m_instanceBuffer = device->createBuffer(C16Mio, bu::eStorageBuffer);
        m_staging = device->createBuffer(C16Mio, vk::BufferUsageFlagBits(), true);

        m_sceneBuffers.allocate(device);
        auto cullBuffer = std::array<BufferPtr,2>
        {
            m_instanceBuffer,
            m_sceneBuffers.getMeshletBuffer(),
        };
        m_culling.init(device, cullBuffer);

        // Debug Box
        m_lines.reserve(2048);
        m_aabbBuffer = device->createBuffer(C16Mio, bu::eVertexBuffer);
    }

    const BufferPtr& RenderSceneList::getInstanceBuffers() const
    {
        return m_instanceBuffer;
    }

    const BufferPtr& RenderSceneList::getCommandBuffers() const
    {
        return m_culling.getCommandBuffers();
    }

    SceneBuffers& RenderSceneList::getSceneBuffers()
    {
        return m_sceneBuffers;
    }

    uint32_t RenderSceneList::getDrawCount() const
    {
        uint32_t test;
        m_culling.getCountBuffer()->getUint(&test);
        return test;
        //return m_culling.drawCount;
    }

    uint32_t RenderSceneList::getLineCount() const
    {
        return m_lines.size();
    }

    void RenderSceneList::install(flecs::world& world, const LerDevicePtr& device)
    {
        static constexpr uint32_t kInstSize = sizeof(Instance);

        world.observer<CTransform, CMesh, CMaterial>().event(flecs::OnSet).each([&](flecs::entity e, CTransform& t, CMesh& m, CMaterial& mat) {
            unsigned int instanceId = m_instances.size();

            m_instances.emplace_back(t.model, m.bounds, m.min, m.max, mat.materialId, m.meshIndex);
            e.set<CInstance>({instanceId});
            e.add<dirty>();
            addAABB(m, t);
        });

        world.observer<CTransform>().event(flecs::OnSet).each([&](flecs::entity e, CTransform& t) {

        });

        world.system<CInstance, dirty>().kind(flecs::OnUpdate).each([&](flecs::entity e, CInstance& i, dirty){
            uint32_t offset = m_patches.size() * kInstSize;
            vk::BufferCopy& region = m_patches.emplace_back();
            region.setSize(kInstSize);
            region.setSrcOffset(offset);
            region.setDstOffset(i.instanceId * kInstSize);
            auto& t = m_instances[i.instanceId];
            void* data = m_staging->hostInfo.pMappedData;
            auto* dest = static_cast<std::byte*>(data);
            std::memcpy(dest+offset, &t, kInstSize);
        });

        world.system().kind(flecs::PostUpdate).iter([&](flecs::iter& it) {
            it.world().remove_all<dirty>();
            if(m_patches.empty())
                return;

            CommandPtr cmd = device->createCommand();
            cmd->cmdBuf.copyBuffer(m_staging->handle, m_instanceBuffer->handle, m_patches);
            device->submitAndWait(cmd);
            m_patches.clear();

            cmd = device->createCommand();
            uint32_t byteSize = m_lines.size()*sizeof(glm::vec3);
            m_staging->uploadFromMemory(m_lines.data(), byteSize);
            cmd->copyBuffer(m_staging, m_aabbBuffer, byteSize);
            device->submitAndWait(cmd);
        });
    }

    void RenderSceneList::sort(const CommandPtr& cmd, const CameraParam& camera, bool prePass)
    {
        m_culling.dispatch(cmd, camera, m_instances.size(), prePass);
    }

    void RenderSceneList::draw(const CommandPtr& cmd)
    {
        constexpr static vk::DeviceSize offset = 0;
        cmd->cmdBuf.drawIndexedIndirectCount(m_culling.getCommandBuffers()->handle, offset, m_culling.getCountBuffer()->handle, offset, m_instances.size(), sizeof(DrawCommand));
    }

    void RenderSceneList::generate(const TexturePtr& depth, const CommandPtr& cmd)
    {
        m_culling.renderDepthPyramid(depth, cmd);
    }

    void RenderSceneList::resize(const LerDevicePtr& device, vk::Extent2D extent)
    {
        m_culling.createDepthPyramid(device, extent);
    }

    void RenderSceneList::drawAABB(const CommandPtr& cmd)
    {
        constexpr static vk::DeviceSize offset = 0;
        //cmd->cmdBuf.pushConstants(boxPass->pipelineLayout.get(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(ler::CameraParam), &test);
        cmd->cmdBuf.bindVertexBuffers(0, 1, &m_aabbBuffer->handle, &offset);
        cmd->draw(getLineCount());
    }

    void RenderSceneList::addLine(const glm::vec3& p1, const glm::vec3& p2)
    {
        m_lines.push_back(p1);
        m_lines.push_back(p2);
    }

    void RenderSceneList::addAABB(const CMesh& mesh, const CTransform& transform)
    {
        auto pts = createBox(mesh, transform);
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

    std::array<glm::vec3, 8> RenderSceneList::createBox(const CMesh& mesh, const CTransform& transform)
    {
        glm::vec3 min = mesh.min;
        glm::vec3 max = mesh.max;
        std::array<glm::vec3, 8> pts = {
                glm::vec3(max.x, max.y, max.z),
                glm::vec3(max.x, max.y, min.z),
                glm::vec3(max.x, min.y, max.z),
                glm::vec3(max.x, min.y, min.z),
                glm::vec3(min.x, max.y, max.z),
                glm::vec3(min.x, max.y, min.z),
                glm::vec3(min.x, min.y, max.z),
                glm::vec3(min.x, min.y, min.z),
        };
        for (auto& p: pts)
            p = glm::vec3(transform.model * glm::vec4(p, 1.f));
        return pts;
    }
}