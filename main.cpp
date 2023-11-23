#include <iostream>

#include "ler_app.hpp"
//#include <FidelityFX/host/ffx_classifier.h>
//#include <FidelityFX/host/ffx_denoiser.h>
//#include <FidelityFX/host/backends/vk/ffx_vk.h>

/*void onSceneChange(ler::LerDevicePtr& device, ler::BatchedMesh& batch) override
{
    ler::log::info("hello scene loaded");
    ler::TexturePtr shadowMap = device->getRenderTarget(ler::RT::eShadowMap);
    vk::ImageView view = shadowMap->view(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 4));
    device->updateStorage(descriptor, 0, batch.instanceBuffer, VK_WHOLE_SIZE);
    device->updateStorage(descriptor, 1, batch.materialBuffer, VK_WHOLE_SIZE);
    device->updateSampler(descriptor, 2, samplerGlobal.get(), pool->getTextures());
    device->updateSampler(descriptor, 3, samplerShadow.get(), vk::ImageLayout::eDepthReadOnlyOptimal, std::span(&view, 1));
    device->updateStorage(descriptor, 4, light, sizeof(ler::SceneParam), true);
    device->updateStorage(descriptor, 5, batch.instanceBuffer, VK_WHOLE_SIZE);
    device->updateStorage(descriptor, 6, batch.commandBuffer, VK_WHOLE_SIZE);

    culling.prepare(device, batch);

    // PrePass Depth
    device->updateStorage(descriptorPrePass, 0, batch.instanceBuffer, VK_WHOLE_SIZE);
    device->updateStorage(descriptorPrePass, 1, batch.commandBuffer, VK_WHOLE_SIZE);

    // Depth PrePass
    shaders.clear();
    info.colorAttach.clear();
    info.topology = vk::PrimitiveTopology::eTriangleList;
    shaders.emplace_back(device->createShader("depth.vert.spv"));
    prePass = device->createGraphicsPipeline(shaders, info);
    descriptorPrePass = prePass->createDescriptorSet(0);
}*/

class ForwardPass : public ler::IRenderPass
{
public:

    //FfxInterface                    m_SDKInterface;
    //FfxClassifierContextDescription m_ClassifierCtxDesc = {};
    //FfxClassifierContext            m_ClassifierContext;

    ~ForwardPass() override
    {
        //ffxClassifierContextDestroy(&m_ClassifierContext);
        //ffxDenoiserContextDestroy(&m_DenoiserContext);
        //free(m_SDKInterface.scratchBuffer);
    }

    void init(const ler::LerDevicePtr& device, ler::RenderSceneList& scene, GLFWwindow* window) override
    {
        std::vector<ler::ShaderPtr> shaders;
        shaders.emplace_back(device->createShader("mesh.vert.spv"));
        shaders.emplace_back(device->createShader("mesh.frag.spv"));

        ler::PipelineInfo info;
        info.textureCount = 300;
        info.writeDepth = true;
        info.polygonMode = vk::PolygonMode::eFill;
        info.topology = vk::PrimitiveTopology::eTriangleList;
        info.colorAttach.emplace_back(vk::Format::eB8G8R8A8Unorm);
        info.depthAttach = vk::Format::eD32Sfloat;
        pipeline = device->createGraphicsPipeline(shaders, info);
        descriptor = pipeline->createDescriptorSet(0);

        // AABB Pass
        std::vector<ler::ShaderPtr> aabbShaders;
        aabbShaders.push_back(device->createShader("aabb.vert.spv"));
        aabbShaders.push_back(device->createShader("aabb.frag.spv"));
        info.textureCount = 0;
        info.topology = vk::PrimitiveTopology::eLineList;
        info.lineWidth = 3.f;
        boxPass = device->createGraphicsPipeline(aabbShaders, info);

        // Depth PrePass
        shaders.clear();
        info.colorAttach.clear();
        info.topology = vk::PrimitiveTopology::eTriangleList;
        shaders.emplace_back(device->createShader("depth.vert.spv"));
        prePass = device->createGraphicsPipeline(shaders, info);
        descriptorPrePass = prePass->createDescriptorSet(0);

        // PrePass Depth
        device->updateStorage(descriptorPrePass, 0, scene.getInstanceBuffers(), VK_WHOLE_SIZE);
        device->updateStorage(descriptorPrePass, 1, scene.getCommandBuffers(), VK_WHOLE_SIZE);

        m_light = device->createBuffer(sizeof(ler::SceneParam), vk::BufferUsageFlagBits::eUniformBuffer, true);
        samplerGlobal = device->createSampler(vk::SamplerAddressMode::eRepeat, true);
        samplerShadow = device->createSampler(vk::SamplerAddressMode::eClampToEdge, true);

        // Vertex Shader
        device->updateStorage(descriptor, 0, scene.getInstanceBuffers(), VK_WHOLE_SIZE);
        device->updateStorage(descriptor, 6, scene.getCommandBuffers(), VK_WHOLE_SIZE);

        // Fragment Shader
        device->updateStorage(descriptor, 1, scene.getSceneBuffers().getMaterialBuffer(), VK_WHOLE_SIZE);
        device->updateStorage(descriptor, 4, m_light, sizeof(ler::SceneParam), true);
        device->updateStorage(descriptor, 5, scene.getInstanceBuffers(), VK_WHOLE_SIZE);

        /*
        size_t maxContexts = FFX_CLASSIFIER_CONTEXT_COUNT + FFX_DENOISER_CONTEXT_COUNT;
        const ler::VulkanContext& ctx = device->getVulkanContext();
        const size_t scratchBufferSize = ffxGetScratchMemorySizeVK(ctx.physicalDevice, maxContexts);
        void*        scratchBuffer     = malloc(scratchBufferSize);
        VkDeviceContext vkDeviceContext = { ctx.device, ctx.physicalDevice, vkGetDeviceProcAddr };
        ffxGetInterfaceVK(&m_SDKInterface, ffxGetDeviceVK(&vkDeviceContext), scratchBuffer, scratchBufferSize, maxContexts);*/
    }

