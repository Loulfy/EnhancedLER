//
// Created by loulfy on 07/07/2023.
//

#include "ler_dev.hpp"

#define SPIRV_REFLECT_HAS_VULKAN_H
#include <spirv_reflect.h>

namespace ler
{
    static const std::array<vk::Format, 5> c_depthFormats =
    {
        vk::Format::eD32SfloatS8Uint,
        vk::Format::eD32Sfloat,
        vk::Format::eD24UnormS8Uint,
        vk::Format::eD16UnormS8Uint,
        vk::Format::eD16Unorm
    };

    static const std::array<std::set<std::string>, 5> c_VertexAttrMap =
    {{
         {"inPos"},
         {"inTex", "inUV"},
         {"inNormal"},
         {"inTangent"},
         {"inColor"}
     }};

    LerDevice::LerDevice(const VulkanContext& context) : m_context(context)
    {
        m_queues[int(CommandQueue::Graphics)] = std::make_unique<Queue>(m_context, CommandQueue::Graphics,
                                                                        context.graphicsQueueFamily);
        m_queues[int(CommandQueue::Transfer)] = std::make_unique<Queue>(m_context, CommandQueue::Transfer,
                                                                        context.transferQueueFamily);

        m_texturePools.emplace_back(std::make_shared<TexturePool>());
        m_texturePools.back()->init(this);
    }

    LerDevice::~LerDevice()
    {

    }

    BufferPtr LerDevice::createBuffer(uint32_t byteSize, vk::BufferUsageFlags usages, bool staging)
    {
        auto buffer = std::make_shared<Buffer>(m_context);
        vk::BufferUsageFlags usageFlags = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst;
        usageFlags |= usages;
        usageFlags |= vk::BufferUsageFlagBits::eShaderDeviceAddress;
        usageFlags |= vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;
        buffer->info = vk::BufferCreateInfo();
        buffer->info.setSize(byteSize);
        buffer->info.setUsage(usageFlags);
        buffer->info.setSharingMode(vk::SharingMode::eExclusive);

        buffer->allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        buffer->allocInfo.flags = staging ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                            VMA_ALLOCATION_CREATE_MAPPED_BIT
                                          : VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        vmaCreateBuffer(m_context.allocator, reinterpret_cast<VkBufferCreateInfo*>(&buffer->info), &buffer->allocInfo,
                        reinterpret_cast<VkBuffer*>(&buffer->handle), &buffer->allocation, &buffer->hostInfo);

        return buffer;
    }

    void Buffer::uploadFromMemory(const void* src, uint32_t byteSize) const
    {
        if (isStaging() && length() >= byteSize)
            std::memcpy(hostInfo.pMappedData, src, byteSize);
        else
            log::error("Failed to upload to buffer");
    }

    void Buffer::getUint(uint32_t* ptr) const
    {
        void* data = hostInfo.pMappedData;
        auto t = (uint32_t*) data;
        *ptr = *t;
    }

    static vk::ImageUsageFlags pickImageUsage(vk::Format format, bool isRenderTarget)
    {
        vk::ImageUsageFlags ret = vk::ImageUsageFlagBits::eTransferSrc |
                                  vk::ImageUsageFlagBits::eTransferDst |
                                  vk::ImageUsageFlagBits::eSampled;

        if (isRenderTarget)
        {
            ret |= vk::ImageUsageFlagBits::eInputAttachment;
            switch (format)
            {
                case vk::Format::eS8Uint:
                case vk::Format::eD16Unorm:
                case vk::Format::eD32Sfloat:
                case vk::Format::eD16UnormS8Uint:
                case vk::Format::eD24UnormS8Uint:
                case vk::Format::eD32SfloatS8Uint:
                case vk::Format::eX8D24UnormPack32:
                    ret |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
                    break;

                default:
                    ret |= vk::ImageUsageFlagBits::eColorAttachment;
                    ret |= vk::ImageUsageFlagBits::eStorage;
                    break;
            }
        }
        return ret;
    }

    vk::ImageAspectFlags LerDevice::guessImageAspectFlags(vk::Format format, bool stencil)
    {
        switch (format)
        {
            case vk::Format::eD16Unorm:
            case vk::Format::eX8D24UnormPack32:
            case vk::Format::eD32Sfloat:
                return vk::ImageAspectFlagBits::eDepth;

            case vk::Format::eS8Uint:
                return vk::ImageAspectFlagBits::eStencil;

            case vk::Format::eD16UnormS8Uint:
            case vk::Format::eD24UnormS8Uint:
            case vk::Format::eD32SfloatS8Uint:
            {
                if(stencil)
                    return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
                else
                    return vk::ImageAspectFlagBits::eDepth;
            }

            default:
                return vk::ImageAspectFlagBits::eColor;
        }
    }

    TexturePoolPtr LerDevice::getTexturePool()
    {
        return m_texturePools.back();
    }

    TexturePtr LerDevice::getRenderTarget(RT target) const
    {
        return m_renderTargets[uint32_t(target)];
    }

    void LerDevice::setRenderTarget(RT target, TexturePtr texture)
    {
        m_renderTargets[uint32_t(target)] = texture;
    }

    vk::Viewport LerDevice::createViewport(const vk::Extent2D& extent)
    {
        //return {0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0, 1.0f};
        auto h = static_cast<float>(extent.height);
        auto w = static_cast<float>(extent.width);
        return {0, h, w, -h, 0, 1.0f};
    }

    void LerDevice::getFrustumPlanes(glm::mat4 mvp, glm::vec4* planes)
    {
        using glm::vec4;

        mvp = glm::transpose(mvp);
        planes[0] = vec4(mvp[3] + mvp[0]); // left
        planes[1] = vec4(mvp[3] - mvp[0]); // right
        planes[2] = vec4(mvp[3] + mvp[1]); // bottom
        planes[3] = vec4(mvp[3] - mvp[1]); // top
        planes[4] = vec4(mvp[3] + mvp[2]); // near
        planes[5] = vec4(mvp[3] - mvp[2]); // far
    }

