#include <iostream>

#include "ler_app.hpp"

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
    }

    void resize(const ler::LerDevicePtr& device, vk::Extent2D extent) override
    {
        m_depth = device->createTexture(vk::Format::eD32Sfloat, extent, vk::SampleCountFlagBits::e1, true);
        device->initTextureLayout(m_depth, vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::AccessFlagBits::eDepthStencilAttachmentWrite);
        device->setRenderTarget(ler::RT::eDepth, m_depth);
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
        cam.proj[1][1] *= -1;

        ler::CommandPtr cmd = device->createCommand();

        // Depth Pass
        sceneList.sort(cmd, cam, true);
        cmd->executePass({
            .viewport = frame.extent,
            .depthStencilAttachment = {.texture = m_depth, .loadOp = vk::AttachmentLoadOp::eClear},
        });
        cmd->bindPipeline(prePass, descriptorPrePass);
        cmd->cmdBuf.pushConstants(prePass->pipelineLayout.get(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(ler::CameraParam), &cam);
        sceneList.getSceneBuffers().bind(cmd, true);
        sceneList.draw(cmd);
        cmd->end();

        device->submitAndWait(cmd);
        cmd = device->createCommand();

        // AABB Pass
        cmd->executePass({
            .viewport = frame.extent,
            .colorAttachments = {{.texture = frame.image, .loadOp = vk::AttachmentLoadOp::eClear}},
            .depthStencilAttachment = {.texture = m_depth, .loadOp = vk::AttachmentLoadOp::eLoad},
        });
        cmd->bindPipeline(boxPass, nullptr);
        cmd->cmdBuf.pushConstants(boxPass->pipelineLayout.get(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(ler::CameraParam), &cam);
        sceneList.drawAABB(cmd);
        cmd->end();

        sceneList.generate(m_depth, cmd);
        sceneList.sort(cmd, cam, false);
        cmd->executePass({
            .viewport = frame.extent,
            .colorAttachments = {{.texture = frame.image, .loadOp = vk::AttachmentLoadOp::eLoad}},
            .depthStencilAttachment = {.texture = m_depth, .loadOp = vk::AttachmentLoadOp::eLoad},
        });
        cmd->bindPipeline(pipeline, descriptor);
        cmd->cmdBuf.pushConstants(pipeline->pipelineLayout.get(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(ler::CameraParam), &cam);
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

int main()
{
    ler::LerApp app({1280, 720});
    app.addPass<ForwardPass>();
    app.addPass<ler::TextureViewerPass>();
    app.addPass<ler::ImGuiPass>();
    app.loadSceneAsync("kitty.fbx");
    //app.loadSceneAsync("sponza.glb");
    app.run();
    return EXIT_SUCCESS;
}
