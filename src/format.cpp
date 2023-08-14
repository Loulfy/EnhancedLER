//
// Created by loulfy on 15/12/2022.
//

#include "ler_dev.hpp"
#include "ler_img.hpp"

namespace ler
{
    // SPRIV Reflect
    uint32_t LerDevice::formatSize(VkFormat format)
    {
        uint32_t result = 0;
        switch (format)
        {
            case VK_FORMAT_UNDEFINED: result = 0; break;
            case VK_FORMAT_R4G4_UNORM_PACK8: result = 1; break;
            case VK_FORMAT_R4G4B4A4_UNORM_PACK16: result = 2; break;
            case VK_FORMAT_B4G4R4A4_UNORM_PACK16: result = 2; break;
            case VK_FORMAT_R5G6B5_UNORM_PACK16: result = 2; break;
            case VK_FORMAT_B5G6R5_UNORM_PACK16: result = 2; break;
            case VK_FORMAT_R5G5B5A1_UNORM_PACK16: result = 2; break;
            case VK_FORMAT_B5G5R5A1_UNORM_PACK16: result = 2; break;
            case VK_FORMAT_A1R5G5B5_UNORM_PACK16: result = 2; break;
            case VK_FORMAT_R8_UNORM: result = 1; break;
            case VK_FORMAT_R8_SNORM: result = 1; break;
            case VK_FORMAT_R8_USCALED: result = 1; break;
            case VK_FORMAT_R8_SSCALED: result = 1; break;
            case VK_FORMAT_R8_UINT: result = 1; break;
            case VK_FORMAT_R8_SINT: result = 1; break;
            case VK_FORMAT_R8_SRGB: result = 1; break;
            case VK_FORMAT_R8G8_UNORM: result = 2; break;
            case VK_FORMAT_R8G8_SNORM: result = 2; break;
            case VK_FORMAT_R8G8_USCALED: result = 2; break;
            case VK_FORMAT_R8G8_SSCALED: result = 2; break;
            case VK_FORMAT_R8G8_UINT: result = 2; break;
            case VK_FORMAT_R8G8_SINT: result = 2; break;
            case VK_FORMAT_R8G8_SRGB: result = 2; break;
            case VK_FORMAT_R8G8B8_UNORM: result = 3; break;
            case VK_FORMAT_R8G8B8_SNORM: result = 3; break;
            case VK_FORMAT_R8G8B8_USCALED: result = 3; break;
            case VK_FORMAT_R8G8B8_SSCALED: result = 3; break;
            case VK_FORMAT_R8G8B8_UINT: result = 3; break;
            case VK_FORMAT_R8G8B8_SINT: result = 3; break;
            case VK_FORMAT_R8G8B8_SRGB: result = 3; break;
            case VK_FORMAT_B8G8R8_UNORM: result = 3; break;
            case VK_FORMAT_B8G8R8_SNORM: result = 3; break;
            case VK_FORMAT_B8G8R8_USCALED: result = 3; break;
            case VK_FORMAT_B8G8R8_SSCALED: result = 3; break;
            case VK_FORMAT_B8G8R8_UINT: result = 3; break;
            case VK_FORMAT_B8G8R8_SINT: result = 3; break;
            case VK_FORMAT_B8G8R8_SRGB: result = 3; break;
            case VK_FORMAT_R8G8B8A8_UNORM: result = 4; break;
            case VK_FORMAT_R8G8B8A8_SNORM: result = 4; break;
            case VK_FORMAT_R8G8B8A8_USCALED: result = 4; break;
            case VK_FORMAT_R8G8B8A8_SSCALED: result = 4; break;
            case VK_FORMAT_R8G8B8A8_UINT: result = 4; break;
            case VK_FORMAT_R8G8B8A8_SINT: result = 4; break;
            case VK_FORMAT_R8G8B8A8_SRGB: result = 4; break;
            case VK_FORMAT_B8G8R8A8_UNORM: result = 4; break;
            case VK_FORMAT_B8G8R8A8_SNORM: result = 4; break;
            case VK_FORMAT_B8G8R8A8_USCALED: result = 4; break;
            case VK_FORMAT_B8G8R8A8_SSCALED: result = 4; break;
            case VK_FORMAT_B8G8R8A8_UINT: result = 4; break;
            case VK_FORMAT_B8G8R8A8_SINT: result = 4; break;
            case VK_FORMAT_B8G8R8A8_SRGB: result = 4; break;
            case VK_FORMAT_A8B8G8R8_UNORM_PACK32: result = 4; break;
            case VK_FORMAT_A8B8G8R8_SNORM_PACK32: result = 4; break;
            case VK_FORMAT_A8B8G8R8_USCALED_PACK32: result = 4; break;
            case VK_FORMAT_A8B8G8R8_SSCALED_PACK32: result = 4; break;
            case VK_FORMAT_A8B8G8R8_UINT_PACK32: result = 4; break;
            case VK_FORMAT_A8B8G8R8_SINT_PACK32: result = 4; break;
            case VK_FORMAT_A8B8G8R8_SRGB_PACK32: result = 4; break;
            case VK_FORMAT_A2R10G10B10_UNORM_PACK32: result = 4; break;
            case VK_FORMAT_A2R10G10B10_SNORM_PACK32: result = 4; break;
            case VK_FORMAT_A2R10G10B10_USCALED_PACK32: result = 4; break;
            case VK_FORMAT_A2R10G10B10_SSCALED_PACK32: result = 4; break;
            case VK_FORMAT_A2R10G10B10_UINT_PACK32: result = 4; break;
            case VK_FORMAT_A2R10G10B10_SINT_PACK32: result = 4; break;
            case VK_FORMAT_A2B10G10R10_UNORM_PACK32: result = 4; break;
            case VK_FORMAT_A2B10G10R10_SNORM_PACK32: result = 4; break;
            case VK_FORMAT_A2B10G10R10_USCALED_PACK32: result = 4; break;
            case VK_FORMAT_A2B10G10R10_SSCALED_PACK32: result = 4; break;
            case VK_FORMAT_A2B10G10R10_UINT_PACK32: result = 4; break;
            case VK_FORMAT_A2B10G10R10_SINT_PACK32: result = 4; break;
            case VK_FORMAT_R16_UNORM: result = 2; break;
            case VK_FORMAT_R16_SNORM: result = 2; break;
            case VK_FORMAT_R16_USCALED: result = 2; break;
            case VK_FORMAT_R16_SSCALED: result = 2; break;
            case VK_FORMAT_R16_UINT: result = 2; break;
            case VK_FORMAT_R16_SINT: result = 2; break;
            case VK_FORMAT_R16_SFLOAT: result = 2; break;
            case VK_FORMAT_R16G16_UNORM: result = 4; break;
            case VK_FORMAT_R16G16_SNORM: result = 4; break;
            case VK_FORMAT_R16G16_USCALED: result = 4; break;
            case VK_FORMAT_R16G16_SSCALED: result = 4; break;
            case VK_FORMAT_R16G16_UINT: result = 4; break;
            case VK_FORMAT_R16G16_SINT: result = 4; break;
            case VK_FORMAT_R16G16_SFLOAT: result = 4; break;
            case VK_FORMAT_R16G16B16_UNORM: result = 6; break;
            case VK_FORMAT_R16G16B16_SNORM: result = 6; break;
            case VK_FORMAT_R16G16B16_USCALED: result = 6; break;
            case VK_FORMAT_R16G16B16_SSCALED: result = 6; break;
            case VK_FORMAT_R16G16B16_UINT: result = 6; break;
            case VK_FORMAT_R16G16B16_SINT: result = 6; break;
            case VK_FORMAT_R16G16B16_SFLOAT: result = 6; break;
            case VK_FORMAT_R16G16B16A16_UNORM: result = 8; break;
            case VK_FORMAT_R16G16B16A16_SNORM: result = 8; break;
            case VK_FORMAT_R16G16B16A16_USCALED: result = 8; break;
            case VK_FORMAT_R16G16B16A16_SSCALED: result = 8; break;
            case VK_FORMAT_R16G16B16A16_UINT: result = 8; break;
            case VK_FORMAT_R16G16B16A16_SINT: result = 8; break;
            case VK_FORMAT_R16G16B16A16_SFLOAT: result = 8; break;
            case VK_FORMAT_R32_UINT: result = 4; break;
            case VK_FORMAT_R32_SINT: result = 4; break;
            case VK_FORMAT_R32_SFLOAT: result = 4; break;
            case VK_FORMAT_R32G32_UINT: result = 8; break;
            case VK_FORMAT_R32G32_SINT: result = 8; break;
            case VK_FORMAT_R32G32_SFLOAT: result = 8; break;
            case VK_FORMAT_R32G32B32_UINT: result = 12; break;
            case VK_FORMAT_R32G32B32_SINT: result = 12; break;
            case VK_FORMAT_R32G32B32_SFLOAT: result = 12; break;
            case VK_FORMAT_R32G32B32A32_UINT: result = 16; break;
            case VK_FORMAT_R32G32B32A32_SINT: result = 16; break;
            case VK_FORMAT_R32G32B32A32_SFLOAT: result = 16; break;
            case VK_FORMAT_R64_UINT: result = 8; break;
            case VK_FORMAT_R64_SINT: result = 8; break;
            case VK_FORMAT_R64_SFLOAT: result = 8; break;
            case VK_FORMAT_R64G64_UINT: result = 16; break;
            case VK_FORMAT_R64G64_SINT: result = 16; break;
            case VK_FORMAT_R64G64_SFLOAT: result = 16; break;
            case VK_FORMAT_R64G64B64_UINT: result = 24; break;
            case VK_FORMAT_R64G64B64_SINT: result = 24; break;
            case VK_FORMAT_R64G64B64_SFLOAT: result = 24; break;
            case VK_FORMAT_R64G64B64A64_UINT: result = 32; break;
            case VK_FORMAT_R64G64B64A64_SINT: result = 32; break;
            case VK_FORMAT_R64G64B64A64_SFLOAT: result = 32; break;
            case VK_FORMAT_B10G11R11_UFLOAT_PACK32: result = 4; break;
            case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: result = 4; break;

            default:
                break;
        }
        return result;
    }

