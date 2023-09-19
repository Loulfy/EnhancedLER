//
// Created by loulfy on 06/07/2023.
//

#include "ler_app.hpp"

#include <INIReader.h>

namespace ler
{
    static void iterate_tree(flecs::entity root, const std::function<void(flecs::entity)> runner)
    {
        runner(root);
        root.children([&runner](flecs::entity child){
            iterate_tree(child, runner);
        });
    }

    static void markDirty(flecs::entity p)
    {
        iterate_tree(p, [](flecs::entity e){ e.add<dirty>(); });
    }

    LerApp::LerApp(ler::LerConfig cfg) : m_config(cfg)
    {
        log::setup(log::level::debug);
        log::info("LER Init");
        log::info("CPU: {}, {} Threads", getCpuName(), std::thread::hardware_concurrency());
        log::info("RAM: {} Go", getRamCapacity()/1000);

        // Pre-Load!
        autoExec();

        if (!glfwInit())
            log::exit("Failed to init glfw");

        // Init filesystem
        FileSystemService::Get().mount(FsTag_Cache, StdFileSystem::Create(CACHED_DIR));
        FileSystemService::Get().mount(FsTag_Assets, StdFileSystem::Create(ASSETS_DIR));
        FileSystemService::Get().mount(FsTag_Default, StdFileSystem::Create(""));

        // Init window
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, cfg.resizable);
        m_window = glfwCreateWindow(static_cast<int>(cfg.width), static_cast<int>(cfg.height), "LER", nullptr, nullptr);
        glfwSetWindowUserPointer(m_window, this);
        glfwSetKeyCallback(m_window, glfw_key_callback);
        glfwSetScrollCallback(m_window, glfw_scroll_callback);
        glfwSetMouseButtonCallback(m_window, glfw_mouse_callback);
        glfwSetWindowSizeCallback(m_window, glfw_size_callback);

        // Because it's shiny
        updateWindowIcon(ASSETS_DIR / "wrench.png");

        uint32_t count;
        const char** extensions = glfwGetRequiredInstanceExtensions(&count);
        std::vector<const char*> instanceExtensions(extensions, extensions + count);
        std::copy(extensions, extensions + count, std::back_inserter(cfg.extensions));

        // Init vulkan
        m_vulkan = std::make_unique<VulkanInitializer>(cfg);
        const VulkanContext& context = m_vulkan->getVulkanContext();

        // Init Glslang
        m_glslang = std::make_unique<GlslangInitializer>();

        // Init PhysX
        m_physx = std::make_unique<PXInitializer>();

        // Init device
        m_device = std::make_shared<LerDevice>(context);

        // Init Service
        Service::Init(m_device);

        // Init surface
        VkSurfaceKHR glfwSurface;
        auto res = glfwCreateWindowSurface(context.instance, m_window, nullptr, &glfwSurface);
        if (res != VK_SUCCESS)
            log::exit("Failed to create window surface");

        m_surface = vk::UniqueSurfaceKHR(vk::SurfaceKHR(glfwSurface), { context.instance });

        updateSwapChain();

        GlslangInitializer::shaderAutoCompile();

        bindController(std::make_shared<FpsCamera>());
        m_controller->updateMatrices();

        m_device->getTexturePool()->fetch("white.png", ler::FsTag_Assets);

        m_renderer.allocate(m_device);
        m_renderer.install(m_world, m_device);

        // HERE

        /*
        m_world.system<CMesh, CTransform>().each([&](flecs::entity e, CMesh& m, CTransform& t){
            auto info = m_scene.meshes[m.meshIndex];
            m_scene.renderList.addPerDraw(m, t, info.get());
        });*/

        m_world.observer<CPhysic>().event(flecs::OnSet).each([&](flecs::entity e, CPhysic& p) {
            m_physx->getScene()->addActor(*p.actor);
        });

        m_world.observer<CPhysic>().event(flecs::OnRemove).each([&](flecs::entity e, CPhysic& p) {
            m_physx->getScene()->removeActor(*p.actor);
        });

