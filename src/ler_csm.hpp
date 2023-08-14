//
// Created by loulfy on 25/07/2023.
//

#ifndef LER_CSM_HPP
#define LER_CSM_HPP

#include "ler_cam.hpp"
#include "ler_dev.hpp"
#include "ler_res.hpp"

namespace ler
{
    class CascadedShadowPass : public IRenderPass
    {
    public:

        static constexpr int SHADOW_MAP_CASCADE_COUNT = 4;
        static constexpr float cascadeSplitLambda = 0.95f;
        struct Cascade
        {
            float splitDepth;
            glm::mat4 viewProjMatrix;
        };

        void init(LerDevicePtr& device, GLFWwindow* window) override;
        void onSceneChange(LerDevicePtr& device, BatchedMesh& batch) override;
        void render(LerDevicePtr& device, FrameWindow& frame, BatchedMesh& batch, const CameraParam& param, SceneParam& scene) override;
        void update(const CameraParam& param, SceneParam& scene);

    public:

        std::array<Cascade, SHADOW_MAP_CASCADE_COUNT> m_cascades;
        vk::DescriptorSet m_descriptor = nullptr;
        PipelinePtr m_pipeline;
        TexturePtr m_shadowMap;
    };
}

#endif //LER_CSM_HPP