    void LerDevice::getFrustumCorners(glm::mat4 mvp, glm::vec4* points)
    {
        using glm::vec4;

        const vec4 corners[] = {
                vec4(-1, -1, -1, 1), vec4(1, -1, -1, 1),
                vec4(1,  1, -1, 1),  vec4(-1,  1, -1, 1),
                vec4(-1, -1,  1, 1), vec4(1, -1,  1, 1),
                vec4(1,  1,  1, 1),  vec4(-1,  1,  1, 1)
        };

        const glm::mat4 invMVP = glm::inverse(mvp);

        for (int i = 0; i != 8; i++) {
            const vec4 q = invMVP * corners[i];
            points[i] = q / q.w;
        }
    }

    vk::Format LerDevice::chooseDepthFormat()
    {
        for (const vk::Format& format: c_depthFormats)
        {
            vk::FormatProperties depthFormatProperties = m_context.physicalDevice.getFormatProperties(format);
            // Format must support depth stencil attachment for optimal tiling
            if (depthFormatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
                return format;
        }
        return vk::Format::eD32Sfloat;
    }

    vk::ImageView Texture::view(vk::ImageSubresourceRange subresource)
    {
        if(m_views.contains(subresource))
          return m_views.at(subresource).get();

        subresource.setAspectMask(LerDevice::guessImageAspectFlags(info.format));

        vk::ImageViewCreateInfo createInfo;
        createInfo.setImage(handle);
        createInfo.setViewType(subresource.layerCount == 1 ? vk::ImageViewType::e2D : vk::ImageViewType::e2DArray);
        createInfo.setFormat(info.format);
        createInfo.setSubresourceRange(subresource);

        auto iter_pair = m_views.emplace(subresource, m_context.device.createImageViewUnique(createInfo));
        return std::get<0>(iter_pair)->second.get();
    }

    void LerDevice::populateTexture(const TexturePtr& texture, vk::Format format, const vk::Extent2D& extent,
                                    vk::SampleCountFlagBits sampleCount, bool isRenderTarget, uint32_t arrayLayers, uint32_t mipLevels)
    {
        texture->allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        texture->allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        texture->info = vk::ImageCreateInfo();
        texture->info.setImageType(vk::ImageType::e2D);
        texture->info.setExtent(vk::Extent3D(extent.width, extent.height, 1));
        texture->info.setMipLevels(mipLevels);
        texture->info.setArrayLayers(arrayLayers);
        texture->info.setFormat(format);
        texture->info.setInitialLayout(vk::ImageLayout::eUndefined);
        texture->info.setUsage(pickImageUsage(format, isRenderTarget));
        texture->info.setSharingMode(vk::SharingMode::eExclusive);
        texture->info.setSamples(sampleCount);
        texture->info.setFlags({});
        texture->info.setTiling(vk::ImageTiling::eOptimal);

        vmaCreateImage(m_context.allocator, reinterpret_cast<VkImageCreateInfo*>(&texture->info), &texture->allocInfo,
                       reinterpret_cast<VkImage*>(&texture->handle), &texture->allocation, nullptr);
    }

    TexturePtr
    LerDevice::createTexture(vk::Format format, const vk::Extent2D& extent, vk::SampleCountFlagBits sampleCount,
                             bool isRenderTarget, uint32_t arrayLayers, uint32_t mipLevels)
    {
        auto texture = std::make_shared<Texture>(m_context);
        populateTexture(texture, format, extent, sampleCount, isRenderTarget, arrayLayers, mipLevels);
        return texture;
    }

    TexturePtr LerDevice::createTextureFromNative(vk::Image image, vk::Format format, const vk::Extent2D& extent)
    {
        auto texture = std::make_shared<Texture>(m_context);

        texture->info = vk::ImageCreateInfo();
        texture->info.setImageType(vk::ImageType::e2D);
        texture->info.setExtent(vk::Extent3D(extent.width, extent.height, 1));
        texture->info.setSamples(vk::SampleCountFlagBits::e1);
        texture->info.setFormat(format);
        texture->info.setArrayLayers(1);

        texture->handle = image;
        texture->allocation = nullptr;

        return texture;
    }

    void RenderTarget::reset(const std::span<TexturePtr>& textures)
    {
        attachments.resize(textures.size());
        attachments.assign(textures.begin(), textures.end());
        clearValue(Color::White);
    }

    void RenderTarget::clearValue(const std::array<float, 4>& color)
    {
        clearValues.clear();
        for(const auto& attachment : attachments)
        {
            auto aspect = LerDevice::guessImageAspectFlags(attachment->info.format);
            if(aspect == vk::ImageAspectFlagBits::eColor)
                clearValues.emplace_back(vk::ClearColorValue(color));
            else
                clearValues.emplace_back(vk::ClearDepthStencilValue(1.0f, 0));
        }
    }

    PipelineRenderingAttachment RenderTarget::renderingAttachment() const
    {
        return attachments | std::views::transform([](auto& img){ return img->info.format; }) | std::ranges::to<std::vector>();
    }

    vk::UniqueSampler LerDevice::createSampler(const vk::SamplerAddressMode& addressMode, bool filter)
    {
        vk::SamplerCreateInfo samplerInfo;
        samplerInfo.setMagFilter(filter ? vk::Filter::eLinear : vk::Filter::eNearest);
        samplerInfo.setMinFilter(filter ? vk::Filter::eLinear : vk::Filter::eNearest);
        samplerInfo.setMipmapMode(filter ? vk::SamplerMipmapMode::eLinear : vk::SamplerMipmapMode::eNearest);
        samplerInfo.setAddressModeU(addressMode);
        samplerInfo.setAddressModeV(addressMode);
        samplerInfo.setAddressModeW(addressMode);
        samplerInfo.setMipLodBias(0.f);
        samplerInfo.setAnisotropyEnable(false);
        samplerInfo.setMaxAnisotropy(1.f);
        samplerInfo.setCompareEnable(false);
        samplerInfo.setCompareOp(vk::CompareOp::eLess);
        samplerInfo.setMinLod(0.f);
        samplerInfo.setMaxLod(std::numeric_limits<float>::max());
        samplerInfo.setBorderColor(vk::BorderColor::eFloatOpaqueBlack);

        return m_context.device.createSamplerUnique(samplerInfo, nullptr);
    }

    vk::UniqueSampler LerDevice::createSamplerMipMap(const vk::SamplerAddressMode& addressMode, bool filter, float maxLod, bool reduction)
    {
        vk::SamplerCreateInfo samplerInfo;
        samplerInfo.setMagFilter(filter ? vk::Filter::eLinear : vk::Filter::eNearest);
        samplerInfo.setMinFilter(filter ? vk::Filter::eLinear : vk::Filter::eNearest);
        samplerInfo.setMipmapMode(vk::SamplerMipmapMode::eNearest);
        samplerInfo.setAddressModeU(addressMode);
        samplerInfo.setAddressModeV(addressMode);
        samplerInfo.setAddressModeW(addressMode);
        samplerInfo.setMipLodBias(0.f);
        samplerInfo.setAnisotropyEnable(false);
        samplerInfo.setMaxAnisotropy(1.f);
        samplerInfo.setCompareEnable(false);
        samplerInfo.setCompareOp(vk::CompareOp::eLess);
        samplerInfo.setMinLod(0.f);
        samplerInfo.setMaxLod(maxLod);
        samplerInfo.setBorderColor(vk::BorderColor::eFloatOpaqueBlack);

        if(reduction)
        {
            vk::SamplerReductionModeCreateInfoEXT createInfoReduction;
            createInfoReduction.setReductionMode(vk::SamplerReductionMode::eMin);
            samplerInfo.setPNext(&createInfoReduction);
        }

        return m_context.device.createSamplerUnique(samplerInfo, nullptr);
    }

    uint32_t guessVertexInputBinding(const char* name)
    {
        for (size_t i = 0; i < c_VertexAttrMap.size(); ++i)
            if (c_VertexAttrMap[i].contains(name))
                return i;
        throw std::runtime_error("Vertex Input Attribute not reserved");
    }

    ShaderPtr LerDevice::createShader(const fs::path& path) const
    {
        auto shader = std::make_shared<Shader>();
        auto bytecode = FileSystemService::Get().readFile(FsTag_Cache, path);
        vk::ShaderModuleCreateInfo shaderInfo;
        shaderInfo.setCodeSize(bytecode.size());
        shaderInfo.setPCode(reinterpret_cast<const uint32_t*>(bytecode.data()));
        shader->shaderModule = m_context.device.createShaderModuleUnique(shaderInfo);

        uint32_t count = 0;
        SpvReflectShaderModule module;
        SpvReflectResult result = spvReflectCreateShaderModule(bytecode.size(), bytecode.data(), &module);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);
        assert(module.generator == SPV_REFLECT_GENERATOR_KHRONOS_GLSLANG_REFERENCE_FRONT_END);

        shader->stageFlagBits = static_cast<vk::ShaderStageFlagBits>(module.shader_stage);
        log::debug("======================================================");
        log::debug("Reflect Shader: {}, Stage: {}", path.stem().string(), vk::to_string(shader->stageFlagBits));

        // Input Variables
        result = spvReflectEnumerateInputVariables(&module, &count, nullptr);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        std::vector<SpvReflectInterfaceVariable*> inputs(count);
        result = spvReflectEnumerateInputVariables(&module, &count, inputs.data());
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        std::set<uint32_t> availableBinding;
        if (module.shader_stage == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT)
        {
            for (auto& in: inputs)
            {
                if (in->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN)
                    continue;

                uint32_t binding = guessVertexInputBinding(in->name);
                shader->attributeDesc.emplace_back(in->location, binding, static_cast<vk::Format>(in->format), 0);
                log::debug("location = {}, binding = {}, name = {}", in->location, binding, in->name);
                if (!availableBinding.contains(binding))
                {
                    shader->bindingDesc.emplace_back(binding, 0, vk::VertexInputRate::eVertex);
                    availableBinding.insert(binding);
                }
            }

            std::sort(shader->attributeDesc.begin(), shader->attributeDesc.end(),
                      [](const VkVertexInputAttributeDescription& a, const VkVertexInputAttributeDescription& b)
                      {
                          return a.location < b.location;
                      });

            // Compute final offsets of each attribute, and total vertex stride.
            for (size_t i = 0; i < shader->attributeDesc.size(); ++i)
            {
                uint32_t format_size = formatSize(static_cast<VkFormat>(shader->attributeDesc[i].format));
                shader->attributeDesc[i].offset = shader->bindingDesc[i].stride;
                shader->bindingDesc[i].stride += format_size;
            }
        }

        shader->pvi = vk::PipelineVertexInputStateCreateInfo();
        shader->pvi.setVertexAttributeDescriptions(shader->attributeDesc);
        shader->pvi.setVertexBindingDescriptions(shader->bindingDesc);

        // Push Constants
        result = spvReflectEnumeratePushConstantBlocks(&module, &count, nullptr);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        std::vector<SpvReflectBlockVariable*> constants(count);
        result = spvReflectEnumeratePushConstantBlocks(&module, &count, constants.data());
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        for (auto& block: constants)
            shader->pushConstants.emplace_back(shader->stageFlagBits, block->offset, block->size);

        // Descriptor Set
        result = spvReflectEnumerateDescriptorSets(&module, &count, nullptr);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        std::vector<SpvReflectDescriptorSet*> sets(count);
        result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        for (auto& set: sets)
        {
            DescriptorSetLayoutData desc;
            desc.set_number = set->set;
            desc.bindings.resize(set->binding_count);
            for (size_t i = 0; i < set->binding_count; ++i)
            {
                auto& binding = desc.bindings[i];
                binding.binding = set->bindings[i]->binding;
                binding.descriptorCount = set->bindings[i]->count;
                binding.descriptorType = static_cast<vk::DescriptorType>(set->bindings[i]->descriptor_type);
                binding.stageFlags = shader->stageFlagBits;
                log::debug("set = {}, binding = {}, count = {:02}, type = {}", set->set, binding.binding,
                           binding.descriptorCount, vk::to_string(binding.descriptorType));
            }
            shader->descriptorMap.insert({set->set, desc});
        }

        spvReflectDestroyShaderModule(&module);
        return shader;
    }

