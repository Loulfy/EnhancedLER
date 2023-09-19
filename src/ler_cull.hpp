//
// Created by loulfy on 06/08/2023.
//

#ifndef LER_CULL_HPP
#define LER_CULL_HPP

#include "ler_res.hpp"

namespace ler
{
    class InstanceCull
    {
    public:

        void init(const LerDevicePtr& device, const std::array<BufferPtr, 2>& buffers);
        void dispatch(const CommandPtr& cmd, const CameraParam& camera, uint32_t instanceCount, bool prePass);
        void createDepthPyramid(const LerDevicePtr& device, vk::Extent2D extent);
        void renderDepthPyramid(const TexturePtr& depth, const CommandPtr& cmd);
        void updateDescriptors(const TexturePtr& depth);

        [[nodiscard]] const BufferPtr& getCountBuffer() const { return m_visibleBuffer; }
        [[nodiscard]] const BufferPtr& getCommandBuffers() const { return m_commandBuffer; }
        uint32_t drawCount;

    private:

        Frustum m_frustum;
        PipelinePtr m_pipeline;
        BufferPtr m_frustumBuffer;
        BufferPtr m_visibleBuffer;
        BufferPtr m_commandBuffer;
        vk::DescriptorSet m_descriptor;

        u32 m_hzbSize = 2048u;
        u32 m_mipLevels = 11u;
        PipelinePtr m_pyramid;
        TexturePtr m_depthPyramid;
        std::array<vk::ImageView,16> m_views;
        vk::UniqueSampler m_reductionSampler;
        std::array<vk::DescriptorSet,16> m_slots;

        void beginBarrier(const CommandPtr& cmd);
        void endBarrier(const CommandPtr& cmd, uint32_t mipLevel);
        void addLayoutPyramid(const CommandPtr& cmd);
    };
}

#endif //LER_CULL_HPP
