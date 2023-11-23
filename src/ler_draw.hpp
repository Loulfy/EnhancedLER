//
// Created by loulfy on 03/09/2023.
//

#ifndef LER_DRAW_HPP
#define LER_DRAW_HPP

#include "ler_mesh.hpp"
#include "ler_cull.hpp"

#include <stack>
#include <algorithm>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace ler
{
    constexpr static char RT_BackBuffer[] = "backBuffer";

    struct SceneParam
    {
        uint32_t instanceCount = 0;
    };

    struct RenderParams
    {
        CameraParam camera;
        SceneParam scene;
    };

    //*********************************************
    //*********************************************

    enum RenderResType
    {
        RS_ReadOnlyBuffer = 0,
        RS_SampledTexture,
        RS_StorageBuffer,
        RS_StorageImage,
        RS_RenderTarget,
        RS_DepthWrite,
        RS_Raytracing,
    };

    enum RenderPassType
    {
        RP_Graphics,
        RP_Compute,
        RP_RayTracing
    };

    NLOHMANN_JSON_SERIALIZE_ENUM( RenderPassType, {
        {RP_RayTracing, "raytracing"},
        {RP_Graphics, "graphics"},
        {RP_Compute, "compute"},
    })

    struct BufferDesc
    {
        uint32_t byteSize = 0;
        vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eStorageBuffer;
    };

    struct TextureDesc
    {
        vk::Format format = vk::Format::eUndefined;
        vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eClear;
        std::array<float,4> clearColor = Color::White;
    };

    struct RenderDesc
    {
        std::string name;
        uint32_t handle = UINT32_MAX;
        RenderResType type = RS_ReadOnlyBuffer;
        uint32_t binding = UINT32_MAX;

        BufferDesc buffer;
        TextureDesc texture;
    };

    void to_json(json& j, const RenderDesc& r);
    void from_json(const json& j, RenderDesc& r);

    //*********************************************
    //*********************************************

    using RenderResource = std::variant<std::monostate,BufferPtr,TexturePtr,TexturePoolPtr,vk::AccelerationStructureKHR>;

    struct RenderPassDesc
    {
        std::string name;
        RenderPassType pass = RP_Graphics;
        std::vector<RenderDesc> resources;
    };

    struct RenderGraphInfo
    {
        std::string name;
        std::vector<RenderPassDesc> passes;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RenderPassDesc, name, pass, resources);
    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RenderGraphInfo, name, passes);

    struct RenderGraph;
    class RenderGraphPass
    {
    public:

        virtual ~RenderGraphPass() = default;
        virtual void create(const LerDevicePtr& device, RenderGraph& graph, std::span<RenderDesc> resources) = 0;
        virtual void resize(const LerDevicePtr& device, const vk::Extent2D& viewport) {};
        virtual void render(CommandPtr& cmd, const SceneBuffers& sb, RenderParams params) = 0;
        [[nodiscard]] virtual PipelinePtr getPipeline() const = 0;
        [[nodiscard]] virtual std::string getName() const = 0;
    };

    struct RenderGraphNode
    {
        size_t index = 0;
        std::string name;
        RenderPass rendering;
        RenderPassType type = RP_Graphics;
        std::vector<RenderDesc> bindings;
        RenderGraphPass* pass = nullptr;
        vk::DescriptorSet descriptor = nullptr;
        std::set<uint32_t> outputs;
        std::vector<RenderGraphNode*> edges;
    };

    class RenderGraph
    {
    public:

        ~RenderGraph();
        void parse(const fs::path& desc);
        void compile(const LerDevicePtr& device, const vk::Extent2D& viewport);
        void resize(const LerDevicePtr& device, const vk::Extent2D& viewport);
        void execute(CommandPtr& cmd, TexturePtr& backBuffer, const SceneBuffers& sb, const RenderParams& params);
        void onSceneChange(SceneBuffers* scene);

        void addResource(const std::string& name, const RenderResource& res);
        bool getResource(uint32_t handle, BufferPtr& buffer) const;
        bool getResource(const std::string& name, BufferPtr& buffer) const;

        template<typename T>
        void addPass(const std::string& name)
        {
            for(RenderGraphNode& node : m_nodes)
            {
                if(node.name == name)
                    node.pass = new T;
            }
        }

        template<typename T>
        void addPass()
        {
            RenderGraphPass* pass = new T;
            for(RenderGraphNode& node : m_nodes)
            {
                if(node.name == pass->getName())
                {
                    node.pass = pass;
                    return;
                }
            }

            // Not Found Node, Clean.
            delete pass;
        }

    private:

        RenderGraphInfo m_info;
        std::vector<RenderGraphNode> m_nodes;
        std::vector<RenderResource> m_resourceCache;
        std::unordered_map<std::string,uint32_t> m_resourceMap;
        std::vector<RenderGraphNode*> m_sortedNodes;

        vk::UniqueSampler samplerGlobal;

        void setRenderAttachment(const RenderDesc& desc, vk::RenderingAttachmentInfo& info);
        void applyBarrier(CommandPtr& cmd, const RenderDesc& desc, ResourceState state);
        void bindResource(const PipelinePtr& pipeline, const RenderDesc& desc, vk::DescriptorSet descriptor);
        void computeEdges(RenderGraphNode& node);
        void topologicalSort();

        static ResourceState guessState(const RenderDesc& res);
        static bool guessOutput(const RenderDesc& res);
    };

    class RenderSceneList
    {
    public:

        void allocate(const LerDevicePtr& device);
        void install(flecs::world& world, const LerDevicePtr& device);
        [[nodiscard]] const BufferPtr& getInstanceBuffers() const;
        [[nodiscard]] const BufferPtr& getCommandBuffers() const;
        [[nodiscard]] uint32_t getInstanceCount() const;
        [[nodiscard]] SceneBuffers& getSceneBuffers();
        [[nodiscard]] uint32_t getDrawCount() const;
        [[nodiscard]] uint32_t getLineCount() const;

        void sort(const CommandPtr& cmd, const CameraParam& camera, bool prePass);
        void draw(const CommandPtr& cmd);
        void drawAABB(const CommandPtr& cmd);

        void generate(const TexturePtr& depth, const CommandPtr& cmd);
        void resize(const LerDevicePtr& device, vk::Extent2D extent);

    private:

        void addLine(const glm::vec3& p1, const glm::vec3& p2);
        void addAABB(const CMesh& mesh, const CTransform& transform);
        static std::array<glm::vec3, 8> createBox(const CMesh& mesh, const CTransform& transform);

        InstanceCull m_culling;
        std::vector<vk::BufferCopy> m_patches;
        std::vector<Instance> m_instances;
        std::vector<glm::vec3> m_lines;
        SceneBuffers m_sceneBuffers;
        BufferPtr m_instanceBuffer;
        BufferPtr m_aabbBuffer;
        BufferPtr m_staging;
    };

    class IRenderPass
    {
    public:

        virtual ~IRenderPass() = default;
        virtual void prepare(const LerDevicePtr& device) {}
        virtual void init(const LerDevicePtr& device, RenderSceneList& scene, GLFWwindow* window) {}
        virtual void onSceneChange(const LerDevicePtr& device, SceneBuffers* scene) {}
        virtual void resize(const LerDevicePtr& device, vk::Extent2D extent) {}
        virtual void render(const LerDevicePtr& device, FrameWindow& frame, RenderSceneList& sceneList, RenderParams& params) {}
        virtual void clean() {}
    };
}

#endif //LER_DRAW_HPP
