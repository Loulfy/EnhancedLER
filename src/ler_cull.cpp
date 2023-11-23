//
// Created by loulfy on 06/08/2023.
//

#include "ler_cull.hpp"

namespace ler
{
    void InstanceCull::init(const LerDevicePtr& device, const std::array<BufferPtr, 2>& buffers)
    {
        using bu = vk::BufferUsageFlagBits;
        m_visibleBuffer = device->createBuffer(256, bu::eStorageBuffer | bu::eIndirectBuffer, true);
        m_frustumBuffer = device->createBuffer(256, bu::eUniformBuffer, true);
        m_commandBuffer = device->createBuffer(C16Mio, bu::eStorageBuffer | bu::eIndirectBuffer);

        m_reductionSampler = device->createSamplerMipMap(vk::SamplerAddressMode::eClampToEdge, true, f32(m_mipLevels), true);
        m_depthPyramid = device->createTexture(vk::Format::eR16Sfloat, vk::Extent2D(2048, 2048), vk::SampleCountFlagBits::e1, true, 1, m_mipLevels);

        ler::ShaderPtr shader = device->createShader("generate_draws.comp.spv");
        m_pipeline = device->createComputePipeline(shader);
        m_descriptor = m_pipeline->createDescriptorSet(0);

        device->updateStorage(m_descriptor, 0, m_frustumBuffer, 256, true);
        device->updateStorage(m_descriptor, 1, buffers[0], VK_WHOLE_SIZE); // Instance
        device->updateStorage(m_descriptor, 2, buffers[1], VK_WHOLE_SIZE); // Meshlet
        device->updateStorage(m_descriptor, 3, m_commandBuffer, VK_WHOLE_SIZE);
        device->updateStorage(m_descriptor, 4, m_visibleBuffer, 256);

        shader = device->createShader("downsample.comp.spv");
        m_pyramid = device->createComputePipeline(shader);

        for(auto & m_slot : m_slots)
            m_slot = m_pyramid->createDescriptorSet(0);
    }

    void InstanceCull::createDepthPyramid(const LerDevicePtr& device, vk::Extent2D extent)
    {
        u32 size = glm::max(extent.width, extent.height);
        m_hzbSize = glm::ceilPowerOfTwo(size);
        m_mipLevels = glm::log2(size) + 1u;

        m_reductionSampler = device->createSamplerMipMap(vk::SamplerAddressMode::eClampToEdge, true, f32(m_mipLevels), true);
        m_depthPyramid = device->createTexture(vk::Format::eR16Sfloat, vk::Extent2D(m_hzbSize, m_hzbSize), vk::SampleCountFlagBits::e1, true, 1, m_mipLevels);
        device->initTextureLayout(m_depthPyramid, vk::ImageLayout::eShaderReadOnlyOptimal, vk::AccessFlagBits::eShaderRead);
    }

    void InstanceCull::updateDescriptors(const TexturePtr& depth)
    {
        vk::Sampler sampler = m_reductionSampler.get();
        for(uint32_t mipIndex = 0; mipIndex < m_mipLevels; ++mipIndex)
        {
            m_views[mipIndex] = m_depthPyramid->view(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, mipIndex, 1, 0, 1));
            vk::ImageView imageDst = m_views[mipIndex];
            vk::ImageView imageSrc = mipIndex == 0 ? depth->view() : m_views[mipIndex - 1u];
            m_pyramid->updateSampler(m_slots[mipIndex], 1, sampler, vk::ImageLayout::eGeneral, imageDst);
            m_pyramid->updateSampler(m_slots[mipIndex], 0, sampler, vk::ImageLayout::eShaderReadOnlyOptimal, imageSrc);
        }

