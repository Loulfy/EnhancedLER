//
// Created by loulfy on 07/07/2023.
//

#ifndef LER_VKI_HPP
#define LER_VKI_HPP

#include <set>
#include <map>
#include <vector>
#include <filesystem>
namespace fs = std::filesystem;

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>
#include <rtxmu/VkAccelStructManager.h>

#include "ler_log.hpp"

namespace ler
{
    struct VulkanContext
    {
        vk::Instance instance;
        vk::PhysicalDevice physicalDevice;
        vk::Device device;
        uint32_t graphicsQueueFamily = UINT32_MAX;
        uint32_t transferQueueFamily = UINT32_MAX;
        VmaAllocator allocator = nullptr;
        vk::PipelineCache pipelineCache;
    };

    struct LerConfig
    {
        uint32_t width = 1920;
        uint32_t height = 1080;
        bool resizable = true;
        bool debug = true;
        bool vsync = true;
        bool msaa = true;

        std::vector<const char*> extensions;
    };

    class VulkanInitializer
    {
    public:

        ~VulkanInitializer();
        explicit VulkanInitializer(LerConfig& cfg);
        [[nodiscard]] const VulkanContext& getVulkanContext() const { return m_context; }

    private:

        vk::UniqueInstance m_instance;
        uint32_t m_graphicsQueueFamily = UINT32_MAX;
        uint32_t m_transferQueueFamily = UINT32_MAX;
        vk::UniqueDevice m_device;
        vk::PhysicalDevice m_physicalDevice;
        vk::UniquePipelineCache m_pipelineCache;
        VulkanContext m_context;
    };
}

#endif //LER_VKI_HPP
