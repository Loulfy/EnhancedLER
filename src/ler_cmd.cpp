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

    void TrackedCommandBuffer::addImageBarrier(const TexturePtr& texture, ResourceState new_state) const
    {
        ResourceState old_state = texture->state;
        vk::ImageMemoryBarrier2KHR barrier;
        barrier.srcAccessMask = util_to_vk_access_flags( old_state );
        barrier.srcStageMask = util_determine_pipeline_stage_flags2( barrier.srcAccessMask, queueKind);
        barrier.dstAccessMask = util_to_vk_access_flags( new_state );
        barrier.dstStageMask = util_determine_pipeline_stage_flags2( barrier.dstAccessMask, queueKind);
        barrier.oldLayout = util_to_vk_image_layout( old_state );
        barrier.newLayout = util_to_vk_image_layout( new_state );
        barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setImage(texture->handle);
        vk::ImageAspectFlags aspect = LerDevice::guessImageAspectFlags(texture->info.format, false);
        barrier.setSubresourceRange(vk::ImageSubresourceRange(aspect, 0, VK_REMAINING_MIP_LEVELS, 0, texture->info.arrayLayers));

        texture->state = new_state;
        vk::DependencyInfoKHR dependency_info;
        dependency_info.imageMemoryBarrierCount = 1;
        dependency_info.pImageMemoryBarriers = &barrier;

        /*log::debug("[ImageBarrier: {}] srcStage: {}, dstStage {}, oldLayout: {}, newLayout: {}", texture->name,
           vk::to_string(barrier.srcStageMask), vk::to_string(barrier.dstStageMask),
           vk::to_string(barrier.oldLayout), vk::to_string(barrier.newLayout));*/
        cmdBuf.pipelineBarrier2(dependency_info);
    }

    void TrackedCommandBuffer::addBufferBarrier(const ler::BufferPtr& buffer, ler::ResourceState new_state) const
    {
        ResourceState old_state = buffer->state;
        vk::BufferMemoryBarrier2KHR barrier;
        barrier.srcAccessMask = util_to_vk_access_flags(old_state);
        barrier.srcStageMask = util_determine_pipeline_stage_flags2(barrier.srcAccessMask, queueKind);
        barrier.dstAccessMask = util_to_vk_access_flags(new_state);
        barrier.dstStageMask = util_determine_pipeline_stage_flags2(barrier.dstAccessMask, queueKind);
        barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        barrier.buffer = buffer->handle;
        barrier.offset = 0;
        barrier.size = VK_WHOLE_SIZE;

        vk::DependencyInfoKHR dependency_info;
        dependency_info.bufferMemoryBarrierCount = 1;
        dependency_info.pBufferMemoryBarriers = &barrier;

        cmdBuf.pipelineBarrier2(dependency_info);
        /*log::debug("[BufferBarrier] srcStage: {}, dstStage {}, srcMask: {}, dstMask: {}",
                   vk::to_string(barrier.srcStageMask), vk::to_string(barrier.dstStageMask),
                   vk::to_string(barrier.srcAccessMask), vk::to_string(barrier.dstAccessMask));*/
    }

    void TrackedCommandBuffer::addBarrier(const std::shared_ptr<IResource>& resource, ResourceState new_state) const
    {
        if(dynamic_cast<Texture*>(resource.get()))
            addImageBarrier(std::static_pointer_cast<Texture>(resource), new_state);
        else if(dynamic_cast<Buffer*>(resource.get()))
            addBufferBarrier(std::static_pointer_cast<Buffer>(resource), new_state);
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
        addImageBarrier(texture, CopyDest);
        // Copy buffer to texture
        vk::BufferImageCopy copyRegion(0, 0, 0);
        copyRegion.imageExtent = texture->info.extent;
        copyRegion.imageSubresource = vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1);
        cmdBuf.copyBufferToImage(buffer->handle, texture->handle, vk::ImageLayout::eTransferDstOptimal, 1, &copyRegion);
        // prepare texture to color layout
        addImageBarrier(texture, ShaderResource);
    }

    void TrackedCommandBuffer::bindPipeline(const PipelinePtr& pipeline, const vk::DescriptorSet set) const
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
            attachment.setClearValue(rColorAttachment.clear.color);
        }

        if (desc.depthStencilAttachment.texture)
        {
            depthAttachment.setImageView(desc.depthStencilAttachment.texture->view());
            depthAttachment.setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal);
            depthAttachment.setLoadOp(desc.depthStencilAttachment.loadOp);
            depthAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
            if(depthAttachment.loadOp == vk::AttachmentLoadOp::eClear)
                depthAttachment.setClearValue(vk::ClearDepthStencilValue(1.0f, 0));
            depthAttachment.setClearValue(desc.depthStencilAttachment.clear.depthStencil);
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

    void TrackedCommandBuffer::beginRenderPass(const RenderPass& pass)
    {
        vk::RenderingInfo renderInfo;
        vk::Viewport viewport = LerDevice::createViewport(pass.viewport);
        vk::Rect2D renderArea(vk::Offset2D(0,0), pass.viewport);
        renderInfo.setRenderArea(renderArea);
        renderInfo.setLayerCount(1);

        renderInfo.setPColorAttachments(pass.colors.data());
        renderInfo.setColorAttachmentCount(pass.colorCount);

        if (pass.depth.imageView)
            renderInfo.setPDepthAttachment(&pass.depth);

        cmdBuf.beginRendering(renderInfo);
        cmdBuf.setScissor(0, 1, &renderArea);
        cmdBuf.setViewport(0, 1, &viewport);
        m_beginRendering = true;
    }

    void TrackedCommandBuffer::draw(uint32_t vertexCount) const
    {
        cmdBuf.draw(vertexCount, 1, 0, 0);
    }

    void TrackedCommandBuffer::end() const
    {
        if (m_beginRendering)
            cmdBuf.endRendering();
    }
}