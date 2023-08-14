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

        void init(LerDevicePtr& device);
        void prepare(LerDevicePtr& device, BatchedMesh& batch);
        void dispatch(LerDevicePtr& device, const CameraParam& camera, BatchedMesh& batch, bool prePass);

        [[nodiscard]] BufferPtr getCountBuffer() const { return m_visibleBuffer; }
        uint32_t drawCount;

    private:

        Frustum m_frustum;
        PipelinePtr m_pipeline;
        BufferPtr m_frustumBuffer;
        BufferPtr m_visibleBuffer;
        TexturePtr m_depthPyramid;
        std::array<vk::DescriptorSet,2> m_descriptor;

        PipelinePtr m_pyramid;
        vk::UniqueSampler m_reductionSampler;

        std::array<vk::ImageView,13> m_views;

        void generateDepthPyramid(LerDevicePtr& device);

        void addLayoutPyramid(CommandPtr& cmd);
        void endBarrier(CommandPtr& cmd, uint32_t mipLevel);
        void beginBarrier(CommandPtr& cmd);

        static void startDepthBarrier(CommandPtr& cmd, const TexturePtr& depth);
        static void endDepthBarrier(CommandPtr& cmd, const TexturePtr& depth);
    };
}

#endif //LER_CULL_HPP