        vk::ImageView view = m_depthPyramid->view(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, m_mipLevels, 0, 1));
        m_pipeline->updateSampler(m_descriptor, 5,  m_reductionSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal, view);
    }

    void InstanceCull::dispatch(const CommandPtr& cmd, const CameraParam& camera, uint32_t instanceCount, bool prePass)
    {
        m_frustum.cull = prePass ? 0 : 42;
        m_frustum.num = instanceCount;
        m_frustum.camera = camera.test;
        m_visibleBuffer->getUint(&drawCount);
        LerDevice::getFrustumPlanes(camera.proj * camera.view, m_frustum.planes);
        LerDevice::getFrustumCorners(camera.proj * camera.view, m_frustum.corners);
        static constexpr uint32_t resetNum = 0;
        m_visibleBuffer->uploadFromMemory(&resetNum, sizeof(uint32_t));
        m_frustumBuffer->uploadFromMemory(&m_frustum, sizeof(Frustum));

        cmd->bindPipeline(m_pipeline, m_descriptor);
        cmd->cmdBuf.pushConstants(m_pipeline->pipelineLayout.get(), vk::ShaderStageFlagBits::eCompute, 0, 128, &camera);
        cmd->cmdBuf.dispatch(1 + m_frustum.num / 64, 1, 1);

        using ps = vk::PipelineStageFlagBits;
        std::vector<vk::BufferMemoryBarrier> bufferBarriers;
        bufferBarriers.emplace_back(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eMemoryRead, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_commandBuffer->handle, 0, VK_WHOLE_SIZE);
        bufferBarriers.emplace_back(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eMemoryRead, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, m_visibleBuffer->handle, 0, VK_WHOLE_SIZE);
        cmd->cmdBuf.pipelineBarrier(ps::eComputeShader, ps::eDrawIndirect, vk::DependencyFlags(), {}, bufferBarriers, {});
    }

    u32 divideRoundingUp(u32 dividend, u32 divisor)
    {
        return (dividend + divisor - 1) / divisor;
    }

    void InstanceCull::renderDepthPyramid(const TexturePtr& depth, const CommandPtr& cmd)
    {
        updateDescriptors(depth);

        using ps = vk::PipelineStageFlagBits;
        std::vector<vk::ImageMemoryBarrier> imageBarriers;
        auto& barrier = imageBarriers.emplace_back();
        barrier.setImage(depth->handle);
        barrier.setOldLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
        barrier.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        barrier.setSrcAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
        barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
        barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1));
        cmd->cmdBuf.pipelineBarrier(ps::eEarlyFragmentTests, ps::eComputeShader, vk::DependencyFlags(), {}, {}, imageBarriers);

        beginBarrier(cmd);
        for (u32 mipIndex = 0; mipIndex < m_mipLevels; ++mipIndex)
        {
            u32 hzbMipSize = m_hzbSize >> mipIndex;
            cmd->bindPipeline(m_pyramid, m_slots[mipIndex]);
            glm::uvec2 groupCount(divideRoundingUp(hzbMipSize, 32u));
            cmd->cmdBuf.pushConstants(m_pyramid->pipelineLayout.get(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(u32), &hzbMipSize);
            cmd->cmdBuf.dispatch(groupCount.x, groupCount.y, 1);

            endBarrier(cmd, mipIndex);
        }

        imageBarriers.clear();
        barrier = imageBarriers.emplace_back();
        barrier.setImage(depth->handle);
        barrier.setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        barrier.setNewLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
        barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
        barrier.setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
        barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1));
        cmd->cmdBuf.pipelineBarrier(ps::eComputeShader, ps::eEarlyFragmentTests, vk::DependencyFlags(), {}, {}, imageBarriers);

    }

    void InstanceCull::beginBarrier(const CommandPtr& cmd)
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
        barrier.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, m_mipLevels, 0, 1));
        cmd->cmdBuf.pipelineBarrier(ps::eComputeShader, ps::eComputeShader, vk::DependencyFlags(), {}, {}, imageBarriers);
    }


    void InstanceCull::endBarrier(const CommandPtr& cmd, uint32_t mipLevel)
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

    void InstanceCull::addLayoutPyramid(const CommandPtr& cmd)
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
        barrier.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, m_mipLevels, 0, 1));
        cmd->cmdBuf.pipelineBarrier(ps::eAllCommands, ps::eAllCommands, vk::DependencyFlags(), {}, {}, imageBarriers);
    }
}