        m_world.observer<CTransform>().event(flecs::OnSet).each([&](flecs::entity e, CTransform& t) {
            auto* glmData = const_cast<float*>(glm::value_ptr(t.model));
            if(e.has<CPhysic>())
                e.get<CPhysic>()->actor->setGlobalPose(convertGlmToPx(t.model));
        });
    }

    void LerApp::autoExec()
    {
        fs::path path(getHomeDir());
        path/= "ler/config.ini";
        INIReader reader(path.string());

        if (reader.ParseError() < 0)
            return;

        m_config.width = reader.GetInteger("window", "width", 1280);
        m_config.height = reader.GetInteger("window", "height", 720);
        m_config.resizable = reader.GetBoolean("window", "resizable", true);

        m_config.vsync = reader.GetBoolean("engine", "vsync", true);
        m_config.msaa = reader.GetInteger("engine", "msaa", 1);

        m_config.debug = reader.GetBoolean("debug", "enable", true);
    }

    void LerApp::updateWindowIcon(const fs::path& path)
    {
        GLFWimage icon;
        ImagePtr img = ImageLoader::load(path);
        icon.width = static_cast<int>(img->extent().width);
        icon.height = static_cast<int>(img->extent().width);
        icon.pixels = img->data();
        glfwSetWindowIcon(m_window, 1, &icon);
    }

    void LerApp::updateSwapChain()
    {
        const VulkanContext& context = m_vulkan->getVulkanContext();
        context.device.waitIdle();
        m_swapChain = SwapChain::create(context, m_surface.get(), m_config.width, m_config.height, m_config.vsync);

        m_images.clear();
        auto swapChainImages = context.device.getSwapchainImagesKHR(m_swapChain.handle.get());
        for(auto& image : swapChainImages)
            m_images.emplace_back(m_device->createTextureFromNative(image, m_swapChain.format, m_swapChain.extent));

        for(size_t i = 0; i < m_images.size(); ++i)
        {
            m_targets[i].image = m_images[i];
            m_targets[i].extent = m_swapChain.extent;
            m_targets[i].format = m_swapChain.format;
        }

        CommandPtr cmd = m_device->createCommand();
        for(auto& image : m_images)
        {
            using ps = vk::PipelineStageFlagBits;
            std::vector<vk::ImageMemoryBarrier> imageBarriers;
            auto& barrier = imageBarriers.emplace_back();
            barrier.setImage(image->handle);
            barrier.setOldLayout(vk::ImageLayout::eUndefined);
            barrier.setNewLayout(vk::ImageLayout::ePresentSrcKHR);
            barrier.setSrcAccessMask(vk::AccessFlagBits::eNone);
            barrier.setDstAccessMask(vk::AccessFlagBits::eNone);
            barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
            barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
            barrier.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
            cmd->cmdBuf.pipelineBarrier(ps::eAllCommands, ps::eAllCommands, vk::DependencyFlags(), {}, {}, imageBarriers);
        }
        m_device->submitOneshot(cmd);
    }

    void LerApp::notifyResize()
    {
        m_renderer.resize(m_device, m_swapChain.extent);
        for(auto& pass : m_renderPasses)
            pass->resize(m_device, m_swapChain.extent);
    }

    void LerApp::resize(int width, int height)
    {
        m_config.width = static_cast<uint32_t>(width);
        m_config.height = static_cast<uint32_t>(height);
        updateSwapChain();
        notifyResize();
    }

    void LerApp::run()
    {
        log::info("LER Run");
        vk::Result result;
        CommandPtr cmd;
        CameraParam camera;
        SceneParam scene;
        uint32_t swapChainIndex = 0;
        const VulkanContext& context = m_vulkan->getVulkanContext();
        vk::Queue queue = context.device.getQueue(context.graphicsQueueFamily, 0);
        auto presentSemaphore = context.device.createSemaphoreUnique({});

        double x, y;

        for(auto& pass : m_renderPasses)
            pass->init(m_device, m_renderer, m_window);

        notifyResize();

        while(!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();

            // Acquire next frame
            result = context.device.acquireNextImageKHR(m_swapChain.handle.get(), std::numeric_limits<uint64_t>::max(), presentSemaphore.get(), vk::Fence(), &swapChainIndex);
            assert(result == vk::Result::eSuccess);

            // All commands will now wait next image
            m_device->queueWaitForSemaphore(CommandQueue::Graphics, presentSemaphore.get(), 0);

            // Layout transition
            cmd = m_device->createCommand();
            cmd->addBarrier(m_images[swapChainIndex], vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput);
            m_device->submitCommand(cmd);

            // Update Camera
            if(isCursorLock())
            {
                glfwGetCursorPos(m_window, &x, &y);
                m_controller->motionCallback(glm::vec2(x, y));
            }

            // Begin
            for(auto& pass : m_renderPasses)
                pass->prepare(m_device);

            m_physx->simulate();
            m_world.progress();

            //m_controller->updateMatrices();
            camera.proj = m_controller->getProjMatrix();
            camera.view = m_controller->getViewMatrix();
            //camera.proj[1][1] *= -1;

            if(m_selected != flecs::entity::null())
            {
                const auto* t = m_selected.get<CTransform>();
                const auto* m = m_selected.get<CMesh>();
                glm::vec3 min = m->min;
                glm::vec3 max = m->max;

                min = glm::vec3(t->model * glm::vec4(min, 1.f));
                max = glm::vec3(t->model * glm::vec4(max, 1.f));
                glm::vec3 center = (min+max)/2.f;

                glm::mat4 object = glm::translate(glm::mat4(1.f), center);
                ImGuizmo::BeginFrame();
                ImGuizmo::SetRect(0, 0, m_config.width, m_config.height);
                ImGuizmo::Manipulate(glm::value_ptr(camera.view), glm::value_ptr(camera.proj), ImGuizmo::OPERATION::TRANSLATE, ImGuizmo::WORLD, glm::value_ptr(object));

                //m_selected.set<CTransform>({t});
                if(ImGui::GetIO().KeyCtrl)
                    m_selected = flecs::entity::null();
            }

            // Render
            RenderParams params(camera);
            for(auto& pass : m_renderPasses)
                pass->render(m_device, m_targets[swapChainIndex], m_renderer, params);

            // Next command will signal present image
            m_device->queueSignalSemaphore(CommandQueue::Graphics, presentSemaphore.get(), 0);

            // Layout transition
            cmd = m_device->createCommand();
            cmd->addBarrier(m_images[swapChainIndex], vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe);
            m_device->submitCommand(cmd);

            // Present
            vk::PresentInfoKHR presentInfo;
            presentInfo.setWaitSemaphoreCount(1);
            presentInfo.setPWaitSemaphores(&presentSemaphore.get());
            presentInfo.setSwapchainCount(1);
            presentInfo.setPSwapchains(&m_swapChain.handle.get());
            presentInfo.setPImageIndices(&swapChainIndex);

            result = queue.presentKHR(&presentInfo);
            assert(result == vk::Result::eSuccess);

            for(SceneBuffers* s : SceneImporter::PollUpdate(m_device, m_world))
            {
                for(auto& pass : m_renderPasses)
                    pass->onSceneChange(m_device, s);
            }

            AsyncQueue<AsyncRequest>::Update(*this);
            AsyncQueue<AsyncRequest>::Update(*this);
            Event::GetDispatcher().update();
            m_device->runGarbageCollection();
        }

        // Stall device
        Async::GetPool().reset(); // Silly because the pool keep last task...
        context.device.waitIdle();
        Service::Destroy();
        for(auto& pass : m_renderPasses)
            pass->clean();
    }

    void LerApp::glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        auto app = static_cast<LerApp*>(glfwGetWindowUserPointer(window));
        if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        if(key == GLFW_KEY_SPACE && action == GLFW_PRESS)
        {
            if(glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            else
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        if(key == GLFW_KEY_BACKSPACE && action == GLFW_PRESS && app->m_selected != flecs::entity::null())
        {
            app->m_selected.destruct();
            app->m_selected = flecs::entity::null();
        }

        app->m_controller->keyboardCallback(key, action, 0.002);
    }

    void LerApp::glfw_mouse_callback(GLFWwindow* window, int button, int action, int mods)
    {
        auto app = static_cast<LerApp*>(glfwGetWindowUserPointer(window));
        app->m_controller->mouseCallback(button, action);
        if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
        {
            double x, y;
            glm::vec3 o;
            glfwGetCursorPos(window, &x, &y);
            auto p = glm::vec2((float)x, (float)y);

            auto* scene = app->m_physx->getScene();
            glm::vec3 rayDir = app->m_controller->rayCast(p, o);

            physx::PxRaycastBuffer hit;
            physx::PxVec3 origin(o.x, o.y, o.z);
            physx::PxVec3 unitDir(rayDir.x, rayDir.y, rayDir.z);
            if(scene->raycast(origin, unitDir, 1000, hit))
            {
                auto* actor = hit.getAnyHit(0).actor;
                auto* tag = static_cast<ler::UserData*>(actor->userData);
                app->m_selected = tag->e;
            }
        }
    }

    void LerApp::glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
    {
        auto app = static_cast<LerApp*>(glfwGetWindowUserPointer(window));
        app->m_controller->scrollCallback(glm::vec2(xoffset, yoffset));
    }

    void LerApp::glfw_size_callback(GLFWwindow* window, int width, int height)
    {
        auto app = static_cast<LerApp*>(glfwGetWindowUserPointer(window));
        app->resize(width, height);
    }
}