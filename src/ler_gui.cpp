//
// Created by loulfy on 12/07/2023.
//

#define GLFW_INCLUDE_NONE // Do not include any OpenGL/Vulkan headers

#include <GLFW/glfw3.h>

#include "ler_gui.hpp"
#include "ler_res.hpp"

namespace ler
{
    /*void ImGuiPass::createFontsTexture(LerDevicePtr& device)
    {
        unsigned char* pixels;
        int width, height;
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        size_t upload_size = width * height * 4 * sizeof(char);

        m_fontsTexture = device->createTexture(vk::Format::eR8G8B8A8Unorm, vk::Extent2D(width, height));
        m_fontSampler = device->createSampler(vk::SamplerAddressMode::eRepeat, true);

        BufferPtr staging = device->createBuffer(upload_size, vk::BufferUsageFlagBits(), true);
        staging->uploadFromMemory(pixels, upload_size);

        CommandPtr cmd = device->createCommand();
        cmd->copyBufferToTexture(staging, m_fontsTexture);
        device->submitAndWait(cmd);
    }*/

    vk::UniqueDescriptorPool ImGuiPass::createPool(LerDevicePtr& device)
    {
        auto ctx = device->getVulkanContext();
        std::array<vk::DescriptorPoolSize, 11> pool_sizes =
        {{
             {vk::DescriptorType::eSampler, 1000},
             {vk::DescriptorType::eCombinedImageSampler, 1000},
             {vk::DescriptorType::eSampledImage, 1000},
             {vk::DescriptorType::eStorageImage, 1000},
             {vk::DescriptorType::eUniformTexelBuffer, 1000},
             {vk::DescriptorType::eStorageTexelBuffer, 1000},
             {vk::DescriptorType::eUniformBuffer, 1000},
             {vk::DescriptorType::eStorageBuffer, 1000},
             {vk::DescriptorType::eUniformBufferDynamic, 1000},
             {vk::DescriptorType::eStorageBufferDynamic, 1000},
             {vk::DescriptorType::eInputAttachment, 1000}
         }};

        vk::DescriptorPoolCreateInfo poolInfo;
        poolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
        poolInfo.setPoolSizes(pool_sizes);
        poolInfo.setMaxSets(1000);

        return ctx.device.createDescriptorPoolUnique(poolInfo);
    }

