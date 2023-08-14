//
// Created by loulfy on 14/07/2023.
//

#include "ler_img.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace ler
{
    static const std::set<fs::path> c_supportedImages =
    {
        ".png",
        ".jpg",
        ".jpeg",
        ".dds",
        ".ktx",
        ".tga"
    };

    static const std::set<fs::path> c_supportedModels =
    {
        ".obj",
        ".fbx",
        ".glb",
        ".gltf"
    };

    StbImage::StbImage(const std::vector<char>& blob)
    {
        auto buff = reinterpret_cast<const stbi_uc*>(blob.data());
        image = stbi_load_from_memory(buff, static_cast<int>(blob.size()), &width, &height, &level, STBI_rgb_alpha);
    }

    StbImage::StbImage(const fs::path& path)
    {
        image = stbi_load(path.string().c_str(), &width, &height, &level, STBI_rgb_alpha);
    }

    StbImage::~StbImage()
    {
        stbi_image_free(image);
    }

    GliImage::GliImage(const std::vector<char>& blob)
    {
        image = gli::load(blob.data(), blob.size());
        if(image.empty())
            log::error("GLI failed to load blob");
    }

    GliImage::GliImage(const fs::path& path)
    {
        image = gli::load(path.string().c_str());
    }

    vk::Format GliImage::format() const
    {
        try
        {
            return convert_format(image.format());
        }
        catch(const std::exception& e)
        {
            log::error(std::string("GLI: ") + e.what());
        }
        return vk::Format::eUndefined;
    }

    ImagePtr ImageLoader::load(const FileSystemPtr& fs, const fs::path& path)
    {
        const auto ext = fs->format_hint(path);
        if(c_supportedImages.contains(ext) && fs->exists(path))
        {
            const Blob& blob = fs->readFile(path);
            if(ext == ".dds" || ext == ".ktx")
                return std::make_shared<GliImage>(blob);
            else
                return std::make_shared<StbImage>(blob);
        }

        log::error("Format not supported: " + ext.string());
        return {};
    }

    ImagePtr ImageLoader::load(const fs::path& path)
    {
        const auto ext = path.extension();
        if(c_supportedImages.contains(ext))
        {
            if(ext == ".dds" || ext == ".ktx")
                return std::make_shared<GliImage>(path);
            else
                return std::make_shared<StbImage>(path);
        }

        log::error("Format not supported: " + ext.string());
        return {};
    }

    bool ImageLoader::support(const fs::path& ext)
    {
        return c_supportedImages.contains(ext);
    }
}