//
// Created by loulfy on 25/07/2023.
//

#ifndef LER_CSM_HPP
#define LER_CSM_HPP

#include "ler_cam.hpp"
#include "ler_draw.hpp"

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

        void init(const LerDevicePtr& device, RenderSceneList& scene, GLFWwindow* window) override;
        void onSceneChange(const LerDevicePtr& device, SceneBuffers* scene) override;
        void render(const LerDevicePtr& device, FrameWindow& frame, RenderSceneList& sceneList, RenderParams& params) override;
        void update(const CameraParam& param, SceneParamDep& scene);

    public:

        std::array<Cascade, SHADOW_MAP_CASCADE_COUNT> m_cascades;
        vk::DescriptorSet m_descriptor = nullptr;
        PipelinePtr m_pipeline;
        TexturePtr m_shadowMap;
    };
}

#endif //LER_CSM_HPP
