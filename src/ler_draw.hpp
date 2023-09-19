//
// Created by loulfy on 03/09/2023.
//

#ifndef LER_DRAW_HPP
#define LER_DRAW_HPP

#include "ler_mesh.hpp"
#include "ler_cull.hpp"

namespace ler
{
    struct RenderParams
    {
        CameraParam camera;
        SceneParam scene;
    };

    class RenderSceneList
    {
    public:

        void allocate(const LerDevicePtr& device);
        void install(flecs::world& world, const LerDevicePtr& device);
        [[nodiscard]] const BufferPtr& getInstanceBuffers() const;
        [[nodiscard]] const BufferPtr& getCommandBuffers() const;
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