    void resize(const ler::LerDevicePtr& device, vk::Extent2D extent) override
    {
        m_depth = device->createTexture(vk::Format::eD32Sfloat, extent, vk::SampleCountFlagBits::e1, true);
        device->initTextureLayout(m_depth, vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::AccessFlagBits::eDepthStencilAttachmentWrite);
        device->setRenderTarget(ler::RT::eDepth, m_depth);

        /*
        m_ClassifierCtxDesc.flags = FFX_CLASSIFIER_SHADOW;
        m_ClassifierCtxDesc.flags |= FFX_CLASSIFIER_CLASSIFY_BY_NORMALS;
        m_ClassifierCtxDesc.resolution.width = extent.width;
        m_ClassifierCtxDesc.resolution.height = extent.height;
        m_ClassifierCtxDesc.backendInterface = m_SDKInterface;
        FFX_ASSERT(ffxClassifierContextCreate(&m_ClassifierContext, &m_ClassifierCtxDesc) == FFX_OK);

        FfxClassifierShadowDispatchDescription shadowClassifierDispatchParams = {};*/
    }

    void onSceneChange(const ler::LerDevicePtr& device, ler::SceneBuffers* scene) override
    {
        ler::log::info("hello scene loaded");
        ler::TexturePoolPtr pool = device->getTexturePool();
        device->updateSampler(descriptor, 2, samplerGlobal.get(), pool->getTextures());
    }

    void render(const ler::LerDevicePtr& device, ler::FrameWindow& frame, ler::RenderSceneList& sceneList, ler::RenderParams& params) override
    {
        ler::CameraParam cam;
        cam.proj = params.camera.proj;
        cam.view = params.camera.view;
        cam.test = params.camera.test;
        cam.proj[1][1] *= -1;

        ler::CommandPtr cmd;

        /*
        ler::CommandPtr cmd = device->createCommand();

        // Depth Pass
        sceneList.sort(cmd, cam, true);
        cmd->executePass({
            .viewport = frame.extent,
            .depthStencilAttachment = {.texture = m_depth, .loadOp = vk::AttachmentLoadOp::eClear},
        });
        cmd->bindPipeline(prePass, descriptorPrePass);
        cmd->cmdBuf.pushConstants(prePass->pipelineLayout.get(), vk::ShaderStageFlagBits::eVertex, 0, 128, &cam);
        sceneList.getSceneBuffers().bind(cmd, true);
        sceneList.draw(cmd);
        cmd->end();

        device->submitAndWait(cmd);*/
        cmd = device->createCommand();

        // AABB Pass
        cmd->executePass({
            .viewport = frame.extent,
            .colorAttachments = {{.texture = frame.image, .loadOp = vk::AttachmentLoadOp::eClear}},
            .depthStencilAttachment = {.texture = m_depth, .loadOp = vk::AttachmentLoadOp::eLoad},
        });
        cmd->bindPipeline(boxPass, nullptr);
        cmd->cmdBuf.pushConstants(boxPass->pipelineLayout.get(), vk::ShaderStageFlagBits::eVertex, 0, 128, &cam);
        sceneList.drawAABB(cmd);
        cmd->end();

        sceneList.generate(m_depth, cmd);
        sceneList.sort(cmd, cam, false);
        cmd->executePass({
            .viewport = frame.extent,
            .colorAttachments = {{.texture = frame.image, .loadOp = vk::AttachmentLoadOp::eClear}},
            .depthStencilAttachment = {.texture = m_depth, .loadOp = vk::AttachmentLoadOp::eClear},
        });
        cmd->bindPipeline(pipeline, descriptor);
        cmd->cmdBuf.pushConstants(pipeline->pipelineLayout.get(), vk::ShaderStageFlagBits::eVertex, 0, 128, &cam);
        sceneList.getSceneBuffers().bind(cmd, false);
        sceneList.draw(cmd);
        cmd->end();
        device->submitAndWait(cmd);
        ImGui::Begin("Scene Renderer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("DrawCount: %d", sceneList.getDrawCount());
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
    }

private:

    ler::PipelinePtr pipeline;
    ler::PipelinePtr boxPass;
    ler::PipelinePtr prePass;
    vk::UniqueSampler samplerGlobal;
    vk::UniqueSampler samplerShadow;
    ler::TexturePtr m_depth;
    ler::BufferPtr m_light;
    vk::DescriptorSet descriptor = nullptr;
    vk::DescriptorSet descriptorPrePass = nullptr;
};

class TrianglePass : public ler::RenderGraphPass
{
public:

