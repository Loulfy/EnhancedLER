//
// Created by loulfy on 06/07/2023.
//

#ifndef LER_DEV_H
#define LER_DEV_H

#include "ler_vki.hpp"
#include "ler_sys.hpp"

#include <glm/glm.hpp>
#include <functional>

struct GLFWwindow;

namespace ler
{
    // Boost hash_combine
    template<class T>
    void hash_combine(size_t& seed, const T& v)
    {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
}

namespace std
{
    template<> struct hash<vk::ImageSubresourceRange>
    {
        std::size_t operator()(vk::ImageSubresourceRange const& s) const noexcept
        {
            size_t hash = 0;
            ler::hash_combine(hash, s.baseMipLevel);
            ler::hash_combine(hash, s.levelCount);
            ler::hash_combine(hash, s.baseArrayLayer);
            ler::hash_combine(hash, s.layerCount);
            return hash;
        }
    };
}

namespace ler
{
    struct Color
    {
        static constexpr std::array<float, 4> Black = {0.f, 0.f, 0.f, 1.f};
        static constexpr std::array<float, 4> White = {1.f, 1.f, 1.f, 1.f};
        static constexpr std::array<float, 4> Gray = {0.45f, 0.55f, 0.6f, 1.f};
        static constexpr std::array<float, 4> Magenta = {1.f, 0.f, 1.f, 1.f};
    };

    struct CameraParam
    {
        alignas(16) glm::mat4 proj = glm::mat4(1.f);
        alignas(16) glm::mat4 view = glm::mat4(1.f);
    };

    struct SceneParam
    {
        alignas(16) glm::vec4 split;
        glm::mat4 light[4] = {glm::mat4(1.f) };
    };

    class IResource
    {
    protected:
        IResource() = default;
        virtual ~IResource() = default;

    public:
        // Non-copyable and non-movable
        IResource(const IResource&) = delete;
        IResource(const IResource&&) = delete;
        IResource& operator=(const IResource&) = delete;
        IResource& operator=(const IResource&&) = delete;
    };

    struct Buffer : public IResource
    {
        vk::Buffer handle;
        vk::BufferCreateInfo info;
        VmaAllocation allocation = nullptr;
        VmaAllocationCreateInfo allocInfo = {};
        VmaAllocationInfo hostInfo = {};

        ~Buffer() override { vmaDestroyBuffer(m_context.allocator, static_cast<VkBuffer>(handle), allocation); }
        explicit Buffer(const VulkanContext& context) : m_context(context) { }
        [[nodiscard]] uint32_t length() const { return info.size; }
        [[nodiscard]] bool isStaging() const { return allocInfo.flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT; }
        void uploadFromMemory(const void* src, uint32_t byteSize) const;
        void getUint(uint32_t* ptr) const;

    private:

        const VulkanContext& m_context;
    };

    using BufferPtr = std::shared_ptr<Buffer>;

    struct Texture : public IResource
    {
        vk::Image handle;
        std::string name;
        vk::ImageCreateInfo info;
        VmaAllocation allocation = nullptr;
        VmaAllocationCreateInfo allocInfo = {};

        ~Texture() override { if(allocation) vmaDestroyImage(m_context.allocator, static_cast<VkImage>(handle), allocation); }
        explicit Texture(const VulkanContext& context) : m_context(context) { }
        [[nodiscard]] vk::ImageView view(vk::ImageSubresourceRange subresource = DefaultSub);

        static constexpr vk::ImageSubresourceRange DefaultSub = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);

    private:

        const VulkanContext& m_context;
        std::unordered_map<vk::ImageSubresourceRange, vk::UniqueImageView> m_views;
    };

    using TexturePtr = std::shared_ptr<Texture>;

    struct DescriptorSetLayoutData
    {
        uint32_t set_number = 0;
        VkDescriptorSetLayoutCreateInfo create_info;
        std::vector<vk::DescriptorSetLayoutBinding> bindings;
    };

    struct Shader
    {
        vk::UniqueShaderModule shaderModule;
        vk::ShaderStageFlagBits stageFlagBits = {};
        vk::PipelineVertexInputStateCreateInfo pvi;
        std::vector<vk::PushConstantRange> pushConstants;
        std::map<uint32_t, DescriptorSetLayoutData> descriptorMap;
        std::vector<vk::VertexInputBindingDescription> bindingDesc;
        std::vector<vk::VertexInputAttributeDescription> attributeDesc;
    };

