//
// Created by loulfy on 07/07/2023.
//

#ifndef LER_SCN_HPP
#define LER_SCN_HPP

#include "ler_vki.hpp"

namespace ler
{
    struct SwapChain
    {
        vk::UniqueSwapchainKHR handle;
        vk::Format format = vk::Format::eB8G8R8A8Unorm;
        vk::Extent2D extent;

        static SwapChain create(const VulkanContext& context, vk::SurfaceKHR surface, uint32_t width, uint32_t height, bool vSync);
        static vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes, bool vSync);
        static vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height);
        static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
    };
}

#endif //LER_SCN_HPP
