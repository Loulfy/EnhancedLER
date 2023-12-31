cmake_minimum_required(VERSION 3.25)
project(ler)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(SPIRV_REFLECT_EXAMPLES OFF)
set(SPIRV_REFLECT_EXECUTABLE OFF)
set(SPIRV_REFLECT_STATIC_LIB ON)
set(RTXMU_WITH_D3D12 OFF)
set(FFX_API_VK ON)
set(FFX_API_DX12 OFF)
set(FFX_DENOISER ON)
set(FFX_CLASSIFIER ON)
set(ENABLE_HLSL ON)

find_package(Vulkan REQUIRED)
find_package(spdlog REQUIRED)
find_package(assimp REQUIRED)
find_package(glfw3 REQUIRED)
find_package(flecs REQUIRED)
find_package(PhysX REQUIRED)
find_package(EnTT REQUIRED)
find_package(inih REQUIRED)
find_package(glm REQUIRED)
find_package(stb REQUIRED)
find_package(gli REQUIRED)
find_package(SPIRV-Tools REQUIRED)
find_package(meshoptimizer REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(vulkan-memory-allocator REQUIRED)
find_package(bshoshany-thread-pool REQUIRED)

add_subdirectory(libs/RTXMU)
add_subdirectory(libs/glslang)
add_subdirectory(libs/SPIRV-Reflect)
#add_subdirectory(libs/FidelityFX-SDK/sdk)

include_directories(src)
include_directories(libs/imgui)
include_directories(libs/imgui/backends)
include_directories(libs/ImGuizmo)


add_definitions(-DPROJECT_DIR=\"${PROJECT_SOURCE_DIR}\")

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set (WIN32_RESOURCES "res/ler.rc")
endif()

set(IMGUI
    "libs/imgui/imconfig.h"
    "libs/imgui/imgui.h"
    "libs/imgui/imgui_internal.h"
    "libs/imgui/imstb_rectpack.h"
    "libs/imgui/imstb_textedit.h"
    "libs/imgui/imstb_truetype.h"
    "libs/imgui/imgui.cpp"
    "libs/imgui/imgui_demo.cpp"
    "libs/imgui/imgui_draw.cpp"
    "libs/imgui/imgui_tables.cpp"
    "libs/imgui/imgui_widgets.cpp"
    "libs/imgui/backends/imgui_impl_vulkan.h"
    "libs/imgui/backends/imgui_impl_vulkan.cpp"
    "libs/imgui/backends/imgui_impl_glfw.h"
    "libs/imgui/backends/imgui_impl_glfw.cpp"
    "libs/ImGuizmo/ImGuizmo.h"
    "libs/ImGuizmo/ImGuizmo.cpp"
)

set(LER
    "src/ler_shader.hpp"
    "src/format.cpp"
    "src/ler_log.hpp"
    "src/ler_vki.hpp"
    "src/ler_vki.cpp"
    "src/ler_dev.hpp"
    "src/ler_dev.cpp"
    "src/ler_cmd.cpp"
    "src/ler_scn.hpp"
    "src/ler_scn.cpp"
    "src/ler_sys.hpp"
    "src/ler_sys.cpp"
    "src/ler_svc.hpp"
    "src/ler_svc.cpp"
    "src/ler_gui.hpp"
    "src/ler_gui.cpp"
    "src/ler_spv.hpp"
    "src/ler_spv.cpp"
    "src/ler_img.hpp"
    "src/ler_img.cpp"
    "src/ler_res.hpp"
    "src/ler_res.cpp"
    "src/ler_cam.hpp"
    "src/ler_cam.cpp"
    "src/ler_csm.hpp"
    "src/ler_csm.cpp"
    "src/ler_psx.hpp"
    "src/ler_psx.cpp"
    "src/ler_mesh.hpp"
    "src/ler_mesh.cpp"
    "src/ler_cull.hpp"
    "src/ler_cull.cpp"
    "src/ler_draw.hpp"
    "src/ler_draw.cpp"
    "src/ler_app.hpp"
    "src/ler_app.cpp"
    "src/mpsc.inl"
    "src/mpsc.hpp"
)

#ffx_denoiser_x64
#ffx_classifier_x64
#ffx_backend_vk_x64

file(COPY ${PROJECT_SOURCE_DIR}/src/ler_shader.hpp DESTINATION ${PROJECT_SOURCE_DIR}/assets/)
add_executable(ler main.cpp ${LER} ${IMGUI} ${WIN32_RESOURCES})
target_link_libraries(ler PRIVATE
    Vulkan::Vulkan
    spdlog::spdlog
    assimp::assimp
    EnTT::EnTT
    inih::inih
    glfw
    glm::glm
    stb::stb
    gli::gli
    vulkan-memory-allocator::vulkan-memory-allocator
    bshoshany-thread-pool::bshoshany-thread-pool
    spirv-reflect-static
    flecs::flecs_static
    glslang
    glslang-default-resource-limits
    SPIRV
    rtxmu
    physx::physx
    PhysX::PhysXFoundation
    PhysX::PhysXCommon
    PhysX::PhysXPvdSDK
    PhysX::PhysX
    PhysX::PhysXCooking
    PhysX::PhysXExtensions
    PhysX::PhysXCharacterKinematic
    meshoptimizer::meshoptimizer
    wbemuuid
    nlohmann_json::nlohmann_json
)