    // GLI
    VkFormat gli_convert_format(gli::format format) {
        switch (format) {
            case gli::FORMAT_RG4_UNORM_PACK8:
                return VK_FORMAT_R4G4_UNORM_PACK8;
            case gli::FORMAT_RGBA4_UNORM_PACK16:
                return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
            case gli::FORMAT_BGRA4_UNORM_PACK16:
                return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
            case gli::FORMAT_R5G6B5_UNORM_PACK16:
                return VK_FORMAT_R5G6B5_UNORM_PACK16;
            case gli::FORMAT_B5G6R5_UNORM_PACK16:
                return VK_FORMAT_B5G6R5_UNORM_PACK16;
            case gli::FORMAT_RGB5A1_UNORM_PACK16:
                return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
            case gli::FORMAT_BGR5A1_UNORM_PACK16:
                return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
            case gli::FORMAT_A1RGB5_UNORM_PACK16:
                return VK_FORMAT_A1R5G5B5_UNORM_PACK16;
            case gli::FORMAT_R8_UNORM_PACK8:
                return VK_FORMAT_R8_UNORM;
            case gli::FORMAT_R8_SNORM_PACK8:
                return VK_FORMAT_R8_SNORM;
            case gli::FORMAT_R8_USCALED_PACK8:
                return VK_FORMAT_R8_USCALED;
            case gli::FORMAT_R8_SSCALED_PACK8:
                return VK_FORMAT_R8_SSCALED;
            case gli::FORMAT_R8_UINT_PACK8:
                return VK_FORMAT_R8_UINT;
            case gli::FORMAT_R8_SINT_PACK8:
                return VK_FORMAT_R8_SINT;
            case gli::FORMAT_R8_SRGB_PACK8:
                return VK_FORMAT_R8_SRGB;
            case gli::FORMAT_RG8_UNORM_PACK8:
                return VK_FORMAT_R8G8_UNORM;
            case gli::FORMAT_RG8_SNORM_PACK8:
                return VK_FORMAT_R8G8_SNORM;
            case gli::FORMAT_RG8_USCALED_PACK8:
                return VK_FORMAT_R8G8_USCALED;
            case gli::FORMAT_RG8_SSCALED_PACK8:
                return VK_FORMAT_R8G8_SSCALED;
            case gli::FORMAT_RG8_UINT_PACK8:
                return VK_FORMAT_R8G8_UINT;
            case gli::FORMAT_RG8_SINT_PACK8:
                return VK_FORMAT_R8G8_SINT;
            case gli::FORMAT_RG8_SRGB_PACK8:
                return VK_FORMAT_R8G8_SRGB;
            case gli::FORMAT_RGB8_UNORM_PACK8:
                return VK_FORMAT_R8G8B8_UNORM;
            case gli::FORMAT_RGB8_SNORM_PACK8:
                return VK_FORMAT_R8G8B8_SNORM;
            case gli::FORMAT_RGB8_USCALED_PACK8:
                return VK_FORMAT_R8G8B8_USCALED;
            case gli::FORMAT_RGB8_SSCALED_PACK8:
                return VK_FORMAT_R8G8B8_SSCALED;
            case gli::FORMAT_RGB8_UINT_PACK8:
                return VK_FORMAT_R8G8B8_UINT;
            case gli::FORMAT_RGB8_SINT_PACK8:
                return VK_FORMAT_R8G8B8_SINT;
            case gli::FORMAT_RGB8_SRGB_PACK8:
                return VK_FORMAT_R8G8B8_SRGB;
            case gli::FORMAT_BGR8_UNORM_PACK8:
                return VK_FORMAT_B8G8R8_UNORM;
            case gli::FORMAT_BGR8_SNORM_PACK8:
                return VK_FORMAT_B8G8R8_SNORM;
            case gli::FORMAT_BGR8_USCALED_PACK8:
                return VK_FORMAT_B8G8R8_USCALED;
            case gli::FORMAT_BGR8_SSCALED_PACK8:
                return VK_FORMAT_B8G8R8_SSCALED;
            case gli::FORMAT_BGR8_UINT_PACK8:
                return VK_FORMAT_B8G8R8_UINT;
            case gli::FORMAT_BGR8_SINT_PACK8:
                return VK_FORMAT_B8G8R8_SINT;
            case gli::FORMAT_BGR8_SRGB_PACK8:
                return VK_FORMAT_B8G8R8_SRGB;
            case gli::FORMAT_RGBA8_UNORM_PACK8:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case gli::FORMAT_RGBA8_SNORM_PACK8:
                return VK_FORMAT_R8G8B8A8_SNORM;
            case gli::FORMAT_RGBA8_USCALED_PACK8:
                return VK_FORMAT_R8G8B8A8_USCALED;
            case gli::FORMAT_RGBA8_SSCALED_PACK8:
                return VK_FORMAT_R8G8B8A8_SSCALED;
            case gli::FORMAT_RGBA8_UINT_PACK8:
                return VK_FORMAT_R8G8B8A8_UINT;
            case gli::FORMAT_RGBA8_SINT_PACK8:
                return VK_FORMAT_R8G8B8A8_SINT;
            case gli::FORMAT_RGBA8_SRGB_PACK8:
                return VK_FORMAT_R8G8B8A8_SRGB;
            case gli::FORMAT_BGRA8_UNORM_PACK8:
                return VK_FORMAT_B8G8R8A8_UNORM;
            case gli::FORMAT_BGRA8_SNORM_PACK8:
                return VK_FORMAT_B8G8R8A8_SNORM;
            case gli::FORMAT_BGRA8_USCALED_PACK8:
                return VK_FORMAT_B8G8R8A8_USCALED;
            case gli::FORMAT_BGRA8_SSCALED_PACK8:
                return VK_FORMAT_B8G8R8A8_SSCALED;
            case gli::FORMAT_BGRA8_UINT_PACK8:
                return VK_FORMAT_B8G8R8A8_UINT;
            case gli::FORMAT_BGRA8_SINT_PACK8:
                return VK_FORMAT_B8G8R8A8_SINT;
            case gli::FORMAT_BGRA8_SRGB_PACK8:
                return VK_FORMAT_B8G8R8A8_SRGB;
            case gli::FORMAT_RGBA8_UNORM_PACK32:
                return VK_FORMAT_R8G8B8A8_UNORM;
            case gli::FORMAT_RGBA8_SNORM_PACK32:
                return VK_FORMAT_R8G8B8A8_SNORM;
            case gli::FORMAT_RGBA8_USCALED_PACK32:
                return VK_FORMAT_R8G8B8A8_USCALED;
            case gli::FORMAT_RGBA8_SSCALED_PACK32:
                return VK_FORMAT_R8G8B8A8_SSCALED;
            case gli::FORMAT_RGBA8_UINT_PACK32:
                return VK_FORMAT_R8G8B8A8_UINT;
            case gli::FORMAT_RGBA8_SINT_PACK32:
                return VK_FORMAT_R8G8B8A8_SINT;
            case gli::FORMAT_RGBA8_SRGB_PACK32:
                return VK_FORMAT_R8G8B8A8_SRGB;
            case gli::FORMAT_RGB10A2_UNORM_PACK32:
                throw std::runtime_error("Unsupported FORMAT_RGB10A2_UNORM_PACK32");
            case gli::FORMAT_RGB10A2_SNORM_PACK32:
                throw std::runtime_error("Unsupported FORMAT_RGB10A2_SNORM_PACK32");
            case gli::FORMAT_RGB10A2_USCALED_PACK32:
                throw std::runtime_error("Unsupported FORMAT_RGB10A2_USCALED_PACK32");
            case gli::FORMAT_RGB10A2_SSCALED_PACK32:
                throw std::runtime_error("Unsupported FORMAT_RGB10A2_SSCALED_PACK32");
            case gli::FORMAT_RGB10A2_UINT_PACK32:
                throw std::runtime_error("Unsupported FORMAT_RGB10A2_UINT_PACK32");
            case gli::FORMAT_RGB10A2_SINT_PACK32:
                throw std::runtime_error("Unsupported FORMAT_RGB10A2_SINT_PACK32");
            case gli::FORMAT_BGR10A2_UNORM_PACK32:
                throw std::runtime_error("Unsupported FORMAT_BGR10A2_UNORM_PACK32");
            case gli::FORMAT_BGR10A2_SNORM_PACK32:
                throw std::runtime_error("Unsupported FORMAT_BGR10A2_SNORM_PACK32");
            case gli::FORMAT_BGR10A2_USCALED_PACK32:
                throw std::runtime_error("Unsupported FORMAT_BGR10A2_USCALED_PACK32");
            case gli::FORMAT_BGR10A2_SSCALED_PACK32:
                throw std::runtime_error("Unsupported FORMAT_BGR10A2_SSCALED_PACK32");
            case gli::FORMAT_BGR10A2_UINT_PACK32:
                throw std::runtime_error("Unsupported FORMAT_BGR10A2_UINT_PACK32");
            case gli::FORMAT_BGR10A2_SINT_PACK32:
                throw std::runtime_error("Unsupported FORMAT_BGR10A2_SINT_PACK32");
            case gli::FORMAT_R16_UNORM_PACK16:
                return VK_FORMAT_R16_UNORM;
            case gli::FORMAT_R16_SNORM_PACK16:
                return VK_FORMAT_R16_SNORM;
            case gli::FORMAT_R16_USCALED_PACK16:
                return VK_FORMAT_R16_USCALED;
            case gli::FORMAT_R16_SSCALED_PACK16:
                return VK_FORMAT_R16_SSCALED;
            case gli::FORMAT_R16_UINT_PACK16:
                return VK_FORMAT_R16_UINT;
            case gli::FORMAT_R16_SINT_PACK16:
                return VK_FORMAT_R16_SINT;
            case gli::FORMAT_R16_SFLOAT_PACK16:
                return VK_FORMAT_R16_SFLOAT;
            case gli::FORMAT_RG16_UNORM_PACK16:
                return VK_FORMAT_R16_UNORM;
            case gli::FORMAT_RG16_SNORM_PACK16:
                return VK_FORMAT_R16_SNORM;
            case gli::FORMAT_RG16_USCALED_PACK16:
                return VK_FORMAT_R16G16_USCALED;
            case gli::FORMAT_RG16_SSCALED_PACK16:
                return VK_FORMAT_R16G16_SSCALED;
            case gli::FORMAT_RG16_UINT_PACK16:
                return VK_FORMAT_R16G16_UINT;
            case gli::FORMAT_RG16_SINT_PACK16:
                return VK_FORMAT_R16G16_SINT;
            case gli::FORMAT_RG16_SFLOAT_PACK16:
                return VK_FORMAT_R16G16_SFLOAT;
            case gli::FORMAT_RGB16_UNORM_PACK16:
                return VK_FORMAT_R16G16B16_UNORM;
            case gli::FORMAT_RGB16_SNORM_PACK16:
                return VK_FORMAT_R16G16B16_SNORM;
            case gli::FORMAT_RGB16_USCALED_PACK16:
                return VK_FORMAT_R16G16B16_USCALED;
            case gli::FORMAT_RGB16_SSCALED_PACK16:
                return VK_FORMAT_R16G16B16_SSCALED;
            case gli::FORMAT_RGB16_UINT_PACK16:
                return VK_FORMAT_R16G16B16_UINT;
            case gli::FORMAT_RGB16_SINT_PACK16:
                return VK_FORMAT_R16G16B16_SINT;
            case gli::FORMAT_RGB16_SFLOAT_PACK16:
                return VK_FORMAT_R16G16B16_SFLOAT;
            case gli::FORMAT_RGBA16_UNORM_PACK16:
                return VK_FORMAT_R16G16B16A16_UNORM;
            case gli::FORMAT_RGBA16_SNORM_PACK16:
                return VK_FORMAT_R16G16B16_SNORM;
            case gli::FORMAT_RGBA16_USCALED_PACK16:
                return VK_FORMAT_R16G16B16A16_USCALED;
            case gli::FORMAT_RGBA16_SSCALED_PACK16:
                return VK_FORMAT_R16G16B16A16_SSCALED;
            case gli::FORMAT_RGBA16_UINT_PACK16:
                return VK_FORMAT_R16G16B16A16_UINT;
            case gli::FORMAT_RGBA16_SINT_PACK16:
                return VK_FORMAT_R16G16B16A16_SINT;
            case gli::FORMAT_RGBA16_SFLOAT_PACK16:
                return VK_FORMAT_R16G16B16A16_SFLOAT;
            case gli::FORMAT_R32_UINT_PACK32:
                return VK_FORMAT_R32_UINT;
            case gli::FORMAT_R32_SINT_PACK32:
                return VK_FORMAT_R32_SINT;
            case gli::FORMAT_R32_SFLOAT_PACK32:
                return VK_FORMAT_R32_SFLOAT;
            case gli::FORMAT_RG32_UINT_PACK32:
                return VK_FORMAT_R32G32_UINT;
            case gli::FORMAT_RG32_SINT_PACK32:
                return VK_FORMAT_R32G32_SINT;
            case gli::FORMAT_RG32_SFLOAT_PACK32:
                return VK_FORMAT_R32G32_SFLOAT;
            case gli::FORMAT_RGB32_UINT_PACK32:
                return VK_FORMAT_R32G32B32_UINT;
            case gli::FORMAT_RGB32_SINT_PACK32:
                return VK_FORMAT_R32G32B32_SINT;
            case gli::FORMAT_RGB32_SFLOAT_PACK32:
                return VK_FORMAT_R32G32B32_SFLOAT;
            case gli::FORMAT_RGBA32_UINT_PACK32:
                return VK_FORMAT_R32G32B32A32_UINT;
            case gli::FORMAT_RGBA32_SINT_PACK32:
                return VK_FORMAT_R32G32B32A32_SINT;
            case gli::FORMAT_RGBA32_SFLOAT_PACK32:
                return VK_FORMAT_R32G32B32A32_SFLOAT;
            case gli::FORMAT_R64_UINT_PACK64:
                return VK_FORMAT_R64_UINT;
            case gli::FORMAT_R64_SINT_PACK64:
                return VK_FORMAT_R64_SINT;
            case gli::FORMAT_R64_SFLOAT_PACK64:
                return VK_FORMAT_R64_SFLOAT;
            case gli::FORMAT_RG64_UINT_PACK64:
                return VK_FORMAT_R64G64_UINT;
            case gli::FORMAT_RG64_SINT_PACK64:
                return VK_FORMAT_R64G64_SINT;
            case gli::FORMAT_RG64_SFLOAT_PACK64:
                return VK_FORMAT_R64G64_SFLOAT;
            case gli::FORMAT_RGB64_UINT_PACK64:
                return VK_FORMAT_R64G64B64_UINT;
            case gli::FORMAT_RGB64_SINT_PACK64:
                return VK_FORMAT_R64G64B64_SINT;
            case gli::FORMAT_RGB64_SFLOAT_PACK64:
                return VK_FORMAT_R64G64B64_SFLOAT;
            case gli::FORMAT_RGBA64_UINT_PACK64:
                return VK_FORMAT_R64G64B64A64_UINT;
            case gli::FORMAT_RGBA64_SINT_PACK64:
                return VK_FORMAT_R64G64B64A64_SINT;
            case gli::FORMAT_RGBA64_SFLOAT_PACK64:
                return VK_FORMAT_R64G64B64A64_SFLOAT;
            case gli::FORMAT_RG11B10_UFLOAT_PACK32:
                throw std::runtime_error("Unsupported format FORMAT_RG11B10_UFLOAT_PACK32");
            case gli::FORMAT_RGB9E5_UFLOAT_PACK32:
                throw std::runtime_error("Unsupported format FORMAT_RGB9E5_UFLOAT_PACK32");
            case gli::FORMAT_D16_UNORM_PACK16:
                return VK_FORMAT_D16_UNORM;
            case gli::FORMAT_D24_UNORM_PACK32:
                throw std::runtime_error("Unsupported format FORMAT_D24_UNORM_PACK32");
            case gli::FORMAT_D32_SFLOAT_PACK32:
                return VK_FORMAT_D32_SFLOAT;
            case gli::FORMAT_S8_UINT_PACK8:
                return VK_FORMAT_S8_UINT;
            case gli::FORMAT_D16_UNORM_S8_UINT_PACK32:
                return VK_FORMAT_D16_UNORM_S8_UINT;
            case gli::FORMAT_D24_UNORM_S8_UINT_PACK32:
                return VK_FORMAT_D24_UNORM_S8_UINT;
            case gli::FORMAT_D32_SFLOAT_S8_UINT_PACK64:
                return VK_FORMAT_D32_SFLOAT_S8_UINT;
            case gli::FORMAT_RGB_DXT1_UNORM_BLOCK8:
                return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
            case gli::FORMAT_RGB_DXT1_SRGB_BLOCK8:
                return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
            case gli::FORMAT_RGBA_DXT1_UNORM_BLOCK8:
                return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
            case gli::FORMAT_RGBA_DXT1_SRGB_BLOCK8:
                return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
            case gli::FORMAT_RGBA_DXT3_UNORM_BLOCK16:
                return VK_FORMAT_BC2_UNORM_BLOCK;
            case gli::FORMAT_RGBA_DXT3_SRGB_BLOCK16:
                return VK_FORMAT_BC2_SRGB_BLOCK;
            case gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16:
                return VK_FORMAT_BC3_UNORM_BLOCK;
            case gli::FORMAT_RGBA_DXT5_SRGB_BLOCK16:
                return VK_FORMAT_BC3_SRGB_BLOCK;
            case gli::FORMAT_R_ATI1N_UNORM_BLOCK8:
                return VK_FORMAT_BC4_UNORM_BLOCK;
            case gli::FORMAT_R_ATI1N_SNORM_BLOCK8:
                throw std::runtime_error("Unsupported format FORMAT_R_ATI1N_UNORM_BLOCK8");
            case gli::FORMAT_RG_ATI2N_UNORM_BLOCK16:
                return VK_FORMAT_BC5_UNORM_BLOCK;
            case gli::FORMAT_RG_ATI2N_SNORM_BLOCK16:
                throw std::runtime_error("Unsupported format FORMAT_RG_ATI2N_SNORM_BLOCK16");
            case gli::FORMAT_RGB_BP_UFLOAT_BLOCK16:
                throw std::runtime_error("Unsupported format FORMAT_RGB_BP_UFLOAT_BLOCK16");
            case gli::FORMAT_RGB_BP_SFLOAT_BLOCK16:
                throw std::runtime_error("Unsupported format FORMAT_RGB_BP_SFLOAT_BLOCK16");
            case gli::FORMAT_RGBA_BP_UNORM_BLOCK16:
                return VK_FORMAT_BC7_UNORM_BLOCK;
            case gli::FORMAT_RGBA_BP_SRGB_BLOCK16:
                throw std::runtime_error("Unsupported format FORMAT_RGBA_BP_SRGB_BLOCK16");
            case gli::FORMAT_RGB_ETC2_UNORM_BLOCK8:
                return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
            case gli::FORMAT_RGB_ETC2_SRGB_BLOCK8:
                return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
            case gli::FORMAT_RGBA_ETC2_UNORM_BLOCK8:
                return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
            case gli::FORMAT_RGBA_ETC2_SRGB_BLOCK8:
                return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
            case gli::FORMAT_RGBA_ETC2_UNORM_BLOCK16:
                throw std::runtime_error("Unsupported format FORMAT_RGBA_ETC2_UNORM_BLOCK16");
            case gli::FORMAT_RGBA_ETC2_SRGB_BLOCK16:
                throw std::runtime_error("Unsupported format FORMAT_RGBA_ETC2_SRGB_BLOCK16");
            case gli::FORMAT_R_EAC_UNORM_BLOCK8:
                return VK_FORMAT_EAC_R11_UNORM_BLOCK;
            case gli::FORMAT_R_EAC_SNORM_BLOCK8:
                return VK_FORMAT_EAC_R11_SNORM_BLOCK;
            case gli::FORMAT_RG_EAC_UNORM_BLOCK16:
                return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
            case gli::FORMAT_RG_EAC_SNORM_BLOCK16:
                return VK_FORMAT_EAC_R11G11_SNORM_BLOCK;
            case gli::FORMAT_RGBA_ASTC_4X4_UNORM_BLOCK16:
                return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
            case gli::FORMAT_RGBA_ASTC_4X4_SRGB_BLOCK16:
                return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
            case gli::FORMAT_RGBA_ASTC_5X4_UNORM_BLOCK16:
                return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
            case gli::FORMAT_RGBA_ASTC_5X4_SRGB_BLOCK16:
                return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
            case gli::FORMAT_RGBA_ASTC_5X5_UNORM_BLOCK16:
                return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
            case gli::FORMAT_RGBA_ASTC_5X5_SRGB_BLOCK16:
                return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
            case gli::FORMAT_RGBA_ASTC_6X5_UNORM_BLOCK16:
                return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
            case gli::FORMAT_RGBA_ASTC_6X5_SRGB_BLOCK16:
                return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
            case gli::FORMAT_RGBA_ASTC_6X6_UNORM_BLOCK16:
                return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
            case gli::FORMAT_RGBA_ASTC_6X6_SRGB_BLOCK16:
                return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
            case gli::FORMAT_RGBA_ASTC_8X5_UNORM_BLOCK16:
                return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
            case gli::FORMAT_RGBA_ASTC_8X5_SRGB_BLOCK16:
                return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
            case gli::FORMAT_RGBA_ASTC_8X6_UNORM_BLOCK16:
                return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
            case gli::FORMAT_RGBA_ASTC_8X6_SRGB_BLOCK16:
                return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
            case gli::FORMAT_RGBA_ASTC_8X8_UNORM_BLOCK16:
                return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
            case gli::FORMAT_RGBA_ASTC_8X8_SRGB_BLOCK16:
                return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
            case gli::FORMAT_RGBA_ASTC_10X5_UNORM_BLOCK16:
                return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
            case gli::FORMAT_RGBA_ASTC_10X5_SRGB_BLOCK16:
                return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
            case gli::FORMAT_RGBA_ASTC_10X6_UNORM_BLOCK16:
                return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
            case gli::FORMAT_RGBA_ASTC_10X6_SRGB_BLOCK16:
                return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
            case gli::FORMAT_RGBA_ASTC_10X8_UNORM_BLOCK16:
                return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
            case gli::FORMAT_RGBA_ASTC_10X8_SRGB_BLOCK16:
                return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
            case gli::FORMAT_RGBA_ASTC_10X10_UNORM_BLOCK16:
                return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
            case gli::FORMAT_RGBA_ASTC_10X10_SRGB_BLOCK16:
                return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
            case gli::FORMAT_RGBA_ASTC_12X10_UNORM_BLOCK16:
                return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
            case gli::FORMAT_RGBA_ASTC_12X10_SRGB_BLOCK16:
                return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
            case gli::FORMAT_RGBA_ASTC_12X12_UNORM_BLOCK16:
                return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
            case gli::FORMAT_RGBA_ASTC_12X12_SRGB_BLOCK16:
                return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
            case gli::FORMAT_RGB_PVRTC1_8X8_UNORM_BLOCK32:
                return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
            case gli::FORMAT_RGB_PVRTC1_8X8_SRGB_BLOCK32:
                throw std::runtime_error("Unsupported format FORMAT_RGB_PVRTC1_8X8_SRGB_BLOCK32");
            case gli::FORMAT_RGB_PVRTC1_16X8_UNORM_BLOCK32:
                throw std::runtime_error("Unsupported format FORMAT_RGB_PVRTC1_16X8_UNORM_BLOCK32");
            case gli::FORMAT_RGB_PVRTC1_16X8_SRGB_BLOCK32:
                throw std::runtime_error("Unsupported format FORMAT_RGB_PVRTC1_16X8_SRGB_BLOCK32");
            case gli::FORMAT_RGBA_PVRTC1_8X8_UNORM_BLOCK32:
                throw std::runtime_error("Unsupported format FORMAT_RGBA_PVRTC1_8X8_UNORM_BLOCK32");
            case gli::FORMAT_RGBA_PVRTC1_8X8_SRGB_BLOCK32:
                throw std::runtime_error("Unsupported format FORMAT_RGBA_PVRTC1_8X8_SRGB_BLOCK32");
            case gli::FORMAT_RGBA_PVRTC1_16X8_UNORM_BLOCK32:
                throw std::runtime_error("Unsupported format FORMAT_RGBA_PVRTC1_16X8_UNORM_BLOCK32");
            case gli::FORMAT_RGBA_PVRTC1_16X8_SRGB_BLOCK32:
                throw std::runtime_error("Unsupported format FORMAT_RGBA_PVRTC1_16X8_SRGB_BLOCK32");
            case gli::FORMAT_RGBA_PVRTC2_4X4_UNORM_BLOCK8:
                throw std::runtime_error("Unsupported format FORMAT_RGBA_PVRTC2_4X4_UNORM_BLOCK8");
            case gli::FORMAT_RGBA_PVRTC2_4X4_SRGB_BLOCK8:
                throw std::runtime_error("Unsupported format FORMAT_RGBA_PVRTC2_4X4_SRGB_BLOCK8");
            case gli::FORMAT_RGBA_PVRTC2_8X4_UNORM_BLOCK8:
                throw std::runtime_error("Unsupported format FORMAT_RGBA_PVRTC2_8X4_UNORM_BLOCK8");
            case gli::FORMAT_RGBA_PVRTC2_8X4_SRGB_BLOCK8:
                throw std::runtime_error("Unsupported format FORMAT_RGBA_PVRTC2_8X4_SRGB_BLOCK8");
            case gli::FORMAT_RGB_ETC_UNORM_BLOCK8:
                throw std::runtime_error("Unsupported format FORMAT_RGB_ETC_UNORM_BLOCK8");
            case gli::FORMAT_RGB_ATC_UNORM_BLOCK8:
                throw std::runtime_error("Unsupported format FORMAT_RGB_ATC_UNORM_BLOCK8");
            case gli::FORMAT_RGBA_ATCA_UNORM_BLOCK16:
                throw std::runtime_error("Unsupported format FORMAT_RGBA_ATCA_UNORM_BLOCK16");
            case gli::FORMAT_RGBA_ATCI_UNORM_BLOCK16:
                throw std::runtime_error("Unsupported format FORMAT_RGBA_ATCI_UNORM_BLOCK16");
            case gli::FORMAT_L8_UNORM_PACK8:
                return VK_FORMAT_R8_UNORM;
            case gli::FORMAT_A8_UNORM_PACK8:
                return VK_FORMAT_R8_UNORM;
            case gli::FORMAT_LA8_UNORM_PACK8:
                return VK_FORMAT_R8G8_UNORM;
            case gli::FORMAT_L16_UNORM_PACK16:
                return VK_FORMAT_R16_UNORM;
            case gli::FORMAT_A16_UNORM_PACK16:
                return VK_FORMAT_R16_UNORM;
            case gli::FORMAT_LA16_UNORM_PACK16:
                return VK_FORMAT_R16G16_UNORM;
            case gli::FORMAT_BGR8_UNORM_PACK32:
                return VK_FORMAT_B8G8R8_UNORM;
            case gli::FORMAT_BGR8_SRGB_PACK32:
                return VK_FORMAT_B8G8R8_SRGB;
            case gli::FORMAT_RG3B2_UNORM_PACK8:
                throw std::runtime_error("Unsupported format FORMAT_RG3B2_UNORM_PACK8");
            default:
                throw std::runtime_error("Unknown format");
        }
    }

    vk::Format GliImage::convert_format(gli::format format)
    {
        return static_cast<vk::Format>(gli_convert_format(format));
    }
}
