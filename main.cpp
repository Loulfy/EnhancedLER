#include <iostream>

#include "ler_app.hpp"
#include "ler_cull.hpp"
#include <Windows.h>

class DebugPass : public ler::IRenderPass
{
public:

    vk::DescriptorSet descriptor = nullptr;
    vk::UniqueSampler sampler;
    ler::PipelinePtr pipeline;
    ler::TexturePtr texture;
    std::once_flag flag;

    void init(ler::LerDevicePtr& device, GLFWwindow* window) override
    {
        std::vector<ler::ShaderPtr> shaders;
        shaders.emplace_back(device->createShader("quad.vert.spv"));
        shaders.emplace_back(device->createShader("quad.frag.spv"));

        ler::PipelineInfo info;
        info.writeDepth = false;
        info.polygonMode = vk::PolygonMode::eFill;
        info.topology = vk::PrimitiveTopology::eTriangleStrip;
        info.colorAttach.emplace_back(vk::Format::eB8G8R8A8Unorm);
        pipeline = device->createGraphicsPipeline(shaders, info);
        descriptor = pipeline->createDescriptorSet(0);

        sampler = device->createSampler(vk::SamplerAddressMode::eRepeat, true);
        std::call_once(flag, [&](){
            std::cout << "Renderer setup\n";
            auto shadowMap = device->getRenderTarget(ler::RT::eShadowMap);
            auto test = std::span<ler::TexturePtr>(&shadowMap, 1);
            device->updateSampler(descriptor, 0, sampler.get(), test);
        });

    }

    void render(ler::LerDevicePtr& device, ler::FrameWindow& frame, ler::BatchedMesh& batch, const ler::CameraParam& camera, ler::SceneParam& scene) override
    {
        ler::CommandPtr cmd = device->createCommand();

        vk::RenderingAttachmentInfo colorInfo;
        colorInfo.setImageView(frame.image->view());
        colorInfo.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
        colorInfo.setStoreOp(vk::AttachmentStoreOp::eStore);
        colorInfo.setLoadOp(vk::AttachmentLoadOp::eLoad);

        vk::RenderingInfo renderInfo;
        vk::Viewport viewport = ler::LerDevice::createViewport(frame.extent);
        vk::Rect2D renderArea(vk::Offset2D(), frame.extent);
        renderInfo.setRenderArea(renderArea);
        renderInfo.setLayerCount(1);
        renderInfo.setColorAttachmentCount(1);
        renderInfo.setPColorAttachments(&colorInfo);
        cmd->cmdBuf.beginRendering(renderInfo);
        cmd->cmdBuf.setScissor(0, 1, &renderArea);
        cmd->cmdBuf.setViewport(0, 1, &viewport);
        cmd->bindPipeline(pipeline, descriptor);
        cmd->draw(4);
        cmd->cmdBuf.endRendering();
        device->submitCommand(cmd);
    }
};

class ForwardPass : public ler::IRenderPass
{
public:

    ler::PipelinePtr pipeline;
    ler::PipelinePtr prePass;
    ler::PipelinePtr boxPass;
    vk::UniqueSampler samplerGlobal;
    vk::UniqueSampler samplerShadow;
    ler::TexturePtr m_depth;
    vk::DescriptorSet descriptor = nullptr;
    vk::DescriptorSet descriptorPrePass = nullptr;

    ler::TexturePoolPtr pool;
    ler::BufferPtr light;

    ler::InstanceCull culling;

    void init(ler::LerDevicePtr& device, GLFWwindow* window) override
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

        pool = device->getTexturePool();
        pool->fetch("white.png", ler::FsTag_Assets);

        samplerGlobal = device->createSampler(vk::SamplerAddressMode::eRepeat, true);
        samplerShadow = device->createSampler(vk::SamplerAddressMode::eClampToEdge, true);
        m_depth = device->createTexture(vk::Format::eD32Sfloat, vk::Extent2D(1280, 720), vk::SampleCountFlagBits::e1, true);
        device->setRenderTarget(ler::RT::eDepth, m_depth);
        light = device->createBuffer(sizeof(ler::SceneParam), vk::BufferUsageFlagBits::eUniformBuffer, true);

