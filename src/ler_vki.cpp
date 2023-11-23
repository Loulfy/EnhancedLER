//
// Created by loulfy on 07/07/2023.
//

#define VMA_IMPLEMENTATION
#include "ler_vki.hpp"
#include <ranges>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace ler
{
    bool gpuFilter(const vk::PhysicalDevice& phyDev)
    {
        return phyDev.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
    }

    VulkanInitializer::~VulkanInitializer()
    {
        vmaDestroyAllocator(m_context.allocator);
    }

    VulkanInitializer::VulkanInitializer(ler::LerConfig& cfg)
    {
        static const vk::DynamicLoader dl;
        const auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
        rtxmu::VkBlock::getDispatchLoader().vkGetInstanceProcAddr = vkGetInstanceProcAddr;

        auto sdkVersion = vk::enumerateInstanceVersion();
        uint32_t major = VK_VERSION_MAJOR(sdkVersion);
        uint32_t minor = VK_VERSION_MINOR(sdkVersion);
        uint32_t patch = VK_VERSION_PATCH(sdkVersion);

        log::info("Vulkan SDK {}.{}.{}", major, minor, patch);
        if(minor < 3)
            log::exit("Vulkan 1.3 is not supported, please update driver");

        if (cfg.debug)
        {
            cfg.extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            cfg.extensions.emplace_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
            cfg.extensions.emplace_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
            cfg.extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        }

        std::initializer_list<const char*> layers = {
                "VK_LAYER_KHRONOS_validation"
        };

        std::vector<const char*> devices = {
                VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
                VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
                // VULKAN MEMORY ALLOCATOR
                VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
                VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
                VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
                VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
                // SWAP CHAIN
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                // RAY TRACING
                //VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
                // DYNAMIC RENDERING
                VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        };

        // Create instance
        vk::ApplicationInfo appInfo;
        appInfo.setApiVersion(VK_API_VERSION_1_3);
        appInfo.setPEngineName("LER");

        vk::InstanceCreateInfo instInfo;
        instInfo.setPApplicationInfo(&appInfo);
        instInfo.setPEnabledLayerNames(layers);
        instInfo.setPEnabledExtensionNames(cfg.extensions);
        m_instance = vk::createInstanceUnique(instInfo);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance.get());

        // Pick First GPU
        auto physicalDeviceList = m_instance->enumeratePhysicalDevices();
        m_physicalDevice = *std::ranges::find_if(physicalDeviceList, gpuFilter);
        log::info("GPU: {}", m_physicalDevice.getProperties().deviceName.data());

        std::set<std::string> supportedExtensionSet;
        for (const auto& devExt: m_physicalDevice.enumerateDeviceExtensionProperties())
            supportedExtensionSet.insert(devExt.extensionName);

        for(const auto& devExt : devices)
        {
            if(!supportedExtensionSet.contains(devExt))
            {
                std::string fatal("Mandatory Device Extension Not Supported: ");
                fatal+= devExt;
                log::exit(fatal);
            }
        }

        bool supportRayTracing = supportedExtensionSet.contains(VK_KHR_RAY_QUERY_EXTENSION_NAME);
        log::info("Support Ray Tracing: {}", supportRayTracing);

        // Device Features
        auto features = m_physicalDevice.getFeatures();

        // Find Graphics Queue
        const auto queueFamilies = m_physicalDevice.getQueueFamilyProperties();
        const auto family = std::find_if(queueFamilies.begin(), queueFamilies.end(), [](const vk::QueueFamilyProperties& queueFamily) {
            return queueFamily.queueCount > 0 && queueFamily.queueFlags & vk::QueueFlagBits::eGraphics;
        });

        auto props = m_physicalDevice.getProperties();
        log::debug("MaxTextureArrayLayers: {}", props.limits.maxImageArrayLayers);

        m_graphicsQueueFamily = std::distance(queueFamilies.begin(), family);
        m_transferQueueFamily = UINT32_MAX;

        // Find Transfer Queue (for parallel command)
        for(size_t i = 0; i < queueFamilies.size(); ++i)
        {
            if(queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & vk::QueueFlagBits::eTransfer && m_graphicsQueueFamily != i)
            {
                m_transferQueueFamily = i;
                break;
            }
        }

        if(m_transferQueueFamily == UINT32_MAX)
            throw std::runtime_error("No transfer queue available");

        // Create queues
        float queuePriority = 1.0f;
        std::initializer_list<vk::DeviceQueueCreateInfo> queueCreateInfos = {
                { {}, m_graphicsQueueFamily, 1, &queuePriority },
                { {}, m_transferQueueFamily, 1, &queuePriority },
        };

        for(auto& q : queueCreateInfos)
            log::info("Queue Family {}: {}", q.queueFamilyIndex, vk::to_string(queueFamilies[q.queueFamilyIndex].queueFlags));

        vk::PhysicalDeviceProperties2 p2;
        vk::PhysicalDeviceSubgroupProperties subgroupProps;
        p2.pNext = &subgroupProps;
        m_physicalDevice.getProperties2(&p2);
        log::info("SubgroupSize: {}", subgroupProps.subgroupSize);
        log::info("Subgroup Support: {}", vk::to_string(subgroupProps.supportedOperations));

        // Create Device
        vk::DeviceCreateInfo deviceInfo;
        deviceInfo.setQueueCreateInfos(queueCreateInfos);
        deviceInfo.setPEnabledExtensionNames(devices);
        deviceInfo.setPEnabledLayerNames(layers);
        deviceInfo.setPEnabledFeatures(&features);

        vk::PhysicalDeviceVulkan11Features vulkan11Features;
        vulkan11Features.setShaderDrawParameters(true);
        vk::PhysicalDeviceVulkan12Features vulkan12Features;

        vulkan12Features.setDescriptorIndexing(true);
        vulkan12Features.setRuntimeDescriptorArray(true);
        vulkan12Features.setDescriptorBindingPartiallyBound(true);
        vulkan12Features.setDescriptorBindingStorageBufferUpdateAfterBind(true);
        vulkan12Features.setDescriptorBindingSampledImageUpdateAfterBind(true);
        vulkan12Features.setDescriptorBindingUniformBufferUpdateAfterBind(true);
        vulkan12Features.setDescriptorBindingStorageImageUpdateAfterBind(true);
        vulkan12Features.setDescriptorBindingVariableDescriptorCount(true);
        vulkan12Features.setTimelineSemaphore(true);
        vulkan12Features.setBufferDeviceAddress(true);
        vulkan12Features.setShaderSampledImageArrayNonUniformIndexing(true);
        vulkan12Features.setSamplerFilterMinmax(true);

        vulkan12Features.setBufferDeviceAddress(true);
        vulkan12Features.setRuntimeDescriptorArray(true);
        vulkan12Features.setDescriptorBindingVariableDescriptorCount(true);
        vulkan12Features.setShaderSampledImageArrayNonUniformIndexing(true);
        vulkan12Features.setDrawIndirectCount(true);

        vk::PhysicalDeviceVulkan13Features vulkan13Features;
        vulkan13Features.setMaintenance4(true);
        vulkan13Features.setDynamicRendering(true);
        vulkan13Features.setSynchronization2(true);

        vk::StructureChain<vk::DeviceCreateInfo,
                vk::PhysicalDeviceRayQueryFeaturesKHR,
                /*vk::PhysicalDeviceRayTracingPipelineFeaturesKHR,*/
                vk::PhysicalDeviceAccelerationStructureFeaturesKHR,
                vk::PhysicalDeviceVulkan11Features,
                vk::PhysicalDeviceVulkan12Features,
                vk::PhysicalDeviceVulkan13Features> createInfoChain(
                        deviceInfo,
                        {supportRayTracing},
                        {supportRayTracing},
                        /*{false},*/
                        vulkan11Features,
                        vulkan12Features,
                        vulkan13Features);
        m_device = m_physicalDevice.createDeviceUnique(createInfoChain.get<vk::DeviceCreateInfo>());
        VULKAN_HPP_DEFAULT_DISPATCHER.init(m_device.get());

        m_pipelineCache = m_device->createPipelineCacheUnique({});

        // Create Memory Allocator
        VmaAllocatorCreateInfo allocatorCreateInfo = {};
        allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        allocatorCreateInfo.device = m_device.get();
        allocatorCreateInfo.instance = m_instance.get();
        allocatorCreateInfo.physicalDevice = m_physicalDevice;
        allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
        allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

        VmaAllocator allocator;
        vmaCreateAllocator(&allocatorCreateInfo, &allocator);

        m_context.device = m_device.get();
        m_context.instance = m_instance.get();
        m_context.physicalDevice = m_physicalDevice;
        m_context.graphicsQueueFamily = m_graphicsQueueFamily;
        m_context.transferQueueFamily = m_transferQueueFamily;
        m_context.pipelineCache = m_pipelineCache.get();
        m_context.allocator = allocator;
    }
}