    using ShaderPtr = std::shared_ptr<Shader>;

    struct DescriptorAllocator
    {
        std::vector<vk::DescriptorSetLayoutBinding> layoutBinding;
        vk::UniqueDescriptorSetLayout layout;
        vk::UniqueDescriptorPool pool;
    };

    using PipelineRenderingAttachment = std::vector<vk::Format>;

    struct Attachment
    {
        TexturePtr texture;
        vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eDontCare;
        vk::ClearValue clear = vk::ClearDepthStencilValue(1.0f, 0);
    };

    using Attachments = std::initializer_list<Attachment>;

    struct PassDesc
    {
        vk::Extent2D viewport;
        Attachments colorAttachments;
        Attachment depthStencilAttachment;
    };

    struct RenderTarget
    {
        vk::Extent2D extent;
        std::vector<TexturePtr> attachments;
        std::vector<vk::ClearValue> clearValues;

        void reset(const std::span<TexturePtr>& textures);
        void clearValue(const std::array<float, 4>& color = Color::White);
        [[nodiscard]] PipelineRenderingAttachment renderingAttachment() const;
    };

    struct FrameWindow
    {
        vk::Extent2D extent;
        vk::Format format;
        TexturePtr image;
    };

    struct PipelineInfo
    {
        vk::Extent2D extent;
        vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
        vk::PolygonMode polygonMode = vk::PolygonMode::eFill;
        vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;
        uint32_t textureCount = 0;
        bool writeDepth = true;
        float lineWidth = 1.f;
        PipelineRenderingAttachment colorAttach;
        vk::Format depthAttach;
    };

    class BasePipeline
    {
    public:

        explicit BasePipeline(const VulkanContext& context) : m_context(context) { }
        void reflectPipelineLayout(vk::Device device, const std::span<ShaderPtr>& shaders);
        vk::DescriptorSet createDescriptorSet(uint32_t set);

        std::optional<vk::DescriptorType> findBindingType(uint32_t set, uint32_t binding);
        void updateSampler(vk::DescriptorSet descriptor, uint32_t binding, vk::Sampler sampler, vk::ImageLayout layout, vk::ImageView view);

        vk::UniquePipeline handle;
        vk::UniquePipelineLayout pipelineLayout;
        vk::PipelineBindPoint bindPoint = vk::PipelineBindPoint::eGraphics;
        std::unordered_map<uint32_t,DescriptorAllocator> descriptorAllocMap;
        std::unordered_map<VkDescriptorSet, uint32_t> descriptorPoolMap;

    private:

        const VulkanContext& m_context;
    };

    using PipelinePtr = std::shared_ptr<BasePipeline>;

    class GraphicsPipeline : public BasePipeline
    {
    public:

        explicit GraphicsPipeline(const VulkanContext& context) : BasePipeline(context) { }
    };

    class ComputePipeline : public BasePipeline
    {
    public:

        explicit ComputePipeline(const VulkanContext& context) : BasePipeline(context) { }
    };

    enum class CommandQueue
    {
        Graphics,
        Compute,
        Transfer,
        Count
    };

    class TrackedCommandBuffer
    {
    public:
        // the command buffer itself
        vk::CommandBuffer cmdBuf = vk::CommandBuffer();
        vk::CommandPool cmdPool = vk::CommandPool();

        uint64_t submissionID = 0;
        CommandQueue queueKind = CommandQueue::Graphics;
        std::vector<std::shared_ptr<IResource>> referencedResources;

        ~TrackedCommandBuffer();
        explicit TrackedCommandBuffer(const VulkanContext& context) : m_context(context){ }

        void addBarrier(TexturePtr& texture, vk::PipelineStageFlagBits srcStage, vk::PipelineStageFlagBits dstStage);
        void copyBuffer(BufferPtr& src, BufferPtr& dst, uint64_t byteSize = VK_WHOLE_SIZE, uint64_t dstOffset = 0);
        void copyBuffer(BufferPtr& src, BufferPtr& dst, vk::BufferCopy copy);
        void copyBufferToTexture(const BufferPtr& buffer, const TexturePtr& texture);
        void bindPipeline(const PipelinePtr& pipeline, vk::DescriptorSet set = nullptr);
        void executePass(const PassDesc& desc);
        void draw(uint32_t vertexCount);
        void end();

    private:

        bool m_beginRendering = false;
        const VulkanContext& m_context;
    };

