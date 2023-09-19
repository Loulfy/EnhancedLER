//
// Created by loulfy on 14/07/2023.
//

#include "ler_mesh.hpp"

namespace ler
{
    bool AssimpFileSystem::exists(const fs::path& path) const
    {
        return aiScene->GetEmbeddedTexture(path.string().c_str()) != nullptr;
    }

    Blob AssimpFileSystem::readFile(const fs::path& path)
    {
        const aiTexture* em = aiScene->GetEmbeddedTexture(path.string().c_str());
        if (em == nullptr)
        {
            fs::path f = path.parent_path() / fs::path(path);
            return {};
        } else
        {
            const auto* buffer = reinterpret_cast<const unsigned char*>(em->pcData);
            return {buffer, buffer + em->mWidth};
        }
    }

    void AssimpFileSystem::enumerates(std::vector<fs::path>& entries)
    {
        for (size_t i = 0; i < aiScene->mNumTextures; ++i)
        {
            const aiTexture* texture = aiScene->mTextures[i];
            if (texture->mFilename.length > 0)
                entries.emplace_back(texture->mFilename.C_Str());
            else
                entries.emplace_back("*" + std::to_string(i));
        }
    }

    fs::file_time_type AssimpFileSystem::last_write_time(const fs::path& path)
    {
        return fs::last_write_time(path);
    }

    fs::path AssimpFileSystem::format_hint(const fs::path& path)
    {
        const aiTexture* em = aiScene->GetEmbeddedTexture(path.string().c_str());
        if (em)
        {
            std::string ext = ".";
            ext += em->achFormatHint;
            return ext;
        }
        return {};
    }

    glm::mat4 convert(aiMatrix4x4t<ai_real> from)
    {
        auto m = glm::make_mat4(from.Transpose()[0]);
        glm::quat rot = glm::quat_cast(m);
        return m;
    }

    physx::PxMat44 convert(const glm::mat4 from)
    {
        auto* glmData = const_cast<float*>(glm::value_ptr(from));
        return physx::PxMat44(glmData).getTranspose();
    }

    physx::PxMeshScale convertToPxScale(const aiMatrix4x4& aiMatrix)
    {
        aiVector3D scaling;
        aiVector3D position;
        aiQuaternion rotation;
        aiMatrix.Decompose(scaling, rotation, position);
        return {physx::PxVec3(scaling.x, scaling.y, scaling.z)};
    }

    void transformBoundingBox(const glm::mat4& t, glm::vec3& min, glm::vec3& max)
    {
        std::array<glm::vec3, 8> pts = {
                glm::vec3(min.x, min.y, min.z),
                glm::vec3(min.x, max.y, min.z),
                glm::vec3(min.x, min.y, max.z),
                glm::vec3(min.x, max.y, max.z),
                glm::vec3(max.x, min.y, min.z),
                glm::vec3(max.x, max.y, min.z),
                glm::vec3(max.x, min.y, max.z),
                glm::vec3(max.x, max.y, max.z),
        };

        for (auto& p: pts)
            p = glm::vec3(t * glm::vec4(p, 1.f));

        // create
        min = glm::vec3(std::numeric_limits<float>::max());
        max = glm::vec3(std::numeric_limits<float>::lowest());

        for (auto& p : pts)
        {
            min = glm::min(min, p);
            max = glm::max(max, p);
        }
    }

    TexturePool::TexturePool()
    {
        auto s = Event::GetDispatcher().sink<Queue::CommandCompleteEvent>();
        s.connect<&TexturePool::receive>(this);
    }

    TexturePool::~TexturePool()
    {
        auto s = Event::GetDispatcher().sink<Queue::CommandCompleteEvent>();
        s.disconnect<&TexturePool::receive>(this);
    }

    uint32_t TexturePool::fetch(const fs::path& filename, FsTag tag)
    {
        const fs::path ext = FileSystemService::Get(tag)->format_hint(filename);
        if(ImageLoader::support(ext))
        {
            uint32_t index = allocate();
            Resource res(tag, index, filename);
            Async::GetPool().push_task(processImages, m_device, std::move(res));
            return index;
        }

        return 0;
    }

    uint32_t TexturePool::allocate()
    {
        if(m_textureCount == Size)
            log::exit("TexturePool Size exceeded");
        return m_textureCount.fetch_add(1, std::memory_order_relaxed);
    }

    void TexturePool::receive(const Queue::CommandCompleteEvent& e)
    {
        auto range = m_submitted.equal_range(e.submissionId);
        for (auto i = range.first; i != range.second; ++i)
            m_loadedTextures.set(i->second);

        m_submitted.erase(e.submissionId);
    }

    std::span<TexturePtr> TexturePool::getTextures()
    {
        return {m_textures.data(), m_textureCount};
    }

    std::vector<vk::ImageView> TexturePool::getImageViews()
    {
        auto views = m_textures | std::views::take(int(m_textureCount)) | std::views::transform([](const TexturePtr& tex) { return tex->view(); });
        return {views.begin(), views.end()};
    }

    std::vector<std::string> TexturePool::getTextureList() const
    {
        auto kv = std::views::keys(m_cache);
        return {kv.begin(), kv.end()};
    }

    TexturePtr TexturePool::getTextureFromIndex(uint32_t index) const
    {
        if (m_textures.size() > index)
            return m_textures[index];
        return {};
    }

    const std::unordered_multimap<std::string, uint32_t>& TexturePool::getTextureCacheRef() const
    {
        return std::ref(m_cache);
    }

    void TexturePool::set(uint32_t index, const TexturePtr& texture, uint64_t ticket)
    {
        if (m_textures.size() > index && texture)
        {
            m_textures[index] = texture;
            m_cache.emplace(texture->name, index);
            m_submitted.emplace(ticket, index);
        }
    }

    void TexturePool::processImages(LerDevice* device, const Resource& res)
    {
        ImagePtr img = ImageLoader::load(FileSystemService::Get(res.tag), res.path);

        vk::Extent2D extent = img->extent();
        size_t imageSize = img->byteSize();
        TexturePtr texture = device->createTexture(img->format(), extent);
        BufferPtr staging = device->createBuffer(imageSize, vk::BufferUsageFlagBits(), true);
        staging->uploadFromMemory(img->data(), imageSize);
        texture->name = res.path.string();

        CommandPtr cmd = device->createCommand(CommandQueue::Transfer);
        cmd->copyBufferToTexture(staging, texture);

        SubmitTexture submit;
        submit.id = res.id;
        submit.command = cmd;
        submit.texture = texture;
        AsyncQueue<AsyncRequest>::Commit(submit);
        log::debug("Submit async images: {}", res.path.string());
    }
}