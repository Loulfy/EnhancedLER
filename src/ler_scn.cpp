//
// Created by loulfy on 07/07/2023.
//

#include "ler_scn.hpp"

namespace ler
{
    vk::PresentModeKHR
    SwapChain::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes, bool vSync)
    {
        for (const auto& availablePresentMode: availablePresentModes)
        {
            if (availablePresentMode == vk::PresentModeKHR::eFifo && vSync)
                return availablePresentMode;
            if (availablePresentMode == vk::PresentModeKHR::eMailbox && !vSync)
                return availablePresentMode;
        }
        return vk::PresentModeKHR::eImmediate;
    }

    vk::SurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
    {
        if (availableFormats.size() == 1 && availableFormats[0].format == vk::Format::eUndefined)
        {
            return {vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear};
        }

        for (const auto& format: availableFormats)
        {
            if (format.format == vk::Format::eB8G8R8A8Unorm && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
                return format;
        }

        log::exit("found no suitable surface format");
        return {};
    }

    vk::Extent2D
    SwapChain::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height)
    {
        if (capabilities.currentExtent.width == UINT32_MAX)
        {
            vk::Extent2D extent(width, height);
            vk::Extent2D minExtent = capabilities.minImageExtent;
            vk::Extent2D maxExtent = capabilities.maxImageExtent;
            extent.width = std::clamp(extent.width, minExtent.width, maxExtent.width);
            extent.height = std::clamp(extent.height, minExtent.height, maxExtent.height);
            return extent;
        }
        else
        {
            return capabilities.currentExtent;
        }
    }

    SwapChain
    SwapChain::create(const VulkanContext& context, vk::SurfaceKHR surface, uint32_t width, uint32_t height, bool vSync)
    {
        // Setup viewports, vSync
        std::vector<vk::SurfaceFormatKHR> surfaceFormats = context.physicalDevice.getSurfaceFormatsKHR(surface);
        vk::SurfaceCapabilitiesKHR surfaceCapabilities = context.physicalDevice.getSurfaceCapabilitiesKHR(surface);
        std::vector<vk::PresentModeKHR> surfacePresentModes = context.physicalDevice.getSurfacePresentModesKHR(surface);

        vk::Extent2D extent = SwapChain::chooseSwapExtent(surfaceCapabilities, width, height);
        vk::SurfaceFormatKHR surfaceFormat = SwapChain::chooseSwapSurfaceFormat(surfaceFormats);
        vk::PresentModeKHR presentMode = SwapChain::chooseSwapPresentMode(surfacePresentModes, vSync);

        uint32_t backBufferCount = std::clamp(surfaceCapabilities.maxImageCount, 1U, 2U);

        // Create swapChain
        using vkIU = vk::ImageUsageFlagBits;
        vk::SwapchainCreateInfoKHR createInfo;
        createInfo.setSurface(surface);
        createInfo.setMinImageCount(backBufferCount);
        createInfo.setImageFormat(surfaceFormat.format);
        createInfo.setImageColorSpace(surfaceFormat.colorSpace);
        createInfo.setImageExtent(extent);
        createInfo.setImageArrayLayers(1);
        createInfo.setImageUsage(vkIU::eColorAttachment | vkIU::eTransferDst);
        createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
        createInfo.setPreTransform(surfaceCapabilities.currentTransform);
        createInfo.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
        createInfo.setPresentMode(presentMode);
        createInfo.setClipped(true);

        log::info("SwapChain: Images({}), Extent({}x{}), Format({}), Present({})",
            backBufferCount,
            extent.width, extent.height,
            vk::to_string(surfaceFormat.format),
            vk::to_string(presentMode)
        );

        SwapChain swapChain;
        swapChain.format = surfaceFormat.format;
        swapChain.extent = extent;
        swapChain.handle = context.device.createSwapchainKHRUnique(createInfo);
        return swapChain;
    }
}