    void create(const ler::LerDevicePtr& device, ler::RenderGraph& graph, std::span<ler::RenderDesc> resources) override
    {
        std::vector<ler::ShaderPtr> shaders;
        shaders.emplace_back(device->createShader("mesh.vert.spv"));
        shaders.emplace_back(device->createShader("mesh.frag.spv"));

        ler::PipelineInfo info;
        info.textureCount = 300;
        info.writeDepth = true;
        info.polygonMode = vk::PolygonMode::eFill;
        info.topology = vk::PrimitiveTopology::eTriangleList;
        //info.topology = vk::PrimitiveTopology::eTriangleStrip;
        info.colorAttach.emplace_back(vk::Format::eB8G8R8A8Unorm);
        info.depthAttach = vk::Format::eD32Sfloat;
        pipeline = device->createGraphicsPipeline(shaders, info);
        graph.getResource(resources[1].handle, m_drawsBuffer);
        graph.getResource(resources[2].handle, m_countBuffer);
    }

    [[nodiscard]] ler::PipelinePtr getPipeline() const override { return pipeline; }

    void render(ler::CommandPtr& cmd, const ler::SceneBuffers& sb, ler::RenderParams params) override
    {
        //cmd->draw(4);

        ler::CameraParam cam;
        cam.proj = params.camera.proj;
        cam.view = params.camera.view;
        cam.proj[1][1] *= -1;

        sb.bind(cmd, false);
        constexpr static vk::DeviceSize offset = 0;
        cmd->cmdBuf.pushConstants(pipeline->pipelineLayout.get(), vk::ShaderStageFlagBits::eVertex, 0, 128, &cam);
        cmd->cmdBuf.drawIndexedIndirectCount(m_drawsBuffer->handle, offset, m_countBuffer->handle, offset, params.scene.instanceCount, sizeof(DrawCommand));
    }

private:

    ler::PipelinePtr pipeline;
    vk::DescriptorSet descriptor;
    ler::BufferPtr m_drawsBuffer;
    ler::BufferPtr m_countBuffer;
};

class CullingPass : public ler::RenderGraphPass
{
public:

    void create(const ler::LerDevicePtr& device, ler::RenderGraph& graph, std::span<ler::RenderDesc> resources) override
    {
        ler::ShaderPtr shader = device->createShader("generate_draws.comp.spv");
        pipeline = device->createComputePipeline(shader);
        graph.getResource(resources[2].handle, m_frustumBuffer);
        graph.getResource(resources[4].handle, m_visibleBuffer);
    }

    [[nodiscard]] ler::PipelinePtr getPipeline() const override { return pipeline; }

    [[nodiscard]] std::string getName() const override { return "CullingPass"; }

    void render(ler::CommandPtr& cmd, const ler::SceneBuffers& sb, ler::RenderParams params) override
    {
        m_frustum.cull = 0;
        m_frustum.num = params.scene.instanceCount;
        m_frustum.camera = params.camera.test;
        ler::CameraParam& camera = params.camera;
        m_visibleBuffer->getUint(&drawCount);
        ler::LerDevice::getFrustumPlanes(camera.proj * camera.view, m_frustum.planes);
        ler::LerDevice::getFrustumCorners(camera.proj * camera.view, m_frustum.corners);
        static constexpr uint32_t resetNum = 0;
        m_visibleBuffer->uploadFromMemory(&resetNum, sizeof(uint32_t));
        m_frustumBuffer->uploadFromMemory(&m_frustum, sizeof(Frustum));

        cmd->cmdBuf.pushConstants(pipeline->pipelineLayout.get(), vk::ShaderStageFlagBits::eCompute, 0, 128, &camera);
        cmd->cmdBuf.dispatch(1 + m_frustum.num / 64, 1, 1);
    }

private:

