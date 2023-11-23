//
// Created by loulfy on 25/07/2023.
//

#include "ler_csm.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace ler
{
    void CascadedShadowPass::init(const LerDevicePtr& device, RenderSceneList& scene, GLFWwindow* window)
    {
        vk::Extent2D extent(4096, 4096);
        vk::Format depthFormat = vk::Format::eD32Sfloat;
        m_shadowMap = device->createTexture(depthFormat, extent, vk::SampleCountFlagBits::e1, true, SHADOW_MAP_CASCADE_COUNT);

        device->setRenderTarget(RT::eShadowMap, m_shadowMap);

        std::array<ler::ShaderPtr, 1> shaders;
        shaders[0] = device->createShader("shadow.vert.spv");

        ler::PipelineInfo info;
        info.writeDepth = true;
        info.depthAttach = depthFormat;
        info.polygonMode = vk::PolygonMode::eFill;
        info.topology = vk::PrimitiveTopology::eTriangleList;
        m_pipeline = device->createGraphicsPipeline(shaders, info);
        m_descriptor = m_pipeline->createDescriptorSet(0);
    }

    void CascadedShadowPass::onSceneChange(const LerDevicePtr& device, SceneBuffers* scene)
    {
        //device->updateStorage(m_descriptor, 0, batch.instanceBuffer, VK_WHOLE_SIZE);
    }

    void CascadedShadowPass::render(const LerDevicePtr& device, FrameWindow& frame, RenderSceneList& sceneList, RenderParams& params)
    {
        /*
        update(param, scene);

        vk::RenderingAttachmentInfo depthAttachment;
        depthAttachment.setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
        depthAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
        depthAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
        depthAttachment.setClearValue(vk::ClearDepthStencilValue(1.0f, 0));

        vk::Extent2D extent(4096, 4096);
        CommandPtr cmd = device->createCommand();
        vk::RenderingInfo renderInfo;
        vk::Viewport viewport = LerDevice::createViewport(extent);
        vk::Rect2D renderArea(vk::Offset2D(), extent);
        renderInfo.setRenderArea(renderArea);
        renderInfo.setLayerCount(1);
        renderInfo.setPDepthAttachment(&depthAttachment);

        cmd->addBarrier(m_shadowMap, vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eEarlyFragmentTests);

        for(size_t nSlice = 0; nSlice < m_cascades.size(); ++nSlice)
        {
            Cascade& cascade = m_cascades[nSlice];
            vk::ImageSubresourceRange subresource(vk::ImageAspectFlagBits::eDepth, 0, 1, nSlice, 1);
            depthAttachment.setImageView(m_shadowMap->view(subresource));

            cmd->cmdBuf.beginRendering(renderInfo);
            cmd->cmdBuf.setScissor(0, 1, &renderArea);
            cmd->cmdBuf.setViewport(0, 1, &viewport);
            cmd->bindPipeline(m_pipeline, m_descriptor);
            constexpr static vk::DeviceSize offset = 0;
            cmd->cmdBuf.bindIndexBuffer(batch.indexBuffer->handle, offset, vk::IndexType::eUint32);
            cmd->cmdBuf.bindVertexBuffers(0, 1, &batch.vertexBuffer->handle, &offset);

            glm::mat4 viewProj = cascade.viewProjMatrix;
            cmd->cmdBuf.pushConstants(m_pipeline->pipelineLayout.get(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &viewProj);

            auto objects = batch.renderList.instances();
            for(size_t i = 0; i < objects.size(); ++i)
            {
                auto const& mesh = batch.meshes[objects[i]];
                cmd->cmdBuf.drawIndexed(mesh->countIndex, 1, mesh->firstIndex, mesh->firstVertex, i);
            }

            cmd->cmdBuf.endRendering();
        }

        cmd->addBarrier(m_shadowMap, vk::PipelineStageFlagBits::eLateFragmentTests, vk::PipelineStageFlagBits::eFragmentShader);
        device->submitCommand(cmd);*/
    }

    void CascadedShadowPass::update(const CameraParam& param, SceneParamDep& scene)
    {
        /*float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

        //float nearClip = camera->getNearClip();
        //float farClip = camera->getFarClip();
        float nearClip = 0.1f;
        float farClip = 1000.f;
        float clipRange = farClip - nearClip;

        float minZ = nearClip;
        float maxZ = nearClip + clipRange;

        float range = maxZ - minZ;
        float ratio = maxZ / minZ;

        // Calculate split depths based on view camera frustum
        // Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
        for (int32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
        {
            float p = (i + 1.f) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
            float log = minZ * std::pow(ratio, p);
            float uniform = minZ + range * p;
            float d = cascadeSplitLambda * (log - uniform) + uniform;
            cascadeSplits[i] = (d - nearClip) / clipRange;
        }

        // Calculate orthographic projection matrix for each cascade
        float lastSplitDist = 0.0;
        for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++)
        {
            float splitDist = cascadeSplits[i];
            std::array<glm::vec3, 8> frustumCorners = {
                glm::vec3(-1.0f, 1.0f, 0.0f),
                glm::vec3(1.0f, 1.0f, 0.0f),
                glm::vec3(1.0f, -1.0f, 0.0f),
                glm::vec3(-1.0f, -1.0f, 0.0f),
                glm::vec3(-1.0f, 1.0f, 1.0f),
                glm::vec3(1.0f, 1.0f, 1.0f),
                glm::vec3(1.0f, -1.0f, 1.0f),
                glm::vec3(-1.0f, -1.0f, 1.0f),
            };

            // Project frustum corners into world space
            glm::mat4 invCam = glm::inverse(param.proj * param.view);
            for (glm::vec3& p : frustumCorners)
            {
                glm::vec4 invCorner = invCam * glm::vec4(p, 1.0f);
                p = invCorner / invCorner.w;
            }

            for (uint32_t j = 0; j < 4; j++)
            {
                glm::vec3 dist = frustumCorners[j + 4] - frustumCorners[j];
                frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
                frustumCorners[j] = frustumCorners[j] + (dist * lastSplitDist);
            }

            // Get frustum center
            glm::vec3 frustumCenter(0.0f);
            for (const glm::vec3& p : frustumCorners)
                frustumCenter += p;
            frustumCenter /= 8.0f;

            // Get frustum radius
            float radius = 0.0f;
            for (const glm::vec3& p : frustumCorners)
            {
                float distance = glm::length(p - frustumCenter);
                radius = glm::max(radius, distance);
            }
            radius = std::ceil(radius * 16.0f) / 16.0f;

            glm::vec3 maxExtents(radius);
            glm::vec3 minExtents = -maxExtents;

            glm::vec3 lightDir = normalize(-glm::vec3(0, 20, 0.3));
            glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter,
                                                    glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.f,
                                                    maxExtents.z - minExtents.z);


            auto t = maxExtents.z - minExtents.z;
            //lightOrthoMatrix[1][1] *= -1;
            // Store split distance and matrix in cascade
            m_cascades[i].splitDepth = (nearClip + splitDist * clipRange) * -1.0f;
            m_cascades[i].viewProjMatrix = lightOrthoMatrix * lightViewMatrix;

            lastSplitDist = cascadeSplits[i];

            scene.light[i] =  m_cascades[i].viewProjMatrix;
            scene.split[i] = m_cascades[i].splitDepth;
        }*/

        float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

        float nearClip = 0.1f;
        float farClip = 48.f;
        float clipRange = farClip - nearClip;

        float minZ = nearClip;
        float maxZ = nearClip + clipRange;

        float range = maxZ - minZ;
        float ratio = maxZ / minZ;

        // Calculate split depths based on view camera frustum
        // Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
        for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
            float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
            float log = minZ * std::pow(ratio, p);
            float uniform = minZ + range * p;
            float d = cascadeSplitLambda * (log - uniform) + uniform;
            cascadeSplits[i] = (d - nearClip) / clipRange;
        }

        // Calculate orthographic projection matrix for each cascade
        float lastSplitDist = 0.0;
        for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
            float splitDist = cascadeSplits[i];

            glm::vec3 frustumCorners[8] = {
                    glm::vec3(-1.0f,  1.0f, 0.0f),
                    glm::vec3( 1.0f,  1.0f, 0.0f),
                    glm::vec3( 1.0f, -1.0f, 0.0f),
                    glm::vec3(-1.0f, -1.0f, 0.0f),
                    glm::vec3(-1.0f,  1.0f,  1.0f),
                    glm::vec3( 1.0f,  1.0f,  1.0f),
                    glm::vec3( 1.0f, -1.0f,  1.0f),
                    glm::vec3(-1.0f, -1.0f,  1.0f),
            };

            // Project frustum corners into world space
            glm::mat4 invCam = glm::inverse(param.proj * param.view);
            for (uint32_t i = 0; i < 8; i++) {
                glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i], 1.0f);
                frustumCorners[i] = invCorner / invCorner.w;
            }

            for (uint32_t i = 0; i < 4; i++) {
                glm::vec3 dist = frustumCorners[i + 4] - frustumCorners[i];
                frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
                frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
            }

            // Get frustum center
            glm::vec3 frustumCenter = glm::vec3(0.0f);
            for (uint32_t i = 0; i < 8; i++) {
                frustumCenter += frustumCorners[i];
            }
            frustumCenter /= 8.0f;

            float radius = 0.0f;
            for (uint32_t i = 0; i < 8; i++) {
                float distance = glm::length(frustumCorners[i] - frustumCenter);
                radius = glm::max(radius, distance);
            }
            radius = std::ceil(radius * 16.0f) / 16.0f;

            glm::vec3 maxExtents = glm::vec3(radius);
            glm::vec3 minExtents = -maxExtents;

            glm::vec3 lightPos = glm::vec3(0, 10, 0.3);
            glm::vec3 lightDir = normalize(-lightPos);
            glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

            // Store split distance and matrix in cascade
            m_cascades[i].splitDepth = (nearClip + splitDist * clipRange) * -1.0f;
            m_cascades[i].viewProjMatrix = lightOrthoMatrix * lightViewMatrix;

            lastSplitDist = cascadeSplits[i];

            scene.light[i] =  m_cascades[i].viewProjMatrix;
            scene.split[int(i)] = m_cascades[i].splitDepth;
        }
    }
}