//
// Created by loulfy on 12/07/2023.
//

#ifndef LER_GUI_HPP
#define LER_GUI_HPP

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>
#include <ImGuizmo.h>

#include "ler_draw.hpp"

namespace ler
{
    class ImGuiPass : public IRenderPass
    {
    public:

        void init(const LerDevicePtr& device, RenderSceneList& scene, GLFWwindow* window) override;
        void prepare(const LerDevicePtr& device) override;
        void render(const LerDevicePtr& device, FrameWindow& frame, RenderSceneList& sceneList, RenderParams& params) override;
        void clean() override;

        static VkDescriptorSet createTextureDescriptorSet(const TexturePtr& texture, const vk::ImageSubresourceRange& sub, vk::Sampler sampler);

    private:

        static vk::UniqueDescriptorPool createPool(const LerDevicePtr& device);
        vk::UniqueDescriptorPool m_descriptorPool;
    };

    class TextureViewerPass : public IRenderPass
    {
    public:

        void init(const LerDevicePtr& device, RenderSceneList& scene, GLFWwindow* window) override;
        void render(const LerDevicePtr& device, FrameWindow& frame, RenderSceneList& sceneList, RenderParams& params) override;

    private:

        vk::UniqueSampler m_sampler;
        const char* current_item = nullptr;
        VkDescriptorSet ds = nullptr;
        std::map<std::string, VkDescriptorSet> cache;
        std::array<VkDescriptorSet,4> depth = {nullptr};
        VkDescriptorSet mip;
    };
}

#endif //LER_GUI_HPP