    void ImGuiPass::init(LerDevicePtr& device, GLFWwindow* window)
    {
        m_descriptorPool = createPool(device);

        const VulkanContext& context = device->getVulkanContext();

        int w, h;
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        glfwGetFramebufferSize(window, &w, &h);
        io.DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));
        io.ConfigFlags = ImGuiConfigFlags_DockingEnable;
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForVulkan(window, true);

        //this initializes ImGui for Vulkan
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = context.instance;
        init_info.PhysicalDevice = context.physicalDevice;
        init_info.Device = context.device;
        init_info.Queue = context.device.getQueue(context.graphicsQueueFamily, 0);
        init_info.DescriptorPool = m_descriptorPool.get();
        init_info.ColorAttachmentFormat = VK_FORMAT_B8G8R8A8_UNORM;
        init_info.UseDynamicRendering = true;
        init_info.MinImageCount = 2;
        init_info.ImageCount = 2;
        init_info.Subpass = 0;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        ImGui_ImplVulkan_Init(&init_info, nullptr);

        CommandPtr cmd = device->createCommand();
        ImGui_ImplVulkan_CreateFontsTexture(cmd->cmdBuf);
        device->submitAndWait(cmd);
    }

    void ImGuiPass::prepare(LerDevicePtr& device)
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiPass::render(LerDevicePtr& device, FrameWindow& frame, BatchedMesh& batch, const CameraParam& camera, SceneParam& scene)
    {
        ImGui::Render();
        // Record dear ImGui primitives into command buffer
        ImDrawData* draw_data = ImGui::GetDrawData();

        CommandPtr cmd = device->createCommand();

        vk::RenderingAttachmentInfo colorInfo;
        colorInfo.setImageView(frame.image->view());
        colorInfo.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
        colorInfo.setStoreOp(vk::AttachmentStoreOp::eStore);
        colorInfo.setLoadOp(vk::AttachmentLoadOp::eLoad);

        vk::RenderingInfo renderInfo;
        vk::Viewport viewport = LerDevice::createViewport(frame.extent);
        vk::Rect2D renderArea(vk::Offset2D(), frame.extent);
        renderInfo.setRenderArea(renderArea);
        renderInfo.setLayerCount(1);
        renderInfo.setColorAttachmentCount(1);
        renderInfo.setPColorAttachments(&colorInfo);
        cmd->cmdBuf.beginRendering(renderInfo);
        cmd->cmdBuf.setScissor(0, 1, &renderArea);
        cmd->cmdBuf.setViewport(0, 1, &viewport);
        ImGui_ImplVulkan_RenderDrawData(draw_data, cmd->cmdBuf);
        cmd->cmdBuf.endRendering();
        device->submitCommand(cmd);
    }

    void ImGuiPass::clean()
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    VkDescriptorSet ImGuiPass::createTextureDescriptorSet(const TexturePtr& texture, const vk::ImageSubresourceRange& sub, const vk::Sampler sampler)
    {
        auto view = static_cast<VkImageView>(texture->view(sub));
        auto samp = static_cast<VkSampler>(sampler);
        auto aspect = LerDevice::guessImageAspectFlags(texture->info.format);
        if(aspect == vk::ImageAspectFlagBits::eDepth)
            return ImGui_ImplVulkan_AddTexture(samp, view, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
        else if(aspect == (vk::ImageAspectFlagBits::eStencil | vk::ImageAspectFlagBits::eDepth))
            return ImGui_ImplVulkan_AddTexture(samp, view, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        else if(aspect == vk::ImageAspectFlagBits::eColor)
            return ImGui_ImplVulkan_AddTexture(samp, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        return nullptr;
    }

    void TextureViewerPass::init(LerDevicePtr& device, GLFWwindow* window)
    {
        m_sampler = device->createSampler(vk::SamplerAddressMode::eClampToEdge, false);
    }

    void TextureViewerPass::render(LerDevicePtr& device, FrameWindow& frame, BatchedMesh& batch, const CameraParam& camera, SceneParam& scene)
    {
        ImGui::Begin("Texture Viewer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        if (ImGui::BeginCombo("##combo", current_item)) // The second parameter is the label previewed before opening the combo.
        {
            auto mgr = device->getTexturePool();
            for (const auto& item : mgr->getTextureCacheRef())
            {
                const std::string& key = item.first;
                bool is_selected = (current_item == key.c_str()); // You can store your selection however you want, outside or inside your objects
                if (ImGui::Selectable(key.c_str(), is_selected))
                {
                    current_item = key.c_str();
                    if(!cache.contains(key))
                    {
                        auto texture = mgr->getTextureFromIndex(item.second);
                        if(texture)
                            cache.emplace(key, ImGuiPass::createTextureDescriptorSet(texture, Texture::DefaultSub, m_sampler.get()));
                    }
                    ds = cache.at(key);
                }
                if (is_selected)
                    ImGui::SetItemDefaultFocus();   // You may set the initial focus when opening the combo (scrolling + for keyboard navigation support)
            }
            ImGui::EndCombo();
        }
        using IA = vk::ImageAspectFlagBits;
        if(ds)
            ImGui::Image(ds, ImVec2(256, 256));

        /*for(size_t i = 0; i < 4; ++i)
        {
            auto& d = depth[i];
            if(d)
                ImGui::Image(d, ImVec2(256, 256));
            else
                d = ImGuiPass::createTextureDescriptorSet(device->getRenderTarget(RT::eShadowMap), vk::ImageSubresourceRange(IA::eDepth,0,1,i,1), m_sampler.get());
            if(i%2 == 0)
                ImGui::SameLine();
        }*/
        if(mip)
            ImGui::Image(mip, ImVec2(512, 512));
        else
        {
            if(device->getRenderTarget(RT::eDepthPyramid))
                mip = ImGuiPass::createTextureDescriptorSet(device->getRenderTarget(RT::eDepthPyramid), vk::ImageSubresourceRange(IA::eColor,6,1,0,1), m_sampler.get());
        }
        ImGui::End();
    }
}