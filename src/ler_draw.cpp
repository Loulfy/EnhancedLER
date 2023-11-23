//
// Created by loulfy on 09/09/2023.
//

#include "ler_draw.hpp"

namespace ler
{
    void to_json(json& j, const RenderDesc& r)
    {

    }

    void from_json(const json& j, RenderDesc& r)
    {
        r.name = j["name"];
        r.type = j["type"];
        r.binding = j["binding"];

        if(r.type == RS_StorageBuffer)
        {
            r.buffer.byteSize = j["byteSize"];
            r.buffer.usage = vk::BufferUsageFlags(j["usage"].get<uint32_t>());
        }
        else if(r.type == RS_RenderTarget || r.type == RS_DepthWrite)
        {
            r.texture.format = j["format"];
            r.texture.loadOp = j["loadOp"];
        }
    }

    RenderGraph::~RenderGraph()
    {
        for(RenderGraphNode& node : m_nodes)
            delete node.pass;
    }

    void RenderGraph::parse(const fs::path& path)
    {
        Blob bin = FileSystemService::Get(FsTag_Assets)->readFile(path);
        try
        {
            m_info = json::parse(bin);
        }
        catch(const std::exception& e)
        {
            log::error("Failed to parse RenderGraph: {}", path.string());
        }

        for(size_t i = 0; i < m_info.passes.size(); ++i)
        {
            RenderPassDesc& pass = m_info.passes[i];
            RenderGraphNode& node = m_nodes.emplace_back();
            node.index = i;
            node.type = pass.pass;
            node.name = pass.name;
            for (RenderDesc& desc : pass.resources)
            {
                if(m_resourceMap.contains(desc.name))
                {
                    desc.handle = m_resourceMap.at(desc.name);
                    node.bindings.emplace_back(desc);
                }
                else
                {
                    //log::error("[RenderPass] {} can not found input: {}", pass.name, ref.name);
                    desc.handle = m_resourceCache.size();
                    node.bindings.emplace_back(desc);
                    m_resourceCache.emplace_back();
                    m_resourceMap.emplace(desc.name, desc.handle);
                }

                if(guessOutput(desc))
                    node.outputs.insert(desc.handle);
            }
        }

        for(RenderGraphNode& node : m_nodes)
            computeEdges(node);

        topologicalSort();
        for(RenderGraphNode& node : m_nodes)
            log::debug("[RenderGraph] Create Node: {}", node.name);
    }

    void RenderGraph::compile(const LerDevicePtr& device, const vk::Extent2D& viewport)
    {
        log::debug("[RenderGraph] Compile");
        samplerGlobal = device->createSampler(vk::SamplerAddressMode::eRepeat, true);

        for(RenderGraphNode& node : m_nodes)
        {
            for(RenderDesc& desc : node.bindings)
            {
                if(desc.type == RS_StorageBuffer)
                {
                    log::debug("[RenderGraph] Add buf: {:10s} -> {}", desc.name, vk::to_string(desc.buffer.usage));
                    m_resourceCache[desc.handle] = device->createBuffer(desc.buffer.byteSize, vk::BufferUsageFlags(desc.buffer.usage), true);
                }
            }

            if(node.pass == nullptr)
            {
                log::error("[RenderGraph] Node: {} Not Implemented", node.name);
                throw std::runtime_error("RenderGraph Node Not Implemented");
            }

            node.pass->create(device, *this, node.bindings);
            PipelinePtr pipeline = node.pass->getPipeline();
            node.descriptor = pipeline->createDescriptorSet(0);
            for(RenderDesc& res : node.bindings)
                bindResource(pipeline, res, node.descriptor);
        }
    }

