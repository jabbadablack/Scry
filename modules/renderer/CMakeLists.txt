cmake_minimum_required(VERSION 3.19)
project(Renderer C CXX)

# Force Diligent to build as a static library so we get zero runtime DLLs
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(DILIGENT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# --- Diligent Engine Optimization Flags ---
# Disable EVERYTHING except Vulkan Core to minimize binary size
set(DILIGENT_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(DILIGENT_BUILD_TUTORIALS OFF CACHE BOOL "" FORCE)
set(DILIGENT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DILIGENT_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(DILIGENT_BUILD_FX OFF CACHE BOOL "" FORCE)

set(DILIGENT_NO_DIRECT3D11 ON CACHE BOOL "" FORCE)
set(DILIGENT_NO_DIRECT3D12 ON CACHE BOOL "" FORCE)
set(DILIGENT_NO_OPENGL ON CACHE BOOL "" FORCE)
set(DILIGENT_NO_METAL ON CACHE BOOL "" FORCE)
set(DILIGENT_NO_WEBGPU ON CACHE BOOL "" FORCE)
set(DILIGENT_NO_VULKAN OFF CACHE BOOL "" FORCE)
set(DILIGENT_NO_ARCHIVER ON CACHE BOOL "" FORCE)
set(DILIGENT_NO_RENDER_STATE_PACKAGER ON CACHE BOOL "" FORCE)
set(DILIGENT_NO_SUPER_RESOLUTION ON CACHE BOOL "" FORCE)

# --- CRITICAL: Disable install targets to bypass MinGW ar wildcard bug ---
set(DILIGENT_INSTALL_CORE OFF CACHE BOOL "" FORCE)
set(DILIGENT_INSTALL_TOOLS OFF CACHE BOOL "" FORCE)

include(FetchContent)

FetchContent_Declare(
    DiligentCore
    GIT_REPOSITORY https://github.com/DiligentGraphics/DiligentCore.git
    GIT_TAG master
    SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/DiligentCore
)

FetchContent_MakeAvailable(DiligentCore)

# Suppress DiligentCore warnings that don't affect our code:
#   -Winvalid-offsetof : PipelineStateBase uses offsetof on non-standard-layout for a
#                        static_assert alignment check — conditionally supported and safe.
#   -Winterference-size: MPSCQueue uses std::hardware_destructive_interference_size; the
#                        value is always 64 bytes on x86-64 so no ABI issue in practice.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    foreach(_diligent_target
        Diligent-Common
        Diligent-Primitives
        Diligent-GraphicsAccessories
        Diligent-GraphicsEngine
        Diligent-GraphicsEngineNextGenBase
        Diligent-GraphicsEngineVk-static
        Diligent-GraphicsTools
        Diligent-ShaderTools
        Diligent-HLSL2GLSLConverterLib
    )
        if(TARGET ${_diligent_target})
            target_compile_options(${_diligent_target} PRIVATE
                -Wno-invalid-offsetof
                -Wno-interference-size
            )
        endif()
    endforeach()
endif()

add_library(renderer STATIC src/diligent_module.cpp)

target_include_directories(renderer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    ${diligentcore_SOURCE_DIR}
)

target_include_directories(renderer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(renderer PUBLIC
    ENGINE
    Diligent-BuildSettings
    Diligent-GraphicsEngineVk-static
)

# Exclude shared variants from the build to avoid MinGW linker map script errors
if(TARGET Diligent-GraphicsEngineVk-shared)
    set_target_properties(Diligent-GraphicsEngineVk-shared PROPERTIES EXCLUDE_FROM_ALL ON)
    set_property(TARGET Diligent-GraphicsEngineVk-shared PROPERTY LINK_OPTIONS
        "LINKER:--version-script=${diligentcore_BINARY_DIR}/Graphics/GraphicsEngineVulkan/export.map"
    )
endif()
if(TARGET Diligent-GraphicsEngineOpenGL-shared)
    set_target_properties(Diligent-GraphicsEngineOpenGL-shared PROPERTIES EXCLUDE_FROM_ALL ON)
endif()
if(TARGET Diligent-Archiver-shared)
    set_target_properties(Diligent-Archiver-shared PROPERTIES EXCLUDE_FROM_ALL ON)
endif()
if(TARGET Diligent-SuperResolution-shared)
    set_target_properties(Diligent-SuperResolution-shared PROPERTIES EXCLUDE_FROM_ALL ON)
endif()
