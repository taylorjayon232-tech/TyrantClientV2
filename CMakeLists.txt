# ============================================================================
#  Passive — native C++ client for Minecraft 1.21.11 (Fabric)
#  Targets:
#    passive_agent.dll    — the client (injected into javaw.exe)
#    passive_injector.exe — standalone loader UI
#    passive_bridge.dll   — JNI shim the Fabric jar loads (fabric/bridge/bridge.cpp)
# ============================================================================
cmake_minimum_required(VERSION 3.20)
project(Passive LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (NOT WIN32)
  message(FATAL_ERROR "Passive targets Windows only.")
endif()

# Vendored third-party code lives in vendor/ (run vendor/fetch.bat once).
set(VENDOR ${CMAKE_SOURCE_DIR}/vendor)
if (NOT EXISTS ${VENDOR}/imgui/imgui.h)
  message(FATAL_ERROR "vendor/imgui missing — run vendor\\fetch.bat first.")
endif()
if (NOT EXISTS ${VENDOR}/minhook/include/MinHook.h)
  message(FATAL_ERROR "vendor/minhook missing — run vendor\\fetch.bat first.")
endif()

# ----------------------------- shared: ImGui --------------------------------
add_library(imgui STATIC
  ${VENDOR}/imgui/imgui.cpp
  ${VENDOR}/imgui/imgui_draw.cpp
  ${VENDOR}/imgui/imgui_tables.cpp
  ${VENDOR}/imgui/imgui_widgets.cpp
  ${VENDOR}/imgui/imgui_demo.cpp
  ${VENDOR}/imgui/backends/imgui_impl_win32.cpp
  ${VENDOR}/imgui/backends/imgui_impl_opengl3.cpp
  ${VENDOR}/imgui/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui PUBLIC ${VENDOR}/imgui ${VENDOR}/imgui/backends)
target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
if (MSVC)
  target_compile_options(imgui PRIVATE /W3 /permissive-)
endif()

# ----------------------------- shared: MinHook ------------------------------
file(GLOB MINHOOK_SOURCES ${VENDOR}/minhook/src/*.c ${VENDOR}/minhook/src/hde/*.c)
add_library(minhook STATIC ${MINHOOK_SOURCES})
target_include_directories(minhook PUBLIC ${VENDOR}/minhook/include)

# ----------------------------- agent ---------------------------------------
file(GLOB_RECURSE AGENT_SOURCES CONFIGURE_DEPENDS agent/*.cpp)
# Legacy Module-class files from an earlier architecture are not part of the build (macros.cpp does
# not even compile any more). The live module system is agent/modules/modules.cpp.
list(FILTER AGENT_SOURCES EXCLUDE REGEX "agent/modules/(macros|client|combat|movement|player|render|registry_builtins)\\.cpp$")
add_library(passive_agent SHARED ${AGENT_SOURCES})
target_include_directories(passive_agent PRIVATE
  agent
  ${VENDOR}
  ${VENDOR}/json
  ${CMAKE_SOURCE_DIR}/thirdparty/jdk-include
  ${CMAKE_SOURCE_DIR}/thirdparty/jdk-include/win32
)
target_link_libraries(passive_agent PRIVATE imgui minhook opengl32 user32 gdi32 shell32 windowscodecs ole32)
target_compile_definitions(passive_agent PRIVATE
  WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS PASSIVE_AGENT_BUILD
)
# The agent must not spawn a console window when loaded.
set_target_properties(passive_agent PROPERTIES PREFIX "")
if (MSVC)
  target_compile_options(passive_agent PRIVATE /W4 /permissive- /EHsc /utf-8)
  target_link_options(passive_agent PRIVATE /DEBUG:NONE)
endif()

# Ship the bundled Inter faces beside the injected DLL so the in-game menu
# renders consistently without relying on fonts installed on the host.
set(TYRANT_FONT_DIR ${CMAKE_SOURCE_DIR}/assets/fonts)
add_custom_command(TARGET passive_agent POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:passive_agent>/assets/fonts"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${TYRANT_FONT_DIR}/Inter-Regular.ttf"
    "${TYRANT_FONT_DIR}/Inter-Medium.ttf"
    "${TYRANT_FONT_DIR}/Inter-SemiBold.ttf"
    "$<TARGET_FILE_DIR:passive_agent>/assets/fonts"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/assets/passive-logo.png"
    "$<TARGET_FILE_DIR:passive_agent>/assets/passive-logo.png"
  VERBATIM
)

# ----------------------------- fabric bridge -------------------------------
# The Fabric jar (see fabric/) carries passive_agent.dll and this shim. Only the
# shim is loaded with System.load; it maps the agent with plain LoadLibraryW so
# the JVM holds no reference to it, which is what lets the agent's own graceful
# unload really remove the module - and the standalone injector map it again
# afterwards. The agent's ABI header is shared, so the pair cannot drift apart.
add_library(passive_bridge SHARED fabric/bridge/bridge.cpp)
target_include_directories(passive_bridge PRIVATE
  ${CMAKE_SOURCE_DIR}/agent/core
  ${CMAKE_SOURCE_DIR}/thirdparty/jdk-include
  ${CMAKE_SOURCE_DIR}/thirdparty/jdk-include/win32
)
target_link_libraries(passive_bridge PRIVATE user32)
target_compile_definitions(passive_bridge PRIVATE
  WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS
)
set_target_properties(passive_bridge PROPERTIES PREFIX "")
if (MSVC)
  target_compile_options(passive_bridge PRIVATE /W4 /permissive- /EHsc /utf-8)
  target_link_options(passive_bridge PRIVATE /DEBUG:NONE)
endif()

# ----------------------------- injector ------------------------------------
file(GLOB INJECTOR_SOURCES CONFIGURE_DEPENDS injector/*.cpp)
add_executable(passive_injector WIN32 ${INJECTOR_SOURCES})
target_include_directories(passive_injector PRIVATE
  injector ${VENDOR}
)
target_link_libraries(passive_injector PRIVATE imgui d3d11 dxgi user32 gdi32 windowscodecs ole32)
target_compile_definitions(passive_injector PRIVATE
  WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS TYRANT_INJECTOR_BUILD
)
if (MSVC)
  target_compile_options(passive_injector PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()
add_custom_command(TARGET passive_injector POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/assets/passive-logo.png"
    "$<TARGET_FILE_DIR:passive_injector>/passive-logo.png"
  VERBATIM
)
