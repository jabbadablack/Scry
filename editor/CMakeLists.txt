cmake_minimum_required(VERSION 3.19)
project(ScryEditor C CXX)

include(FetchContent)

FetchContent_Declare(tracy GIT_REPOSITORY https://github.com/wolfpld/tracy.git GIT_TAG v0.13.1)
set(TRACY_ENABLE ON CACHE BOOL "" FORCE)
set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)
set(TRACY_NO_SYSTEM_TRACING ON CACHE BOOL "" FORCE)
set(TRACY_NO_FRAME_IMAGE ON CACHE BOOL "" FORCE)
set(TRACY_NO_CALLSTACK ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(tracy)

# DiligentTools bundles imgui (main branch) and Diligent-Imgui compiles it
# internally when no external 'imgui' CMake target is present at configure time.
# We only need to provide the GLFW platform backend, compiled against that same
# bundled imgui so the ABI stays consistent with Diligent-Imgui.
set(DILIGENT_IMGUI_DIR "${CMAKE_BINARY_DIR}/_deps/DiligentTools/ThirdParty/imgui")

add_library(imgui_glfw_backend STATIC
    "${DILIGENT_IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
)
target_include_directories(imgui_glfw_backend PUBLIC
    "${DILIGENT_IMGUI_DIR}"
    "${DILIGENT_IMGUI_DIR}/backends"
    "${CMAKE_BINARY_DIR}/_deps/DiligentTools/Imgui/interface"
)
target_compile_definitions(imgui_glfw_backend PUBLIC
    IMGUI_DEFINE_MATH_OPERATORS
    "IMGUI_USER_CONFIG=\"ImGuiDiligentConfig.h\""
)
target_link_libraries(imgui_glfw_backend PUBLIC glfw)

add_executable(scry-editor src/main.cpp src/glfw_impl.cpp)

target_link_libraries(scry-editor PRIVATE
    SCRY
    renderer
    imgui_glfw_backend
    TracyClient
    Diligent-Imgui
)