    void BasePipeline::reflectPipelineLayout(vk::Device device, const std::span<ShaderPtr>& shaders)
    {
        // PIPELINE LAYOUT STATE
        auto layoutInfo = vk::PipelineLayoutCreateInfo();
        std::vector<vk::PushConstantRange> pushConstants;
        for (auto& shader: shaders)
            pushConstants.insert(pushConstants.end(), shader->pushConstants.begin(), shader->pushConstants.end());
        layoutInfo.setPushConstantRanges(pushConstants);

        // SHADER REFLECT
        std::set<uint32_t> sets;
        std::vector<vk::DescriptorPoolSize> descriptorPoolSizeInfo;
        std::multimap<uint32_t, DescriptorSetLayoutData> mergedDesc;
        for (auto& shader: shaders)
            mergedDesc.merge(shader->descriptorMap);

        for (auto& e: mergedDesc)
            sets.insert(e.first);

        std::vector<vk::DescriptorSetLayout> setLayouts;
        setLayouts.reserve(sets.size());
        for (auto& set: sets)
        {
            descriptorPoolSizeInfo.clear();
            auto it = descriptorAllocMap.emplace(set, DescriptorAllocator());
            auto& allocator = std::get<0>(it)->second;

            auto descriptorPoolInfo = vk::DescriptorPoolCreateInfo();
            auto descriptorLayoutInfo = vk::DescriptorSetLayoutCreateInfo();
            auto range = mergedDesc.equal_range(set);
            for (auto e = range.first; e != range.second; ++e)
                allocator.layoutBinding.insert(allocator.layoutBinding.end(), e->second.bindings.begin(),
                                               e->second.bindings.end());
            descriptorLayoutInfo.setBindings(allocator.layoutBinding);

            vk::DescriptorSetLayoutBindingFlagsCreateInfo extended_info;
            vk::DescriptorBindingFlags bindless_flags = vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind;
            std::vector<vk::DescriptorBindingFlags> binding_flags(descriptorLayoutInfo.bindingCount, bindless_flags);
            extended_info.setBindingFlags(binding_flags);

            descriptorLayoutInfo.setFlags(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool);
            descriptorLayoutInfo.setPNext(&extended_info);
            for (auto& b: allocator.layoutBinding)
                descriptorPoolSizeInfo.emplace_back(b.descriptorType, b.descriptorCount + 2);
            descriptorPoolInfo.setPoolSizes(descriptorPoolSizeInfo);
            descriptorPoolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind);
            descriptorPoolInfo.setMaxSets(4);
            allocator.pool = device.createDescriptorPoolUnique(descriptorPoolInfo);
            allocator.layout = device.createDescriptorSetLayoutUnique(descriptorLayoutInfo);
            setLayouts.push_back(allocator.layout.get());
        }

