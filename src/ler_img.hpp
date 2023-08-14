//
// Created by loulfy on 14/07/2023.
//

#ifndef LER_IMG_HPP
#define LER_IMG_HPP

#include <gli/gli.hpp>

#include "ler_sys.hpp"
#include "ler_vki.hpp"

namespace ler
{
    class IImage
    {
    public:

        virtual ~IImage() = default;
        [[nodiscard]] virtual vk::Extent2D extent() const = 0;
        [[nodiscard]] virtual unsigned char* data() const = 0;
        [[nodiscard]] virtual vk::Format format() const = 0;
        [[nodiscard]] virtual size_t byteSize() const = 0;
    };

    class StbImage : public IImage
    {
    private:

        int width = 0;
        int height = 0;
        int level = 0;
        unsigned char* image = nullptr;

    public:

        StbImage() = default;
        StbImage(const StbImage&) = delete;
        explicit StbImage(const std::vector<char>& blob);
        explicit StbImage(const fs::path& path);
        ~StbImage() override;

        [[nodiscard]] vk::Extent2D extent() const override { return {uint32_t(width), uint32_t(height)}; }
        [[nodiscard]] unsigned char* data() const override { return image; }
        [[nodiscard]] vk::Format format() const override { return vk::Format::eR8G8B8A8Unorm; }
        [[nodiscard]] size_t byteSize() const override { return width * height * 4; }
    };

    class GliImage : public IImage
    {
    private:

        gli::texture image;

    public:

        explicit GliImage(const std::vector<char>& blob);
        explicit GliImage(const fs::path& path);

        [[nodiscard]] vk::Extent2D extent() const override { return {uint32_t(image.extent().x), uint32_t(image.extent().y)}; }
        [[nodiscard]] unsigned char* data() const override { return (unsigned char *) image.data(); }
        [[nodiscard]] size_t byteSize() const override { return image.size(); }
        [[nodiscard]] vk::Format format() const override;

        static vk::Format convert_format(gli::format format);
    };

    using ImagePtr = std::shared_ptr<IImage>;

    struct ImageLoader
    {
        static ImagePtr load(const FileSystemPtr& fs, const fs::path& path);
        static ImagePtr load(const fs::path& path);
        static bool support(const fs::path& path);
    };
}

#endif //LER_IMG_HPP
