//
// Created by loulfy on 15/07/2023.
//

#include "ler_dev.hpp"

namespace ler
{
    TrackedCommandBuffer::~TrackedCommandBuffer()
    {
        m_context.device.destroyCommandPool(cmdPool);
    }

    void TrackedCommandBuffer::addBarrier(TexturePtr& texture, vk::PipelineStageFlagBits srcStage,
                                          vk::PipelineStageFlagBits dstStage)
    {
        vk::ImageLayout oldLayout = vk::ImageLayout::eUndefined;
        vk::ImageLayout newLayout = vk::ImageLayout::eGeneral;
        vk::AccessFlagBits srcAccess = vk::AccessFlagBits::eNone;
        vk::AccessFlagBits dstAccess = vk::AccessFlagBits::eNone;
        if (dstStage == vk::PipelineStageFlagBits::eBottomOfPipe)
        {
            oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
            newLayout = vk::ImageLayout::ePresentSrcKHR;
            srcAccess = vk::AccessFlagBits::eColorAttachmentWrite;
            dstAccess = vk::AccessFlagBits::eNone;
        }
        if (dstStage == vk::PipelineStageFlagBits::eColorAttachmentOutput)
        {
            oldLayout = vk::ImageLayout::ePresentSrcKHR;
            newLayout = vk::ImageLayout::eColorAttachmentOptimal;
            dstAccess = vk::AccessFlagBits::eColorAttachmentWrite;
            srcAccess = vk::AccessFlagBits::eNone;
        }
        if(dstStage == vk::PipelineStageFlagBits::eFragmentShader)
        {
            newLayout = vk::ImageLayout::eDepthReadOnlyOptimal;
            oldLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
            srcAccess = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            dstAccess = vk::AccessFlagBits::eShaderRead;
        }
        if(dstStage == vk::PipelineStageFlagBits::eEarlyFragmentTests)
        {
            //oldLayout = vk::ImageLayout::eDepthReadOnlyOptimal;
            newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
            srcAccess = vk::AccessFlagBits::eShaderRead;
            dstAccess = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        }

        std::vector<vk::ImageMemoryBarrier> imageBarriers;
        auto imageAspect = LerDevice::guessImageAspectFlags(texture->info.format);
        auto& barrier = imageBarriers.emplace_back();
        barrier.setImage(texture->handle);
        barrier.setOldLayout(oldLayout);
        barrier.setNewLayout(newLayout);
        barrier.setSrcAccessMask(srcAccess);
        barrier.setDstAccessMask(dstAccess);
        barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setSubresourceRange(vk::ImageSubresourceRange(imageAspect, 0, 1, 0, texture->info.arrayLayers));
        cmdBuf.pipelineBarrier(srcStage, dstStage, vk::DependencyFlags(), {}, {}, imageBarriers);
    }

    void TrackedCommandBuffer::copyBuffer(BufferPtr& src, BufferPtr& dst, uint64_t byteSize, uint64_t dstOffset)
    {
        // Protect resource
        referencedResources.emplace_back(src);
        referencedResources.emplace_back(dst);

        vk::BufferCopy copyRegion(0, dstOffset, byteSize);
        cmdBuf.copyBuffer(src->handle, dst->handle, copyRegion);
    }

    void TrackedCommandBuffer::copyBuffer(BufferPtr& src, BufferPtr& dst, vk::BufferCopy copy)
    {
        // Protect resource
        referencedResources.emplace_back(src);
        referencedResources.emplace_back(dst);

        cmdBuf.copyBuffer(src->handle, dst->handle, copy);
    }

