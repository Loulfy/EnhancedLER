//
// Created by loulfy on 06/07/2023.
//

#ifndef LER_APP_H
#define LER_APP_H

#include "ler_dev.hpp"
#include "ler_scn.hpp"
#include "ler_sys.hpp"
#include "ler_svc.hpp"
#include "ler_spv.hpp"
#include "ler_gui.hpp"
#include "ler_img.hpp"
#include "ler_cam.hpp"
#include "ler_res.hpp"
#include "ler_csm.hpp"
#include "ler_psx.hpp"

#define GLFW_INCLUDE_NONE // Do not include any OpenGL/Vulkan headers
#include <GLFW/glfw3.h>

namespace ler
{
    class LerApp
    {
    public:

        friend class SceneImporter;
        explicit LerApp(LerConfig cfg = {});
        void run();

        // Non-copyable and non-movable
        LerApp(const LerApp&) = delete;
        LerApp(const LerApp&&) = delete;
        LerApp& operator=(const LerApp&) = delete;
        LerApp& operator=(const LerApp&&) = delete;

        template<typename T>
        void addPass() { m_renderPasses.emplace_back(std::make_shared<T>()); }
        void resize(int width, int height);

        RenderGraph& renderGraph() { return m_graph; }

        void bindController(const std::shared_ptr<Controller>& controller) { m_controller = controller; }
        void lockCursor() { glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); }
        [[nodiscard]] bool isCursorLock() const { return glfwGetInputMode(m_window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED; }

        void loadSceneAsync(const fs::path& path, FsTag tag = FsTag_Assets)
        {
            SceneImporter::LoadScene(m_device, m_renderer.getSceneBuffers(), tag, path);
        }

        void operator()(SubmitTexture& submit)
        {
            uint64_t submissionId = m_device->submitCommand(submit.command);
            m_device->getTexturePool()->set(submit.id, submit.texture, submissionId);
        }

        void operator()(SubmitScene& submit)
        {
            SceneImporter::SceneSubmissionPtr submission = submit.submission;
            uint64_t submissionId = m_device->submitCommand(submit.command);
            submission->submissionId = submissionId;
        }

    private:

        static void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void glfw_mouse_callback(GLFWwindow* window, int button, int action, int mods);
        static void glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
        static void glfw_size_callback(GLFWwindow* window, int width, int height);

        void updateSwapChain();
        void notifyResize();
        void updateWindowIcon(const fs::path& path);

        void autoExec();

        LerConfig m_config;
        GLFWwindow* m_window = nullptr;
        std::unique_ptr<VulkanInitializer> m_vulkan;
        std::unique_ptr<GlslangInitializer> m_glslang;
        std::unique_ptr<PXInitializer> m_physx;
        std::shared_ptr<LerDevice> m_device;
        vk::UniqueSurfaceKHR m_surface;
        SwapChain m_swapChain;
        std::vector<TexturePtr> m_images;
        std::array<FrameWindow, 3> m_targets;
        std::vector<std::shared_ptr<IRenderPass>> m_renderPasses;
        std::unique_ptr<rtxmu::VkAccelStructManager> m_rtxMemUtil;
        ControllerPtr m_controller;
        RenderSceneList m_renderer;
        RenderGraph m_graph;
        flecs::world m_world;
        flecs::entity m_selected = flecs::entity::null();
    };
}

#endif //LER_APP_H
