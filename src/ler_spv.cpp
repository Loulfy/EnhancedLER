//
// Created by loulfy on 14/07/2023.
//

#include "ler_spv.hpp"
#include "ler_log.hpp"

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <SPIRV/doc.h>

namespace ler
{
    struct KindMapping
    {
        fs::path ext;
        EShLanguage kind;
    };

    static const std::array<KindMapping, 4> c_KindMap = {{
        {".vert", EShLanguage::EShLangVertex},
        {".frag", EShLanguage::EShLangFragment},
        {".geom", EShLanguage::EShLangGeometry},
        {".comp", EShLanguage::EShLangCompute}
    }};

    std::optional<KindMapping> convertShaderExtension(const fs::path& ext)
    {
        for(auto& map : c_KindMap)
            if(map.ext == ext)
                return map;
        return {};
    }

    void DirStackFileIncluder::addInclude(const std::string& include)
    {
        IncludePack pack;
        pack.binary = FileSystemService::Get().readFile(FsTag_Assets, include);
        pack.result = std::make_unique<IncludeResult>(include, pack.binary.data(), pack.binary.size(), nullptr);
        m_includes.emplace(include, std::move(pack));
    }

    DirStackFileIncluder::IncludeResult* DirStackFileIncluder::includeSystem(const char* include, const char* shader, size_t size)
    {
        if(m_includes.contains(include))
            return m_includes[include].result.get();
        return nullptr;
    }

    DirStackFileIncluder::IncludeResult* DirStackFileIncluder::includeLocal(const char* include, const char* shader, size_t size)
    {
        if(m_includes.contains(include))
            return m_includes[include].result.get();
        return nullptr;
    }

    GlslangInitializer::GlslangInitializer()
    {
        glslang::InitializeProcess();
        Includer().addInclude("ler_shader.hpp");
    }

    GlslangInitializer::~GlslangInitializer()
    {
        glslang::FinalizeProcess();
    }

    DirStackFileIncluder& GlslangInitializer::Includer()
    {
        static DirStackFileIncluder includer;
        return std::ref(includer);
    }

    bool GlslangInitializer::compile(glslang::TShader* shader, const std::string& code, EShMessages controls, const std::string& shaderName, const std::string& entryPointName)
    {
        const char* shaderStrings = code.data();
        const int shaderLengths = static_cast<int>(code.size());
        const char* shaderNames = nullptr;

        if (controls & EShMsgDebugInfo)
        {
            shaderNames = shaderName.data();
            shader->setStringsWithLengthsAndNames(&shaderStrings, &shaderLengths, &shaderNames, 1);
        }
        else
        {
            shader->setStringsWithLengths(&shaderStrings, &shaderLengths, 1);
        }

        if (!entryPointName.empty())
            shader->setEntryPoint(entryPointName.c_str());

        return shader->parse(GetDefaultResources(), 460, false, controls, Includer());
    }

    std::vector<uint32_t> GlslangInitializer::compileGlslToSpv(const std::string& code, const fs::path& name)
    {
        auto stage = convertShaderExtension(name.extension());
        if(!stage.has_value())
            return {};

        bool success = true;
        glslang::TShader shader(stage.value().kind);
        EShMessages controls = EShMsgCascadingErrors;
        controls = static_cast<EShMessages>(controls | EShMsgDebugInfo);
        controls = static_cast<EShMessages>(controls | EShMsgSpvRules);
        controls = static_cast<EShMessages>(controls | EShMsgKeepUncalled);
        controls = static_cast<EShMessages>(controls | EShMsgVulkanRules | EShMsgSpvRules);
        shader.setEnvTarget(glslang::EShTargetLanguage::EShTargetSpv, glslang::EShTargetLanguageVersion::EShTargetSpv_1_6);
        success &= compile(&shader, code, controls, name.string());

        if(!success)
        {
            log::error(shader.getInfoLog());
            return {};
        }

        // Link all of them.
        glslang::TProgram program;
        program.addShader(&shader);
        success &= program.link(controls);

        if(!success)
        {
            log::error(program.getInfoLog());
            return {};
        }

        glslang::SpvOptions options;
        spv::SpvBuildLogger logger;
        std::vector<uint32_t> spv;
        //options.disableOptimizer = false;
        //options.optimizeSize = true;
        options.stripDebugInfo = false;
        options.emitNonSemanticShaderDebugInfo = true;
        options.emitNonSemanticShaderDebugSource = true;
        glslang::GlslangToSpv(*program.getIntermediate(shader.getStage()), spv, &logger, &options);
        if(!logger.getAllMessages().empty())
            log::error(logger.getAllMessages());
        return spv;
    }

    void GlslangInitializer::compileFile(const fs::path& input, const fs::path& output)
    {
        const auto blob = FileSystemService::Get().readFile(FsTag_Assets, input);
        std::string src(blob.begin(), blob.end());

        auto spv = compileGlslToSpv(src, input.filename().string());

        if(spv.empty())
            return;

        std::ofstream fout(output, std::ios_base::binary);
        auto size = static_cast<std::streamsize>(spv.size() * sizeof(uint32_t));
        fout.write(reinterpret_cast<const char*>(spv.data()), size);
        fout.close();
    }

    void GlslangInitializer::shaderAutoCompile()
    {
        fs::create_directory(CACHED_DIR);
        auto& fs = FileSystemService::Get();
        std::vector<fs::path> entries;
        fs.enumerates(FsTag_Assets, entries);
        for(const auto& entry : entries)
        {
            auto res = convertShaderExtension(entry.extension());
            if(!res.has_value())
                continue;

            fs::path f = CACHED_DIR / entry.filename();
            f.concat(".spv");
            if(fs::exists(f) && fs::last_write_time(f) > fs.last_write_time(FsTag_Assets, entry))
                continue;

            log::warn("Compile {}", f.make_preferred().string());
            compileFile(entry, f);
        }
    }
}