    void RenderGraph::setRenderAttachment(const RenderDesc& desc, vk::RenderingAttachmentInfo& info)
    {
        RenderResource& r = m_resourceCache[desc.handle];
        if(std::holds_alternative<TexturePtr>(r))
        {
            TexturePtr texture = std::get<TexturePtr>(r);
            info.setImageView(texture->view());
            info.setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal);
            info.setLoadOp(desc.texture.loadOp);
            info.setStoreOp(vk::AttachmentStoreOp::eStore);
        }
    }

    void RenderGraph::resize(const LerDevicePtr& device, const vk::Extent2D& viewport)
    {
        log::debug("[RenderGraph] Resize");
        for(RenderGraphNode& node : m_nodes)
        {
            for(RenderDesc& desc : node.bindings)
            {
                if(desc.type == RS_RenderTarget || desc.type == RS_DepthWrite)
                {
                    log::debug("[RenderGraph] Add tex: {:10s} -> {}", desc.name, vk::to_string(desc.texture.format));
                    m_resourceCache[desc.handle] = device->createTexture(desc.texture.format, viewport, vk::SampleCountFlagBits::e1, true);
                }
            }

            if (node.pass == nullptr)
                continue;
            node.pass->resize(device, viewport);
            node.rendering.viewport = viewport;
            node.rendering.colorCount = 0;
            vk::RenderingAttachmentInfo* info;
            PipelinePtr pipeline = node.pass->getPipeline();
            for(const RenderDesc& desc : node.bindings)
            {
                switch(desc.type)
                {
                    case RS_RenderTarget:
                        node.rendering.colorCount+= 1;
                        info = &node.rendering.colors[desc.binding];
                        setRenderAttachment(desc, *info);
                        info->setClearValue(vk::ClearColorValue(Color::White));
                        info->setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
                        break;
                    case RS_DepthWrite:
                        info = &node.rendering.depth;
                        setRenderAttachment(desc, *info);
                        info->setClearValue(vk::ClearDepthStencilValue(1.f, 0.f));
                        break;
                    default:
                        break;
                }

                if(pipeline)
                    bindResource(pipeline, desc, node.descriptor);
            }
        }
    }

    void RenderGraph::execute(CommandPtr& cmd, TexturePtr& backBuffer, const SceneBuffers& sb, const RenderParams& params)
    {
        for(size_t i = 0; i < m_nodes.size(); ++i)
        {
            RenderGraphNode& node = m_nodes[i];
            vk::DebugUtilsLabelEXT markerInfo;
            auto& color = Color::Palette[i];
            memcpy(markerInfo.color, color.data(), sizeof(float) * 4);
            markerInfo.pLabelName = node.name.c_str();

            // Begin Pass
            cmd->cmdBuf.beginDebugUtilsLabelEXT(markerInfo);

            // Apply Barrier
            for(RenderDesc& desc : node.bindings)
            {
                if(desc.name == RT_BackBuffer && node.type == RP_Graphics)
                    node.rendering.colors[desc.binding].setImageView(backBuffer->view());
                ResourceState state = guessState(desc);
                applyBarrier(cmd, desc, state);
            }

            // Begin Rendering
            if(node.type == RP_Graphics)
                cmd->beginRenderPass(node.rendering);

            // Render
            if(node.pass)
            {
                cmd->bindPipeline(node.pass->getPipeline(), node.descriptor);
                node.pass->render(cmd, sb, params);
            }

            // End Pass
            if(node.type == RP_Graphics)
                cmd->cmdBuf.endRendering();
            cmd->cmdBuf.endDebugUtilsLabelEXT();
        }
    }

    void RenderGraph::onSceneChange(SceneBuffers* scene)
    {
        for(RenderGraphNode& node : m_nodes)
        {
            if(node.pass == nullptr)
                continue;

            PipelinePtr pipeline = node.pass->getPipeline();
            for(RenderDesc& res : node.bindings)
                bindResource(pipeline, res, node.descriptor);
        }
    }

    void RenderGraph::addResource(const std::string& name, const RenderResource& res)
    {
        m_resourceMap.emplace(name, uint32_t(m_resourceCache.size()));
        m_resourceCache.emplace_back(res);
    }

    bool RenderGraph::getResource(uint32_t handle, BufferPtr& buffer) const
    {
        if(m_resourceCache.size() < handle)
            return false;

        if(!std::holds_alternative<BufferPtr>(m_resourceCache[handle]))
            return false;

        buffer = std::get<BufferPtr>(m_resourceCache[handle]);
        return true;
    }

    bool RenderGraph::getResource(const std::string& name, BufferPtr& buffer) const
    {
        if(m_resourceMap.contains(name))
            return getResource(m_resourceMap.at(name), buffer);
        return false;
    }

    ResourceState RenderGraph::guessState(const RenderDesc& res)
    {
        switch(res.type)
        {
            case RS_DepthWrite:
                return DepthWrite;
            case RS_RenderTarget:
                return RenderTarget;
            case RS_SampledTexture:
                return ShaderResource;
            case RS_ReadOnlyBuffer:
                return Indirect;
            case RS_StorageBuffer:
                return UnorderedAccess;
            default:
                return Undefined;
        }
    }

    bool RenderGraph::guessOutput(const RenderDesc& res)
    {
        switch(res.type)
        {
            default:
                return false;
            case RS_DepthWrite:
            case RS_RenderTarget:
            case RS_StorageImage:
            case RS_StorageBuffer:
                return true;
        }
    }

    void RenderGraph::applyBarrier(CommandPtr& cmd, const RenderDesc& desc, ResourceState state)
    {
        RenderResource& r = m_resourceCache[desc.handle];
        if (std::holds_alternative<BufferPtr>(r))
        {
            BufferPtr buf = std::get<BufferPtr>(r);
            cmd->addBufferBarrier(buf, state);
        }
        else if (std::holds_alternative<TexturePtr>(r))
        {
            TexturePtr tex = std::get<TexturePtr>(r);
            cmd->addImageBarrier(tex, state);
        }
    }

    void RenderGraph::bindResource(const PipelinePtr& pipeline, const RenderDesc& res, vk::DescriptorSet descriptor)
    {
        if(res.handle == UINT32_MAX || res.binding == UINT32_MAX)
            return;

        RenderResource& r = m_resourceCache[res.handle];
        if (std::holds_alternative<BufferPtr>(r))
        {
            auto& buf = std::get<BufferPtr>(r);
            pipeline->updateStorage(descriptor, res.binding, buf, VK_WHOLE_SIZE);
        }
        else if (std::holds_alternative<TexturePtr>(r) && res.type == RS_SampledTexture)
        {
            auto& tex = std::get<TexturePtr>(r);
            pipeline->updateSampler(descriptor, res.binding, samplerGlobal.get(), vk::ImageLayout::eReadOnlyOptimal, tex->view());
        }
        else if (std::holds_alternative<TexturePoolPtr>(r))
        {
            auto& pool = std::get<TexturePoolPtr>(r);
            if(pool->getTextureCount() > 1)
                pipeline->updateSampler(descriptor, res.binding, samplerGlobal.get(), pool->getTextures());
        }
    }

    void RenderGraph::computeEdges(RenderGraphNode& node)
    {
        for(const RenderDesc& desc : node.bindings)
        {
            if(guessOutput(desc))
                continue;

            for(auto& parent : m_nodes)
            {
                if(parent.outputs.contains(desc.handle) && std::find(parent.edges.begin(), parent.edges.end(), &node) == parent.edges.end())
                {
                    parent.edges.emplace_back(&node);
                    break;
                }
            }
        }
    }

    void RenderGraph::topologicalSort()
    {
        enum Status {
            New = 0u, Visited, Added
        };

        std::stack<RenderGraphNode*> stack;
        std::vector<u8> node_status(m_nodes.size(), New);

        std::vector<RenderGraphNode> sp = std::move(m_nodes);

        // Topological sorting
        for (auto& node : sp)
        {
            stack.push(&node);

            while (!stack.empty())
            {
                RenderGraphNode* node_handle = stack.top();

                if (node_status[node_handle->index] == Added)
                {
                    stack.pop();
                    continue;
                }

                if (node_status[node_handle->index] == Visited)
                {
                    node_status[node_handle->index] = Added;
                    m_nodes.emplace_back(std::move(*node_handle));
                    stack.pop();
                    continue;
                }

                node_status[node_handle->index] = Visited;

                // Leaf node
                for (auto child_handle : node_handle->edges)
                {
                    if (node_status[child_handle->index] == New)
                        stack.push(child_handle);
                }
            }
        }

        // Reverse
        std::reverse(m_nodes.begin(), m_nodes.end());
    }

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

    uint32_t RenderSceneList::getInstanceCount() const
    {
        return m_instances.size();
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