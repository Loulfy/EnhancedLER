//
// Created by loulfy on 14/07/2023.
//

#ifndef LER_SPV_HPP
#define LER_SPV_HPP

#include "ler_sys.hpp"
#include <glslang/Public/ShaderLang.h>

namespace ler
{
    class DirStackFileIncluder : public glslang::TShader::Includer
    {
    public:

        struct IncludePack
        {
            Blob binary;
            std::unique_ptr<IncludeResult> result;
        };

        void addInclude(const std::string& include);
        IncludeResult* includeSystem(const char* include, const char* shader, size_t size) override;
        IncludeResult* includeLocal(const char* include, const char* shader, size_t size) override;
        void releaseInclude(IncludeResult* result) override {};

    private:

        std::unordered_map<std::string,IncludePack> m_includes;
    };

    class GlslangInitializer
    {
    public:

        GlslangInitializer();
        ~GlslangInitializer();

        static bool compile(glslang::TShader* shader, const std::string& code, EShMessages controls, const std::string& shaderName, const std::string& entryPointName = "main");
        static std::vector<uint32_t> compileGlslToSpv(const std::string& code, const fs::path& name);
        static void compileFile(const fs::path& input, const fs::path& output);
        static void shaderAutoCompile();

        static DirStackFileIncluder& Includer();
    };
}

#endif //LER_SPV_HPP