    void TrackedCommandBuffer::copyBufferToTexture(const BufferPtr& buffer, const TexturePtr& texture)
    {
        // Protect resource
        referencedResources.emplace_back(buffer);
        referencedResources.emplace_back(texture);

        // prepare texture to transfer layout!
        std::vector<vk::ImageMemoryBarrier> imageBarriersStart;
        vk::PipelineStageFlags beforeStageFlags = vk::PipelineStageFlagBits::eTopOfPipe;
        vk::PipelineStageFlags afterStageFlags = vk::PipelineStageFlagBits::eTransfer;

        imageBarriersStart.emplace_back(
                vk::AccessFlags(),
                vk::AccessFlagBits::eTransferWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eTransferDstOptimal,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                texture->handle,
                vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
        );

        cmdBuf.pipelineBarrier(beforeStageFlags, afterStageFlags, vk::DependencyFlags(), {}, {}, imageBarriersStart);

        // Copy buffer to texture
        vk::BufferImageCopy copyRegion(0, 0, 0);
        copyRegion.imageExtent = texture->info.extent;
        copyRegion.imageSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
        cmdBuf.copyBufferToImage(buffer->handle, texture->handle, vk::ImageLayout::eTransferDstOptimal, 1, &copyRegion);
        // prepare texture to color layout
        std::vector<vk::ImageMemoryBarrier> imageBarriersStop;
        beforeStageFlags = vk::PipelineStageFlagBits::eTransfer;
        afterStageFlags = vk::PipelineStageFlagBits::eAllCommands; //eFragmentShader
        imageBarriersStop.emplace_back(
                vk::AccessFlagBits::eTransferWrite,
                vk::AccessFlagBits::eShaderRead,
                vk::ImageLayout::eTransferDstOptimal,
                vk::ImageLayout::eShaderReadOnlyOptimal,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                texture->handle,
                vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
        );

        cmdBuf.pipelineBarrier(beforeStageFlags, afterStageFlags, vk::DependencyFlags(), {}, {}, imageBarriersStop);
    }

    void TrackedCommandBuffer::bindPipeline(const PipelinePtr& pipeline, const vk::DescriptorSet set)
    {
        cmdBuf.bindPipeline(pipeline->bindPoint, pipeline->handle.get());
        if (set)
        {
            cmdBuf.bindDescriptorSets(pipeline->bindPoint, pipeline->pipelineLayout.get(), 0, set, nullptr);
        }
    }

    void TrackedCommandBuffer::executePass(const PassDesc& desc)
    {
        vk::RenderingInfo renderInfo;
        vk::RenderingAttachmentInfo depthAttachment;
        std::vector<vk::RenderingAttachmentInfo> colorAttachments;

        for (const Attachment& rColorAttachment : desc.colorAttachments)
        {
            assert(rColorAttachment.texture);
            auto& attachment = colorAttachments.emplace_back();
            attachment.setImageView(rColorAttachment.texture->view());
            attachment.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
            attachment.setLoadOp(rColorAttachment.loadOp);
            attachment.setStoreOp(vk::AttachmentStoreOp::eStore);
            if(attachment.loadOp == vk::AttachmentLoadOp::eClear)
                attachment.setClearValue(vk::ClearColorValue(Color::White));
            //attachment.setClearValue(rColorAttachment.clear.color);
        }

        if (desc.depthStencilAttachment.texture)
        {
            depthAttachment.setImageView(desc.depthStencilAttachment.texture->view());
            depthAttachment.setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal);
            depthAttachment.setLoadOp(desc.depthStencilAttachment.loadOp);
            depthAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
            if(depthAttachment.loadOp == vk::AttachmentLoadOp::eClear)
                depthAttachment.setClearValue(vk::ClearDepthStencilValue(1.0f, 0));
            //depthAttachment.setClearValue(desc.depthStencilAttachment.clear.depthStencil);
            renderInfo.setPDepthAttachment(&depthAttachment);
        }

        vk::Viewport viewport = LerDevice::createViewport(desc.viewport);
        vk::Rect2D renderArea(vk::Offset2D(0,0), desc.viewport);
        renderInfo.setRenderArea(renderArea);
        renderInfo.setLayerCount(1);
        renderInfo.setColorAttachments(colorAttachments);
        cmdBuf.beginRendering(renderInfo);
        cmdBuf.setScissor(0, 1, &renderArea);
        cmdBuf.setViewport(0, 1, &viewport);
        m_beginRendering = true;
    }

    void TrackedCommandBuffer::draw(uint32_t vertexCount)
    {
        cmdBuf.draw(vertexCount, 1, 0, 0);
    }

    void TrackedCommandBuffer::end()
    {
        if (m_beginRendering)
            cmdBuf.endRendering();
    }
}