        layoutInfo.setSetLayouts(setLayouts);
        pipelineLayout = device.createPipelineLayoutUnique(layoutInfo);
    }

    vk::DescriptorSet BasePipeline::createDescriptorSet(uint32_t set)
    {
        if (!descriptorAllocMap.contains(set))
            return {};

        vk::Result res;
        vk::DescriptorSet descriptorSet;
        const auto& allocator = descriptorAllocMap[set];
        vk::DescriptorSetAllocateInfo descriptorSetAllocInfo;
        descriptorSetAllocInfo.setDescriptorSetCount(1);
        descriptorSetAllocInfo.setDescriptorPool(allocator.pool.get());
        descriptorSetAllocInfo.setPSetLayouts(&allocator.layout.get());
        res = m_context.device.allocateDescriptorSets(&descriptorSetAllocInfo, &descriptorSet);
        assert(res == vk::Result::eSuccess);
        descriptorPoolMap.emplace(static_cast<VkDescriptorSet>(descriptorSet), set);
        return descriptorSet;
    }

    std::optional<vk::DescriptorType> BasePipeline::findBindingType(uint32_t set, uint32_t binding)
    {
        if(descriptorAllocMap.contains(set))
        {
            auto& descriptorDesc = descriptorAllocMap[set];
            for(const vk::DescriptorSetLayoutBinding& info : descriptorDesc.layoutBinding)
            {
                if(info.binding == binding)
                    return info.descriptorType;
            }
        }
        return std::nullopt;
    }

    void BasePipeline::updateSampler(vk::DescriptorSet descriptor, uint32_t binding, vk::Sampler sampler, vk::ImageLayout layout, vk::ImageView view)
    {
        auto d = static_cast<VkDescriptorSet>(descriptor);
        uint32_t set = descriptorPoolMap[d];
        auto type = findBindingType(set, binding);
        std::vector<vk::WriteDescriptorSet> descriptorWrites;
        std::vector<vk::DescriptorImageInfo> descriptorImageInfo;

        auto descriptorWriteInfo = vk::WriteDescriptorSet();
        descriptorWriteInfo.setDescriptorType(type.value());
        descriptorWriteInfo.setDstBinding(binding);
        descriptorWriteInfo.setDstSet(descriptor);
        descriptorWriteInfo.setDescriptorCount(1);

        auto& imageInfo = descriptorImageInfo.emplace_back();
        imageInfo = vk::DescriptorImageInfo();
        imageInfo.setImageLayout(layout);
        imageInfo.setSampler(sampler);
        imageInfo.setImageView(view);

        descriptorWriteInfo.setImageInfo(descriptorImageInfo);
        descriptorWrites.push_back(descriptorWriteInfo);
        m_context.device.updateDescriptorSets(descriptorWrites, nullptr);
    }

    void addShaderStage(std::vector<vk::PipelineShaderStageCreateInfo>& stages, const ShaderPtr& shader)
    {
        stages.emplace_back(
                vk::PipelineShaderStageCreateFlags(),
                shader->stageFlagBits,
                shader->shaderModule.get(),
                "main",
                nullptr
        );
    }

    PipelinePtr LerDevice::createGraphicsPipeline(const std::span<ShaderPtr>& shaders, const PipelineInfo& info)
    {
        vk::PipelineRenderingCreateInfo renderingCreateInfo;
        renderingCreateInfo.setColorAttachmentFormats(info.colorAttach);
        if(info.writeDepth)
            renderingCreateInfo.setDepthAttachmentFormat(info.depthAttach);

        auto pipeline = std::make_shared<GraphicsPipeline>(m_context);
        std::vector<vk::PipelineShaderStageCreateInfo> pipelineShaderStages;
        for (auto& shader: shaders)
            addShaderStage(pipelineShaderStages, shader);

        // TOPOLOGY STATE
        vk::PipelineInputAssemblyStateCreateInfo pia(vk::PipelineInputAssemblyStateCreateFlags(), info.topology);

        // VIEWPORT STATE
        auto viewport = vk::Viewport(0, 0, static_cast<float>(info.extent.width),
                                     static_cast<float>(info.extent.height), 0, 1.0f);
        auto renderArea = vk::Rect2D(vk::Offset2D(), info.extent);

        vk::PipelineViewportStateCreateInfo pv(vk::PipelineViewportStateCreateFlagBits(), 1, &viewport, 1, &renderArea);

        // Multi Sampling STATE
        vk::PipelineMultisampleStateCreateInfo pm(vk::PipelineMultisampleStateCreateFlags(), info.sampleCount);

        // POLYGON STATE
        vk::PipelineRasterizationStateCreateInfo pr;
        pr.setDepthClampEnable(VK_TRUE);
        pr.setRasterizerDiscardEnable(VK_FALSE);
        pr.setPolygonMode(info.polygonMode);
        pr.setFrontFace(vk::FrontFace::eCounterClockwise);
        pr.setDepthBiasEnable(VK_FALSE);
        pr.setDepthBiasConstantFactor(0.f);
        pr.setDepthBiasClamp(0.f);
        pr.setDepthBiasSlopeFactor(0.f);
        pr.setLineWidth(info.lineWidth);

        // DEPTH & STENCIL STATE
        vk::PipelineDepthStencilStateCreateInfo pds;
        pds.setDepthTestEnable(VK_TRUE);
        pds.setDepthWriteEnable(info.writeDepth);
        pds.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
        pds.setDepthBoundsTestEnable(VK_FALSE);
        pds.setStencilTestEnable(VK_FALSE);
        pds.setFront(vk::StencilOpState());
        pds.setBack(vk::StencilOpState());
        pds.setMinDepthBounds(0.f);
        pds.setMaxDepthBounds(1.f);

        // BLEND STATE
        std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments;
        vk::PipelineColorBlendAttachmentState pcb;
        pcb.setBlendEnable(VK_TRUE); // false
        pcb.setSrcColorBlendFactor(vk::BlendFactor::eOne); //one //srcAlpha
        pcb.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha); //one //oneminussrcalpha
        pcb.setColorBlendOp(vk::BlendOp::eAdd);
        pcb.setSrcAlphaBlendFactor(vk::BlendFactor::eOne); //one //oneminussrcalpha
        pcb.setDstAlphaBlendFactor(vk::BlendFactor::eZero); //zero
        pcb.setAlphaBlendOp(vk::BlendOp::eAdd);
        pcb.setColorWriteMask(
                vk::ColorComponentFlagBits::eR |
                vk::ColorComponentFlagBits::eG |
                vk::ColorComponentFlagBits::eB |
                vk::ColorComponentFlagBits::eA);

        for (auto& attachment: info.colorAttach)
        {
            if (guessImageAspectFlags(attachment) == vk::ImageAspectFlagBits::eColor)
                colorBlendAttachments.push_back(pcb);
        }

        vk::PipelineColorBlendStateCreateInfo pbs;
        pbs.setLogicOpEnable(VK_FALSE);
        pbs.setLogicOp(vk::LogicOp::eClear);
        pbs.setAttachments(colorBlendAttachments);

        // DYNAMIC STATE
        std::vector<vk::DynamicState> dynamicStates =
                {
                        vk::DynamicState::eViewport,
                        vk::DynamicState::eScissor
                };

        vk::PipelineDynamicStateCreateInfo pdy(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

        // PIPELINE LAYOUT STATE
        auto layoutInfo = vk::PipelineLayoutCreateInfo();
        std::vector<vk::PushConstantRange> pushConstants;
        for (auto& shader: shaders)
            pushConstants.insert(pushConstants.end(), shader->pushConstants.begin(), shader->pushConstants.end());
        layoutInfo.setPushConstantRanges(pushConstants);

        // SHADER REFLECT
        vk::PipelineVertexInputStateCreateInfo pvi;
        for (auto& shader: shaders)
        {
            if (shader->stageFlagBits == vk::ShaderStageFlagBits::eVertex)
                pvi = shader->pvi;
            if (shader->stageFlagBits == vk::ShaderStageFlagBits::eFragment)
            {
                for (auto& e: shader->descriptorMap)
                {
                    for (auto& bind: e.second.bindings)
                        if (bind.descriptorCount == 0 &&
                            bind.descriptorType == vk::DescriptorType::eCombinedImageSampler)
                            bind.descriptorCount = info.textureCount;
                }
            }
        }

        pipeline->reflectPipelineLayout(m_context.device, shaders);

        auto pipelineInfo = vk::GraphicsPipelineCreateInfo();
        pipelineInfo.setPNext(&renderingCreateInfo);
        pipelineInfo.setRenderPass(nullptr);
        pipelineInfo.setLayout(pipeline->pipelineLayout.get());
        pipelineInfo.setStages(pipelineShaderStages);
        pipelineInfo.setPVertexInputState(&pvi);
        pipelineInfo.setPInputAssemblyState(&pia);
        pipelineInfo.setPViewportState(&pv);
        pipelineInfo.setPRasterizationState(&pr);
        pipelineInfo.setPMultisampleState(&pm);
        pipelineInfo.setPDepthStencilState(&pds);
        pipelineInfo.setPColorBlendState(&pbs);
        pipelineInfo.setPDynamicState(&pdy);

        auto res = m_context.device.createGraphicsPipelineUnique(m_context.pipelineCache, pipelineInfo);
        assert(res.result == vk::Result::eSuccess);
        pipeline->handle = std::move(res.value);
        return pipeline;
    }

    PipelinePtr LerDevice::createComputePipeline(const ShaderPtr& shader)
    {
        auto pipeline = std::make_shared<ComputePipeline>(m_context);
        std::vector<vk::PipelineShaderStageCreateInfo> pipelineShaderStages;
        addShaderStage(pipelineShaderStages, shader);

        std::vector<ShaderPtr> shaders = {shader};
        pipeline->reflectPipelineLayout(m_context.device, shaders);

        auto pipelineInfo = vk::ComputePipelineCreateInfo();
        pipelineInfo.setStage(pipelineShaderStages.front());
        pipelineInfo.setLayout(pipeline->pipelineLayout.get());

        auto res = m_context.device.createComputePipelineUnique(m_context.pipelineCache, pipelineInfo);
        pipeline->bindPoint = vk::PipelineBindPoint::eCompute;
        assert(res.result == vk::Result::eSuccess);
        pipeline->handle = std::move(res.value);
        return pipeline;
    }

    void LerDevice::updateSampler(vk::DescriptorSet descriptorSet, uint32_t binding, vk::Sampler& sampler, const std::span<TexturePtr>& textures, vk::DescriptorType type)
    {
        std::vector<vk::WriteDescriptorSet> descriptorWrites;
        std::vector<vk::DescriptorImageInfo> descriptorImageInfo;

        auto descriptorWriteInfo = vk::WriteDescriptorSet();
        descriptorWriteInfo.setDescriptorType(type);
        descriptorWriteInfo.setDstBinding(binding);
        descriptorWriteInfo.setDstSet(descriptorSet);
        descriptorWriteInfo.setDescriptorCount(textures.size());

        for(auto& tex : textures)
        {
            auto& imageInfo = descriptorImageInfo.emplace_back();
            imageInfo = vk::DescriptorImageInfo();
            imageInfo.setSampler(sampler);
            if(tex && guessImageAspectFlags(tex->info.format) == vk::ImageAspectFlagBits::eColor)
                imageInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            else
                imageInfo.setImageLayout(vk::ImageLayout::eDepthReadOnlyOptimal);
            if(type == vk::DescriptorType::eStorageImage)
                imageInfo.setImageLayout(vk::ImageLayout::eGeneral);
            if(tex)
                imageInfo.setImageView(tex->view());
        }

        descriptorWriteInfo.setImageInfo(descriptorImageInfo);
        descriptorWrites.push_back(descriptorWriteInfo);
        m_context.device.updateDescriptorSets(descriptorWrites, nullptr);
    }

    void LerDevice::updateSampler(vk::DescriptorSet descriptorSet, uint32_t binding, vk::Sampler& sampler, TexturePtr& texture, vk::DescriptorType type)
    {
        updateSampler(descriptorSet, binding, sampler, std::span(&texture, 1), type);
    }

    void LerDevice::updateSampler(vk::DescriptorSet descriptorSet, uint32_t binding, vk::Sampler& sampler, vk::ImageLayout layout, const std::span<vk::ImageView>& views)
    {
        std::vector<vk::WriteDescriptorSet> descriptorWrites;
        std::vector<vk::DescriptorImageInfo> descriptorImageInfo;

        auto descriptorWriteInfo = vk::WriteDescriptorSet();
        descriptorWriteInfo.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        descriptorWriteInfo.setDstBinding(binding);
        descriptorWriteInfo.setDstSet(descriptorSet);
        descriptorWriteInfo.setDescriptorCount(views.size());

        for(auto& v : views)
        {
            auto& imageInfo = descriptorImageInfo.emplace_back();
            imageInfo = vk::DescriptorImageInfo();
            imageInfo.setSampler(sampler);
            imageInfo.setImageLayout(layout);
            if(v)
                imageInfo.setImageView(v);
        }

        descriptorWriteInfo.setImageInfo(descriptorImageInfo);
        descriptorWrites.push_back(descriptorWriteInfo);
        m_context.device.updateDescriptorSets(descriptorWrites, nullptr);
    }

    void LerDevice::updateSampler(vk::DescriptorSet descriptorSet, uint32_t binding, vk::Sampler& sampler, vk::ImageView view, vk::ImageLayout layout, vk::DescriptorType type)
    {
        std::vector<vk::WriteDescriptorSet> descriptorWrites;
        std::vector<vk::DescriptorImageInfo> descriptorImageInfo;

        auto descriptorWriteInfo = vk::WriteDescriptorSet();
        descriptorWriteInfo.setDescriptorType(type);
        descriptorWriteInfo.setDstBinding(binding);
        descriptorWriteInfo.setDstSet(descriptorSet);
        descriptorWriteInfo.setDescriptorCount(1);

        auto& imageInfo = descriptorImageInfo.emplace_back();
        imageInfo = vk::DescriptorImageInfo();
        imageInfo.setImageLayout(layout);
        imageInfo.setSampler(sampler);
        imageInfo.setImageView(view);

        descriptorWriteInfo.setImageInfo(descriptorImageInfo);
        descriptorWrites.push_back(descriptorWriteInfo);
        m_context.device.updateDescriptorSets(descriptorWrites, nullptr);
    }

    void LerDevice::updateStorage(vk::DescriptorSet descriptorSet, uint32_t binding, const BufferPtr& buffer, uint64_t byteSize, bool uniform)
    {
        std::vector<vk::WriteDescriptorSet> descriptorWrites;

        auto descriptorWriteInfo = vk::WriteDescriptorSet();
        descriptorWriteInfo.setDescriptorType(uniform ? vk::DescriptorType::eUniformBuffer : vk::DescriptorType::eStorageBuffer);
        descriptorWriteInfo.setDstBinding(binding);
        descriptorWriteInfo.setDstSet(descriptorSet);
        descriptorWriteInfo.setDescriptorCount(1);

        vk::DescriptorBufferInfo buffInfo(buffer->handle, 0, byteSize);

        descriptorWriteInfo.setBufferInfo(buffInfo);
        descriptorWrites.push_back(descriptorWriteInfo);
        m_context.device.updateDescriptorSets(descriptorWrites, nullptr);
    }

    Queue::Queue(const VulkanContext& context, CommandQueue queueID, uint32_t queueFamilyIndex)
            : m_context(context), m_queueKind(queueID), m_queueFamilyIndex(queueFamilyIndex)
    {
        auto semaphoreTypeInfo = vk::SemaphoreTypeCreateInfo()
                .setSemaphoreType(vk::SemaphoreType::eTimeline);

        auto semaphoreInfo = vk::SemaphoreCreateInfo()
                .setPNext(&semaphoreTypeInfo);

        trackingSemaphore = context.device.createSemaphore(semaphoreInfo);
        m_queue = m_context.device.getQueue(queueFamilyIndex, 0);
    }

    Queue::~Queue()
    {
        m_context.device.destroySemaphore(trackingSemaphore);
        trackingSemaphore = vk::Semaphore();
    }

    CommandPtr Queue::createCommandBuffer()
    {
        vk::Result res;

        CommandPtr ret = std::make_shared<TrackedCommandBuffer>(m_context);
        ret->queueKind = m_queueKind;

        auto cmdPoolInfo = vk::CommandPoolCreateInfo();
        cmdPoolInfo.setQueueFamilyIndex(m_queueFamilyIndex);
        cmdPoolInfo.setFlags(
                vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient);

        res = m_context.device.createCommandPool(&cmdPoolInfo, nullptr, &ret->cmdPool);

        // allocate command buffer
        auto allocInfo = vk::CommandBufferAllocateInfo();
        allocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
        allocInfo.setCommandPool(ret->cmdPool);
        allocInfo.setCommandBufferCount(1);

        res = m_context.device.allocateCommandBuffers(&allocInfo, &ret->cmdBuf);

        return ret;
    }

    CommandPtr Queue::getOrCreateCommandBuffer()
    {
        std::lock_guard lock(m_mutex);

        CommandPtr cmdBuf;
        if (m_commandBuffersPool.empty())
        {
            cmdBuf = createCommandBuffer();
        } else
        {
            cmdBuf = m_commandBuffersPool.back();
            m_commandBuffersPool.pop_back();
        }

        cmdBuf->cmdBuf.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        return cmdBuf;
    }

    void Queue::addWaitSemaphore(vk::Semaphore semaphore, uint64_t value)
    {
        if (!semaphore)
            return;

        m_waitSemaphores.push_back(semaphore);
        m_waitSemaphoreValues.push_back(value);
    }

    void Queue::addSignalSemaphore(vk::Semaphore semaphore, uint64_t value)
    {
        if (!semaphore)
            return;

        m_signalSemaphores.push_back(semaphore);
        m_signalSemaphoreValues.push_back(value);
    }

    uint64_t Queue::updateLastFinishedID()
    {
        m_lastFinishedID = m_context.device.getSemaphoreCounterValue(trackingSemaphore);
        return m_lastFinishedID;
    }

    void Queue::retireCommandBuffers(LerDevice& device)
    {
        std::lock_guard lock(m_mutex);
        std::vector<CommandPtr> submissions = std::move(m_commandBuffersInFlight);

        uint64_t lastFinishedID = updateLastFinishedID();

        for (const CommandPtr& cmd: submissions)
        {
            if (cmd->submissionID <= lastFinishedID)
            {
                if(m_queueKind == CommandQueue::Transfer)
                    Event::GetDispatcher().enqueue<CommandCompleteEvent>(cmd->submissionID, m_queueKind);
                cmd->submissionID = 0;
                cmd->referencedResources.clear();
                m_commandBuffersPool.push_back(cmd);
            } else
            {
                m_commandBuffersInFlight.push_back(cmd);
            }
        }
    }

    bool Queue::pollCommandList(uint64_t submissionID)
    {
        if (submissionID > m_lastSubmittedID || submissionID == 0)
            return false;

        bool completed = m_lastFinishedID >= submissionID;
        if (completed)
            return true;

        completed = updateLastFinishedID() >= submissionID;
        return completed;
    }

    bool Queue::waitCommandList(uint64_t submissionID, uint64_t timeout)
    {
        if (submissionID > m_lastSubmittedID || submissionID == 0)
            return false;

        if (pollCommandList(submissionID))
            return true;

        std::array<const vk::Semaphore, 1> semaphores = {trackingSemaphore};
        std::array<uint64_t, 1> waitValues = {submissionID};

        auto waitInfo = vk::SemaphoreWaitInfo()
                .setSemaphores(semaphores)
                .setValues(waitValues);

        vk::Result result = m_context.device.waitSemaphores(waitInfo, timeout);

        return (result == vk::Result::eSuccess);
    }

    uint64_t Queue::submit(std::vector<CommandPtr>& ppCmd)
    {
        std::vector<vk::PipelineStageFlags> waitStageArray(m_waitSemaphores.size());
        std::vector<vk::CommandBuffer> commandBuffers(ppCmd.size());

        for (size_t i = 0; i < m_waitSemaphores.size(); i++)
        {
            waitStageArray[i] = vk::PipelineStageFlagBits::eTopOfPipe;
        }

        m_lastSubmittedID++;

        for (size_t i = 0; i < ppCmd.size(); i++)
        {
            CommandPtr commandBuffer = ppCmd[i];

            // It's time!
            commandBuffer->cmdBuf.end();

            commandBuffers[i] = commandBuffer->cmdBuf;
            commandBuffer->submissionID = m_lastSubmittedID;
            m_commandBuffersInFlight.push_back(commandBuffer);
        }

        m_signalSemaphores.push_back(trackingSemaphore);
        m_signalSemaphoreValues.push_back(m_lastSubmittedID);

        auto timelineSemaphoreInfo = vk::TimelineSemaphoreSubmitInfo()
                .setSignalSemaphoreValueCount(uint32_t(m_signalSemaphoreValues.size()))
                .setPSignalSemaphoreValues(m_signalSemaphoreValues.data());

        if (!m_waitSemaphoreValues.empty())
        {
            timelineSemaphoreInfo.setWaitSemaphoreValueCount(uint32_t(m_waitSemaphoreValues.size()));
            timelineSemaphoreInfo.setPWaitSemaphoreValues(m_waitSemaphoreValues.data());
        }

        auto submitInfo = vk::SubmitInfo()
                .setPNext(&timelineSemaphoreInfo)
                .setCommandBufferCount(uint32_t(ppCmd.size()))
                .setPCommandBuffers(commandBuffers.data())
                .setWaitSemaphoreCount(uint32_t(m_waitSemaphores.size()))
                .setPWaitSemaphores(m_waitSemaphores.data())
                .setPWaitDstStageMask(waitStageArray.data())
                .setSignalSemaphoreCount(uint32_t(m_signalSemaphores.size()))
                .setPSignalSemaphores(m_signalSemaphores.data());

        m_queue.submit(submitInfo);

        m_waitSemaphores.clear();
        m_waitSemaphoreValues.clear();
        m_signalSemaphores.clear();
        m_signalSemaphoreValues.clear();

        return m_lastSubmittedID;
    }

    void LerDevice::queueWaitForSemaphore(CommandQueue waitQueueID, vk::Semaphore semaphore, uint64_t value)
    {
        Queue& waitQueue = *m_queues[uint32_t(waitQueueID)];
        waitQueue.addWaitSemaphore(semaphore, value);
    }

    void LerDevice::queueSignalSemaphore(CommandQueue executionQueueID, vk::Semaphore semaphore, uint64_t value)
    {
        Queue& executionQueue = *m_queues[uint32_t(executionQueueID)];
        executionQueue.addSignalSemaphore(semaphore, value);
    }

    bool LerDevice::pollCommand(uint64_t submissionId, CommandQueue kind)
    {
        return m_queues[uint32_t(kind)]->pollCommandList(submissionId);
    }

    void LerDevice::queueSubmitSignal(CommandQueue executionQueueID)
    {
        CommandPtr trigger = createCommand(executionQueueID);
        submitCommand(trigger);
    }

    CommandPtr LerDevice::createCommand(CommandQueue kind)
    {
        return m_queues[int(kind)]->getOrCreateCommandBuffer();
    }

    uint64_t LerDevice::submitCommand(CommandPtr& cmd)
    {
        auto ppCmd = std::vector<CommandPtr>{cmd};
        return m_queues[int(cmd->queueKind)]->submit(ppCmd);
    }

    void LerDevice::submitAndWait(CommandPtr& cmd)
    {
        auto ppCmd = std::vector<CommandPtr>{cmd};
        Queue* queue = m_queues[int(cmd->queueKind)].get();
        queue->submit(ppCmd);
        queue->waitCommandList(cmd->submissionID, UINT64_MAX);
    }

    void LerDevice::submitOneshot(CommandPtr& cmd)
    {
        cmd->cmdBuf.end();
        vk::UniqueFence fence = m_context.device.createFenceUnique({});

        vk::SubmitInfo submitInfo;
        submitInfo.setCommandBuffers(cmd->cmdBuf);
        m_queues[0]->m_queue.submit(submitInfo, fence.get());

        auto res = m_context.device.waitForFences(fence.get(), true, std::numeric_limits<uint64_t>::max());
        assert(res == vk::Result::eSuccess);

        m_queues[0]->m_commandBuffersPool.push_back(cmd);
    }

    void LerDevice::runGarbageCollection()
    {
        for (auto& queue: m_queues)
        {
            if (queue)
            {
                queue->retireCommandBuffers(*this);
            }
        }
    }
}