    using CommandPtr = std::shared_ptr<TrackedCommandBuffer>;

    class Queue
    {
    public:

        friend class LerDevice;
        Queue(const VulkanContext& context, CommandQueue queueID, uint32_t queueFamilyIndex);
        ~Queue();

        // creates a command buffer and its synchronization resources
        uint64_t updateLastFinishedID();
        CommandPtr getOrCreateCommandBuffer();

        void addWaitSemaphore(vk::Semaphore semaphore, uint64_t value);
        void addSignalSemaphore(vk::Semaphore semaphore, uint64_t value);

        //static std::string to_string(CommandQueue kind);
        [[nodiscard]] CommandQueue getQueueID() const { return m_queueKind; };
        [[nodiscard]] uint64_t getLastSubmittedID() const { return m_lastSubmittedID; };

        bool pollCommandList(uint64_t submissionID);
        bool waitCommandList(uint64_t submissionID, uint64_t timeout);

        // submission
        uint64_t submit(std::vector<CommandPtr>& ppCmd);

        vk::Semaphore trackingSemaphore;

        struct CommandCompleteEvent
        {
            uint64_t submissionId = 0;
            CommandQueue queueKind = CommandQueue::Graphics;
        };

    private:

        CommandPtr createCommandBuffer();
        void retireCommandBuffers(LerDevice& device);

        const VulkanContext& m_context;

        vk::Queue m_queue = vk::Queue();
        CommandQueue m_queueKind = CommandQueue::Graphics;
        uint32_t m_queueFamilyIndex = UINT32_MAX;

        std::mutex m_mutex;
        std::vector<vk::Semaphore> m_waitSemaphores;
        std::vector<uint64_t> m_waitSemaphoreValues;
        std::vector<vk::Semaphore> m_signalSemaphores;
        std::vector<uint64_t> m_signalSemaphoreValues;

        uint64_t m_lastSubmittedID = 0;
        uint64_t m_lastFinishedID = 0;

        std::vector<CommandPtr> m_commandBuffersInFlight;
        std::vector<CommandPtr> m_commandBuffersPool;
    };

    class TexturePool;
    using TexturePoolPtr = std::shared_ptr<TexturePool>;

    enum class RT
    {
        eDepth,
        eShadowMap,
        eDepthPyramid,
        eCount
    };

    class LerDevice
    {
    public:

        friend class Queue;
        virtual ~LerDevice();
        explicit LerDevice(const VulkanContext& context);
        [[nodiscard]] const VulkanContext& getVulkanContext() const { return m_context; }

        // Buffer
        BufferPtr createBuffer(uint32_t byteSize, vk::BufferUsageFlags usages = vk::BufferUsageFlagBits(), bool staging = false);

        // Texture
        TexturePtr createTexture(vk::Format format, const vk::Extent2D& extent, vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1, bool isRenderTarget = false, uint32_t arrayLayers = 1, uint32_t mipLevels = 1);
        TexturePtr createTextureFromNative(vk::Image image, vk::Format format, const vk::Extent2D& extent);
        vk::UniqueSampler createSampler(const vk::SamplerAddressMode& addressMode, bool filter);
        vk::UniqueSampler createSamplerMipMap(const vk::SamplerAddressMode& addressMode, bool filter, float maxLod, bool reduction = false);
        static vk::ImageAspectFlags guessImageAspectFlags(vk::Format format, bool stencil = true);
        [[nodiscard]] TexturePtr getRenderTarget(RT target) const;
        void setRenderTarget(RT target, TexturePtr texture);
        TexturePoolPtr getTexturePool();
        vk::Format chooseDepthFormat();

        // Pipeline
        [[nodiscard]] ShaderPtr createShader(const fs::path& path) const;
        PipelinePtr createGraphicsPipeline(const std::span<ShaderPtr>& shaders, const PipelineInfo& info);
        PipelinePtr createComputePipeline(const ShaderPtr& shader);

        // DescriptorSet
        void updateSampler(vk::DescriptorSet descriptorSet, uint32_t binding, vk::Sampler& sampler, const std::span<TexturePtr>& textures, vk::DescriptorType type = vk::DescriptorType::eCombinedImageSampler);
        void updateSampler(vk::DescriptorSet descriptorSet, uint32_t binding, vk::Sampler& sampler, TexturePtr& texture, vk::DescriptorType type);
        void updateSampler(vk::DescriptorSet descriptorSet, uint32_t binding, vk::Sampler& sampler, vk::ImageLayout layout, const std::span<vk::ImageView>& views);
        void updateSampler(vk::DescriptorSet descriptorSet, uint32_t binding, vk::Sampler& sampler, vk::ImageView view, vk::ImageLayout layout, vk::DescriptorType type);
        void updateStorage(vk::DescriptorSet descriptorSet, uint32_t binding, const BufferPtr& buffer, uint64_t byteSize, bool uniform = false);