        ler::CommandPtr cmd = device->createCommand();
        using ps = vk::PipelineStageFlagBits;
        std::vector<vk::ImageMemoryBarrier> imageBarriers;
        auto& barrier = imageBarriers.emplace_back();
        barrier.setImage(m_depth->handle);
        barrier.setOldLayout(vk::ImageLayout::eUndefined);
        barrier.setNewLayout(vk::ImageLayout::eDepthAttachmentOptimal);
        barrier.setSrcAccessMask(vk::AccessFlags());
        barrier.setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
        barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1));
        cmd->cmdBuf.pipelineBarrier(ps::eAllCommands, ps::eAllCommands, vk::DependencyFlags(), {}, {}, imageBarriers);
        device->submitAndWait(cmd);

        culling.init(device);
    }

    void resize(ler::LerDevicePtr& device, vk::Extent2D extent) override
    {
        m_depth = device->createTexture(device->chooseDepthFormat(), extent, vk::SampleCountFlagBits::e1, true);
    }

    void onSceneChange(ler::LerDevicePtr& device, ler::BatchedMesh& batch) override
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
    }

    void render(ler::LerDevicePtr& device, ler::FrameWindow& frame, ler::BatchedMesh& batch, const ler::CameraParam& camera, ler::SceneParam& scene) override
    {
        ler::CommandPtr cmd;
        constexpr static vk::DeviceSize offset = 0;
        ler::CameraParam test;
        test.proj = camera.proj;
        test.view = camera.view;
        //test.proj[1][1] *= -1;

        // Depth PrePass
        culling.dispatch(device, test, batch, true);

        cmd = device->createCommand();
        cmd->executePass({.viewport = frame.extent, .depthStencilAttachment = {.texture = m_depth, .loadOp = vk::AttachmentLoadOp::eClear}});
        cmd->bindPipeline(prePass, descriptorPrePass);
        cmd->cmdBuf.pushConstants(prePass->pipelineLayout.get(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(ler::CameraParam), &test);
        cmd->cmdBuf.bindIndexBuffer(batch.indexBuffer->handle, offset, vk::IndexType::eUint32);
        cmd->cmdBuf.bindVertexBuffers(0, 1, &batch.vertexBuffer->handle, &offset);
        cmd->cmdBuf.drawIndexedIndirectCount(batch.commandBuffer->handle, offset, culling.getCountBuffer()->handle, offset, batch.drawCount, sizeof(DrawCommand));
        cmd->end();
        device->submitOneshot(cmd);

        // AABB Pass
        cmd = device->createCommand();
        cmd->executePass({
            .viewport = frame.extent,
            .colorAttachments = {{.texture = frame.image, .loadOp = vk::AttachmentLoadOp::eClear}},
            .depthStencilAttachment = {.texture = m_depth, .loadOp = vk::AttachmentLoadOp::eLoad},
        });
        cmd->bindPipeline(boxPass, nullptr);
        cmd->cmdBuf.pushConstants(boxPass->pipelineLayout.get(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(ler::CameraParam), &test);
        cmd->cmdBuf.bindVertexBuffers(0, 1, &batch.aabbBuffer->handle, &offset);
        cmd->draw(batch.renderList.getLineCount());
        cmd->end();

        device->submitOneshot(cmd);

        // Main Pass
        culling.dispatch(device, test, batch, false);
        light->uploadFromMemory(&scene, sizeof(ler::SceneParam));

        cmd = device->createCommand();

        cmd->executePass({
            .viewport = frame.extent,
            .colorAttachments = {{.texture = frame.image, .loadOp = vk::AttachmentLoadOp::eLoad}},
            .depthStencilAttachment = {.texture = m_depth, .loadOp = vk::AttachmentLoadOp::eLoad},
        });
        cmd->bindPipeline(pipeline, descriptor);

        cmd->cmdBuf.pushConstants(pipeline->pipelineLayout.get(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(ler::CameraParam), &test);
        cmd->cmdBuf.bindIndexBuffer(batch.indexBuffer->handle, offset, vk::IndexType::eUint32);
        cmd->cmdBuf.bindVertexBuffers(0, 1, &batch.vertexBuffer->handle, &offset);
        cmd->cmdBuf.bindVertexBuffers(1, 1, &batch.texcoordBuffer->handle, &offset);
        cmd->cmdBuf.bindVertexBuffers(2, 1, &batch.normalBuffer->handle, &offset);
        cmd->cmdBuf.bindVertexBuffers(3, 1, &batch.tangentBuffer->handle, &offset);

        cmd->cmdBuf.drawIndexedIndirectCount(batch.commandBuffer->handle, offset, culling.getCountBuffer()->handle, offset, batch.drawCount, sizeof(DrawCommand));
        cmd->end();

        device->submitOneshot(cmd);
        ImGui::Begin("Scene Renderer", nullptr, ImGuiWindowFlags_NoResize);
        ImGui::Text("DrawCount: %d", culling.drawCount);
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
    }
};

int main()
{
    ler::LerApp app({1280, 720});
    app.addPass<ler::CascadedShadowPass>();
    app.addPass<ForwardPass>();
    //app.addPass<DebugPass>();
    app.addPass<ler::TextureViewerPass>();
    app.addPass<ler::ImGuiPass>();
    //app.loadSceneAsync(R"(C:\Users\loria\Downloads\Bistro_v5_2\Bistro_v5_2\BistroExterior.fbx)", ler::FsTag_Default);
    app.loadSceneAsync("kitty.fbx");
    app.run();
    return EXIT_SUCCESS;
}