    Frustum m_frustum;
    ler::PipelinePtr pipeline;
    vk::DescriptorSet descriptor;
    ler::BufferPtr m_frustumBuffer;
    ler::BufferPtr m_visibleBuffer;
    uint32_t drawCount = 0;
};

class GBufferPass : public ler::RenderGraphPass
{
public:

    void create(const ler::LerDevicePtr& device, ler::RenderGraph& graph, std::span<ler::RenderDesc> resources) override
    {
        std::vector<ler::ShaderPtr> shaders;
        shaders.emplace_back(device->createShader("gbuffer.vert.spv"));
        shaders.emplace_back(device->createShader("gbuffer.frag.spv"));

        ler::PipelineInfo info;
        info.textureCount = 300;
        info.writeDepth = true;
        info.polygonMode = vk::PolygonMode::eFill;
        info.topology = vk::PrimitiveTopology::eTriangleList;
        info.colorAttach.emplace_back(vk::Format::eR16G16B16A16Sfloat);
        info.colorAttach.emplace_back(vk::Format::eR16G16B16A16Sfloat);
        info.colorAttach.emplace_back(vk::Format::eR8G8B8A8Unorm);
        info.depthAttach = vk::Format::eD32Sfloat;
        pipeline = device->createGraphicsPipeline(shaders, info);
        graph.getResource(resources[1].handle, m_drawsBuffer);
        graph.getResource(resources[2].handle, m_countBuffer);
    }

    [[nodiscard]] ler::PipelinePtr getPipeline() const override { return pipeline; }

    [[nodiscard]] std::string getName() const override { return "GBufferPass"; }

    void render(ler::CommandPtr& cmd, const ler::SceneBuffers& sb, ler::RenderParams params) override
    {
        ler::CameraParam cam;
        cam.proj = params.camera.proj;
        cam.view = params.camera.view;
        cam.proj[1][1] *= -1;

        sb.bind(cmd, false);
        constexpr static vk::DeviceSize offset = 0;
        cmd->cmdBuf.pushConstants(pipeline->pipelineLayout.get(), vk::ShaderStageFlagBits::eVertex, 0, 128, &cam);
        cmd->cmdBuf.drawIndexedIndirectCount(m_drawsBuffer->handle, offset, m_countBuffer->handle, offset, params.scene.instanceCount, sizeof(DrawCommand));
    }

private:

    ler::PipelinePtr pipeline;
    ler::BufferPtr m_drawsBuffer;
    ler::BufferPtr m_countBuffer;
};

class DeferredPass : public ler::RenderGraphPass
{
public:

    void create(const ler::LerDevicePtr& device, ler::RenderGraph& graph, std::span<ler::RenderDesc> resources) override
    {
        std::vector<ler::ShaderPtr> shaders;
        shaders.emplace_back(device->createShader("quad.vert.spv"));
        shaders.emplace_back(device->createShader("deferred.frag.spv"));

        ler::PipelineInfo info;
        info.textureCount = 0;
        info.writeDepth = false;
        info.polygonMode = vk::PolygonMode::eFill;
        info.topology = vk::PrimitiveTopology::eTriangleStrip;
        info.colorAttach.emplace_back(vk::Format::eB8G8R8A8Unorm);
        pipeline = device->createGraphicsPipeline(shaders, info);
    }

    [[nodiscard]] ler::PipelinePtr getPipeline() const override { return pipeline; }

    [[nodiscard]] std::string getName() const override { return "DeferredPass"; }

    void render(ler::CommandPtr& cmd, const ler::SceneBuffers& sb, ler::RenderParams params) override
    {
        cmd->draw(4);
    }

private:

    ler::PipelinePtr pipeline;
};

int main()
{
    ler::LerApp app({1280, 720});
    //app.addPass<ForwardPass>();
    app.addPass<ler::TextureViewerPass>();
    app.addPass<ler::ImGuiPass>();

    app.renderGraph().parse("deferred.json");
    app.renderGraph().addPass<DeferredPass>();
    app.renderGraph().addPass<CullingPass>();
    app.renderGraph().addPass<GBufferPass>();
    
    app.loadSceneAsync("kitty.fbx");
    //app.loadSceneAsync("sponza.glb");
    app.run();
    return EXIT_SUCCESS;
}