        // Execution
        void queueSubmitSignal(CommandQueue executionQueueID);
        void queueWaitForSemaphore(CommandQueue waitQueueID, vk::Semaphore semaphore, uint64_t value);
        void queueSignalSemaphore(CommandQueue executionQueueID, vk::Semaphore semaphore, uint64_t value);
        bool pollCommand(uint64_t submissionId, CommandQueue kind = CommandQueue::Transfer);
        CommandPtr createCommand(CommandQueue kind = CommandQueue::Graphics);
        uint64_t submitCommand(CommandPtr& cmd);
        void submitAndWait(CommandPtr& cmd);
        void submitOneshot(CommandPtr& cmd);
        void runGarbageCollection();

        // Utils
        static vk::Viewport createViewport(const vk::Extent2D& extent);
        static void getFrustumPlanes(glm::mat4 mvp, glm::vec4* planes);
        static void getFrustumCorners(glm::mat4 mvp, glm::vec4* points);

    private:

        static uint32_t formatSize(VkFormat format);
        void populateTexture(const TexturePtr& texture, vk::Format format, const vk::Extent2D& extent, vk::SampleCountFlagBits sampleCount, bool isRenderTarget = false, uint32_t arrayLayers = 1, uint32_t mipLevels = 1);

        const VulkanContext& m_context;
        std::array<std::unique_ptr<Queue>, uint32_t(CommandQueue::Count)> m_queues;
        std::vector<TexturePoolPtr> m_texturePools;
        std::array<TexturePtr, uint32_t(RT::eCount)> m_renderTargets;
    };

    using LerDevicePtr = std::shared_ptr<LerDevice>;

    /*class IRenderPass
    {
    public:

        virtual ~IRenderPass() = default;
        virtual void init(LerDevicePtr& device, GLFWwindow* window) {}
        virtual void prepare(LerDevicePtr& device) {}
        virtual void render(LerDevicePtr& device, FrameWindow& frame, const ViewParam& param) = 0;
        virtual void clean() {}
    };*/

    class TexturePool
    {
    public:

        ~TexturePool();
        TexturePool();

        struct Resource
        {
            FsTag tag;
            uint32_t id;
            fs::path path;
        };

        static constexpr uint32_t Size = 300;
        using Future = std::function<void()>;
        using Require = std::bitset<Size>;
        uint32_t fetch(const fs::path& filename, FsTag tag);

        void receive(const Queue::CommandCompleteEvent& e);
        [[nodiscard]] std::span<TexturePtr> getTextures();
        [[nodiscard]] std::vector<vk::ImageView> getImageViews();
        [[nodiscard]] std::vector<std::string> getTextureList() const;
        [[nodiscard]] TexturePtr getTextureFromIndex(uint32_t index) const;
        [[nodiscard]] const std::unordered_multimap<std::string, uint32_t>& getTextureCacheRef() const;
        [[nodiscard]] bool verify(const Require& key) const { return (key & m_loadedTextures) == key; }

        void init(LerDevice* device) { m_device = device; }
        void set(uint32_t index, const TexturePtr& texture, uint64_t ticket);

    private:

        uint32_t allocate();

        LerDevice* m_device;
        Require m_loadedTextures;
        std::array<TexturePtr, Size> m_textures;
        std::atomic_uint32_t m_textureCount = 0;
        std::atomic_flag m_fence = ATOMIC_FLAG_INIT;
        std::multimap<uint64_t, uint32_t> m_submitted;
        std::unordered_multimap<std::string, uint32_t> m_cache;

        static void processImages(LerDevice* device, const Resource& res);
    };

    struct SubmitTexture
    {
        uint32_t id = UINT32_MAX;
        TexturePtr texture;
        CommandPtr command;
    };

    class BatchedMesh;
    struct SubmitTransfer
    {
        CommandPtr command;
        TexturePool::Require dependency;
        BatchedMesh* batch;
    };

    using AsyncRequest = std::variant<SubmitTexture, SubmitTransfer>;
}

#endif //LER_DEV_H
