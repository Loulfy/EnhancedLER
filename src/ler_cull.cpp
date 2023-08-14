//
// Created by loulfy on 06/08/2023.
//

#include "ler_cull.hpp"

namespace ler
{
    void InstanceCull::init(LerDevicePtr& device)
    {
        ler::ShaderPtr shader = device->createShader("generate_draws.comp.spv");
        m_pipeline = device->createComputePipeline(shader);
        m_descriptor[0] = m_pipeline->createDescriptorSet(0);

        m_visibleBuffer = device->createBuffer(256, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndirectBuffer, true);
        m_frustumBuffer = device->createBuffer(256, vk::BufferUsageFlagBits::eUniformBuffer, true);

        float lod = floor(std::log2f(2048))+1;
        m_depthPyramid = device->createTexture(vk::Format::eR16Sfloat, vk::Extent2D(2048, 2048), vk::SampleCountFlagBits::e1, true, 1, 12);
        device->setRenderTarget(RT::eDepthPyramid, m_depthPyramid);

        vk::DebugMarkerObjectNameInfoEXT nameInfo;
        nameInfo.setPObjectName("Depth Pyramid");
        nameInfo.setObjectType(vk::DebugReportObjectTypeEXT::eImage);
        nameInfo.setObject((uint64_t)static_cast<VkImage>(m_depthPyramid->handle));
        //evice->getVulkanContext().device.debugMarkerSetObjectNameEXT(nameInfo);

        shader = device->createShader("downsample.comp.spv");
        m_pyramid = device->createComputePipeline(shader);
        m_reductionSampler = device->createSamplerMipMap(vk::SamplerAddressMode::eClampToEdge, true, 12.f, true);
        m_descriptor[1] = m_pyramid->createDescriptorSet(0);

        vk::Sampler sampler = m_reductionSampler.get();
        TexturePtr depth = device->getRenderTarget(RT::eDepth);
        /*device->updateSampler(m_descriptor[1], 0, sampler, depth, vk::DescriptorType::eCombinedImageSampler);
        device->updateSampler(m_descriptor[1], 1, sampler, m_depthPyramid, vk::DescriptorType::eStorageImage);*/

        m_views[12] = depth->view();
        CommandPtr cmd = device->createCommand();
        for(uint32_t i = 0; i < 12; ++i)
        {
            m_views[i] = m_depthPyramid->view(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, i, 1, 0, 1));
        }
        addLayoutPyramid(cmd);
        device->submitAndWait(cmd);
    }

    void InstanceCull::prepare(LerDevicePtr& device, BatchedMesh& batch)
    {
        device->updateStorage(m_descriptor[0], 0, m_frustumBuffer, 256, true);
        device->updateStorage(m_descriptor[0], 1, batch.instanceBuffer, VK_WHOLE_SIZE);
        device->updateStorage(m_descriptor[0], 2, batch.meshletBuffer, VK_WHOLE_SIZE);
        device->updateStorage(m_descriptor[0], 3, batch.commandBuffer, VK_WHOLE_SIZE);
        device->updateStorage(m_descriptor[0], 4, m_visibleBuffer, 256);

        vk::ImageView view = m_depthPyramid->view(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 12, 0, 1));
        m_pipeline->updateSampler(m_descriptor[0], 5,  m_reductionSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal, view);
    }

    u32 divideRoundingUp(u32 dividend, u32 divisor)
    {
        return (dividend + divisor - 1) / divisor;
    }

    u32 roundUpToPowerOfTwo(f32 value)
    {
        f32 valueBase = glm::log2(value);
        if (glm::fract(valueBase) == 0.0f)
        {
            return value;
        }
        return glm::pow(2.0f, glm::trunc(valueBase + 1.0f));
    }

    void InstanceCull::dispatch(LerDevicePtr& device, const CameraParam& camera, BatchedMesh& batch, bool prePass)
    {
        if(!prePass)
            generateDepthPyramid(device);

        m_frustum.cull = prePass ? 0 : 42;
        if(!prePass)
            m_visibleBuffer->getUint(&drawCount);
        m_frustum.num = batch.renderList.instances().size();
        LerDevice::getFrustumPlanes(camera.proj * camera.view, m_frustum.planes);
        LerDevice::getFrustumCorners(camera.proj * camera.view, m_frustum.corners);
        static constexpr uint32_t resetNum = 0;
        m_visibleBuffer->uploadFromMemory(&resetNum, sizeof(uint32_t));
        m_frustumBuffer->uploadFromMemory(&m_frustum, sizeof(Frustum));

        CommandPtr cmd = device->createCommand();
        cmd->bindPipeline(m_pipeline, m_descriptor[0]);
        cmd->cmdBuf.pushConstants(m_pipeline->pipelineLayout.get(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(CameraParam), &camera);
        cmd->cmdBuf.dispatch(1 + m_frustum.num / 64, 1, 1);

        using ps = vk::PipelineStageFlagBits;
        std::vector<vk::BufferMemoryBarrier> bufferBarriers;
        bufferBarriers.emplace_back(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eMemoryRead, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, batch.commandBuffer->handle, 0, VK_WHOLE_SIZE);
        bufferBarriers.emplace_back(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eMemoryRead, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_visibleBuffer->handle, 0, VK_WHOLE_SIZE);
        cmd->cmdBuf.pipelineBarrier(ps::eComputeShader, ps::eDrawIndirect, vk::DependencyFlags(), {}, bufferBarriers, {});
        device->submitAndWait(cmd);
    }

    void InstanceCull::generateDepthPyramid(LerDevicePtr& device)
    {
        CommandPtr cmd = device->createCommand();
        //startDepthBarrier(cmd, device->getRenderTarget(RT::eDepth));
        beginBarrier(cmd);
        device->submitAndWait(cmd);

        //cmd = device->createCommand();
        //startDepthBarrier(cmd, device->getRenderTarget(RT::eDepth));

        vk::Sampler sampler = m_reductionSampler.get();
        static u32 hzbSize = roundUpToPowerOfTwo(2048.f);
        static constexpr int32_t depthPyramidLevels = 12;
        for (u32 mipIndex = 0; mipIndex < 12; ++mipIndex)
        {
            cmd = device->createCommand();
            vk::ImageView imageSrc = mipIndex == 0 ? m_views[12] : m_views[mipIndex - 1u];
            vk::ImageView imageDst = m_views[mipIndex];
            //device->updateSampler(m_descriptor[1], 0, sampler, imageSrc, layoutSrc, vk::DescriptorType::eCombinedImageSampler);
            //device->updateSampler(m_descriptor[1], 1, sampler, imageDst, vk::ImageLayout::eGeneral, vk::DescriptorType::eStorageImage);

            m_pyramid->updateSampler(m_descriptor[1], 0, sampler, vk::ImageLayout::eShaderReadOnlyOptimal, imageSrc);
            m_pyramid->updateSampler(m_descriptor[1], 1, sampler, vk::ImageLayout::eGeneral, imageDst);

            //beginBarrier(cmd, mipIndex);
            u32 hzbMipSize = hzbSize >> mipIndex;
            cmd->bindPipeline(m_pyramid, m_descriptor[1]);
            glm::uvec2 groupCount(divideRoundingUp(hzbMipSize, 32u));
            cmd->cmdBuf.pushConstants(m_pyramid->pipelineLayout.get(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(u32), &hzbMipSize);
            cmd->cmdBuf.dispatch(groupCount.x, groupCount.y, 1);

            endBarrier(cmd, mipIndex);
            device->submitAndWait(cmd);
        }

        //endDepthBarrier(cmd, device->getRenderTarget(RT::eDepth));
        //device->submitAndWait(cmd);
    }

    void InstanceCull::addLayoutPyramid(CommandPtr& cmd)
    {
        using ps = vk::PipelineStageFlagBits;
        std::vector<vk::ImageMemoryBarrier> imageBarriers;
        auto& barrier = imageBarriers.emplace_back();
        barrier.setImage(m_depthPyramid->handle);
        barrier.setOldLayout(vk::ImageLayout::eUndefined);
        barrier.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        barrier.setSrcAccessMask(vk::AccessFlagBits::eNone);
        barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
        barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 12, 0, 1));
        cmd->cmdBuf.pipelineBarrier(ps::eAllCommands, ps::eAllCommands, vk::DependencyFlags(), {}, {}, imageBarriers);
    }

    void InstanceCull::startDepthBarrier(CommandPtr& cmd, const TexturePtr& depth)
    {
        using ps = vk::PipelineStageFlagBits;
        std::vector<vk::ImageMemoryBarrier> imageBarriers;
        auto& barrier = imageBarriers.emplace_back();
        barrier.setImage(depth->handle);
        barrier.setOldLayout(vk::ImageLayout::eDepthAttachmentOptimal);
        //barrier.setOldLayout(vk::ImageLayout::eUndefined);
        barrier.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        barrier.setSrcAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
        //barrier.setSrcAccessMask(vk::AccessFlags());
        barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
        barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1));
        cmd->cmdBuf.pipelineBarrier(ps::eEarlyFragmentTests, ps::eComputeShader, vk::DependencyFlags(), {}, {}, imageBarriers);
    }

    void InstanceCull::endDepthBarrier(CommandPtr& cmd, const TexturePtr& depth)
    {
        using ps = vk::PipelineStageFlagBits;
        std::vector<vk::ImageMemoryBarrier> imageBarriers;
        auto& barrier = imageBarriers.emplace_back();
        barrier.setImage(depth->handle);
        barrier.setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        barrier.setNewLayout(vk::ImageLayout::eDepthAttachmentOptimal);
        barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
        barrier.setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
        barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1));
        cmd->cmdBuf.pipelineBarrier(ps::eComputeShader, ps::eEarlyFragmentTests, vk::DependencyFlags(), {}, {}, imageBarriers);
    }

    void InstanceCull::beginBarrier(CommandPtr& cmd)
    {
        using ps = vk::PipelineStageFlagBits;
        std::vector<vk::ImageMemoryBarrier> imageBarriers;
        auto& barrier = imageBarriers.emplace_back();
        barrier.setImage(m_depthPyramid->handle);
        barrier.setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        barrier.setNewLayout(vk::ImageLayout::eGeneral);
        barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
        barrier.setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
        barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 12, 0, 1));
        cmd->cmdBuf.pipelineBarrier(ps::eComputeShader, ps::eComputeShader, vk::DependencyFlags(), {}, {}, imageBarriers);
    }

    void InstanceCull::endBarrier(CommandPtr& cmd, uint32_t mipLevel)
    {
        using ps = vk::PipelineStageFlagBits;
        std::vector<vk::ImageMemoryBarrier> imageBarriers;
        auto& barrier = imageBarriers.emplace_back();
        barrier.setImage(m_depthPyramid->handle);
        barrier.setOldLayout(vk::ImageLayout::eGeneral);
        barrier.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
        barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
        barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, mipLevel, 1, 0, 1));
        cmd->cmdBuf.pipelineBarrier(ps::eComputeShader, ps::eComputeShader, vk::DependencyFlags(), {}, {}, imageBarriers);
    }
}