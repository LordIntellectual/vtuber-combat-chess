cmake_minimum_required (VERSION 3.5)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vTuber Combat Chess — Lord Intellectual (ToonChess-derived)
# Linux (main) and Windows 11 (windows branch)
project(VTuberCombatChess)

# Allow find_package(Bullet) to honor BULLET_ROOT / <PackageName>_ROOT (CMake 3.12+)
if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()
if(POLICY CMP0144)
  cmake_policy(SET CMP0144 NEW)
endif()

set(ToonChess_VERSION_MAJOR 0)
set(ToonChess_VERSION_MINOR 2)

set(TOONCHESS_SHARE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/share)

# Mesh shaders still live under share/toonchess (base assets).
# Prefer runtime discovery next to the executable (portable double-click layout).
# Compile-time fallback: relative path on Windows (never embed C:/Users/...), install
# prefix on Linux.
if(WIN32)
  set(TOONCHESS_SHARE_PATH "\"./share/toonchess/\"")
else()
  file(TO_CMAKE_PATH "${CMAKE_INSTALL_PREFIX}/share/toonchess/" _VCC_SHARE_CMAKE)
  set(TOONCHESS_SHARE_PATH "\"${_VCC_SHARE_CMAKE}\"")
endif()
configure_file (
    "${CMAKE_SOURCE_DIR}/src/get_share_path.cxx.in"
    "${CMAKE_SOURCE_DIR}/src/get_share_path.cxx"
)

set(
  CXX_FILES
  ${CMAKE_SOURCE_DIR}/src/Camera/Camera.cxx
  ${CMAKE_SOURCE_DIR}/src/Camera/ActionCamera.cxx
  ${CMAKE_SOURCE_DIR}/src/ChessGame/ConnectionException.cxx
  ${CMAKE_SOURCE_DIR}/src/ChessGame/StockfishConnector.cxx
  ${CMAKE_SOURCE_DIR}/src/ChessGame/GameException.cxx
  ${CMAKE_SOURCE_DIR}/src/ChessGame/ChessGame.cxx
  ${CMAKE_SOURCE_DIR}/src/Clock/Clock.cxx
  ${CMAKE_SOURCE_DIR}/src/ColorPicking/ColorPicking.cxx
  ${CMAKE_SOURCE_DIR}/src/Event/EventStack.cxx
  ${CMAKE_SOURCE_DIR}/src/mesh/Mesh.cxx
  ${CMAKE_SOURCE_DIR}/src/mesh/meshes.cxx
  ${CMAKE_SOURCE_DIR}/src/mesh/loadObjFile.cxx
  ${CMAKE_SOURCE_DIR}/src/PhysicsWorld/Fragment.cxx
  ${CMAKE_SOURCE_DIR}/src/PhysicsWorld/PhysicsWorld.cxx
  ${CMAKE_SOURCE_DIR}/src/shader/CompilationException.cxx
  ${CMAKE_SOURCE_DIR}/src/shader/LinkingException.cxx
  ${CMAKE_SOURCE_DIR}/src/shader/Shader.cxx
  ${CMAKE_SOURCE_DIR}/src/shader/ShaderProgram.cxx
  ${CMAKE_SOURCE_DIR}/src/shader/shaderPrograms.cxx
  ${CMAKE_SOURCE_DIR}/src/ShadowMapping/ShadowMapping.cxx
  ${CMAKE_SOURCE_DIR}/src/SmokeGenerator/SmokeGenerator.cxx
  ${CMAKE_SOURCE_DIR}/src/utils/utils.cxx
  ${CMAKE_SOURCE_DIR}/src/utils/math.cxx
  ${CMAKE_SOURCE_DIR}/src/get_share_path.cxx
  ${CMAKE_SOURCE_DIR}/src/Theme/Theme.cxx
  ${CMAKE_SOURCE_DIR}/src/Audio/AudioEngine.cxx
  ${CMAKE_SOURCE_DIR}/src/FX/SparkSystem.cxx
  ${CMAKE_SOURCE_DIR}/src/FX/ScreenShake.cxx
  ${CMAKE_SOURCE_DIR}/src/UI/Hud.cxx
  ${CMAKE_SOURCE_DIR}/src/UI/SettingsMenu.cxx
  ${CMAKE_SOURCE_DIR}/src/UI/PieceEditor.cxx
  ${CMAKE_SOURCE_DIR}/src/UI/VictoryScreen.cxx
  ${CMAKE_SOURCE_DIR}/src/UI/PreAlphaSplash.cxx
  ${CMAKE_SOURCE_DIR}/src/Environment/Starfield.cxx
  ${CMAKE_SOURCE_DIR}/src/PieceSet/PieceSet.cxx
  ${CMAKE_SOURCE_DIR}/src/PieceSet/PieceTransform.cxx
  ${CMAKE_SOURCE_DIR}/src/Network/TcpSocket.cxx
  ${CMAKE_SOURCE_DIR}/src/Network/NetProtocol.cxx
  ${CMAKE_SOURCE_DIR}/src/Network/NetSession.cxx
)

install(DIRECTORY "${TOONCHESS_SHARE_DIR}/toonchess" DESTINATION "share")
# Game audio / piece sets / spark shaders live at repo share/nca (sibling of upstream)
set(NCA_SHARE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../share/nca")
if(EXISTS "${NCA_SHARE_DIR}")
  install(DIRECTORY "${NCA_SHARE_DIR}" DESTINATION "share")
endif()

# gl.c must stay pure C. On MSVC, compiling it as C++ (or force-including
# C++ headers via /FI) triggers STL1003. Isolate GLAD in its own C library.
enable_language(C)
set(GLAD_SRC ${CMAKE_SOURCE_DIR}/third_party/glad/src/gl.c)
set_source_files_properties(${GLAD_SRC} PROPERTIES LANGUAGE C)
add_library(vcc_glad STATIC ${GLAD_SRC})
target_include_directories(vcc_glad PUBLIC
  ${CMAKE_SOURCE_DIR}/third_party/glad/include
)
# Never apply C++-only flags to this target
if(MSVC)
  # Explicitly compile as C (/TC) for this TU
  target_compile_options(vcc_glad PRIVATE /TC /W3)
endif()

set(EXECUTABLE_NAME "VTuberCombatChess")
add_executable(${EXECUTABLE_NAME}
  src/NightfireChessArena.cxx
  ${CXX_FILES}
)
target_include_directories(${EXECUTABLE_NAME} PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/third_party
  ${CMAKE_SOURCE_DIR}/third_party/glad/include
)
target_link_libraries(${EXECUTABLE_NAME} vcc_glad)

if(MSVC)
  target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _USE_MATH_DEFINES
  )
  # Force M_PI + iso646 (and/or/not) on C++ TUs only — never on gl.c
  target_compile_options(${EXECUTABLE_NAME} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:/W3>
    $<$<COMPILE_LANGUAGE:CXX>:/EHsc>
    $<$<COMPILE_LANGUAGE:CXX>:/FI${CMAKE_SOURCE_DIR}/src/platform/msvc_compat.hxx>
  )
endif()

# Download and unpack glfw
configure_file(CMakeLists-glfw.txt.in glfw-download/CMakeLists.txt)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
# New CMake rejects very old cmake_minimum_required inside ExternalProject trees
set(ENV{CMAKE_POLICY_VERSION_MINIMUM} "3.5")
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}"
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/glfw-download
)
if(result)
  message(FATAL_ERROR "CMake step for glfw failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/glfw-download )
if(result)
  message(FATAL_ERROR "Build step for glfw failed: ${result}")
endif()

add_subdirectory(
    ${CMAKE_BINARY_DIR}/glfw-src
    ${CMAKE_BINARY_DIR}/glfw-build)
target_link_libraries(${EXECUTABLE_NAME} glfw)

find_package(OpenGL REQUIRED)
if (OPENGL_FOUND)
  include_directories(${OPENGL_INCLUDE_DIR})
  target_link_libraries(${EXECUTABLE_NAME} ${OPENGL_LIBRARIES})
endif()

find_package(Bullet 2.8 REQUIRED)
if(BULLET_FOUND)
  include_directories(${BULLET_INCLUDE_DIR})
  target_link_libraries(${EXECUTABLE_NAME} ${BULLET_LIBRARIES})
endif()

find_package(PNG REQUIRED)
if(PNG_FOUND)
  include_directories(${PNG_INCLUDE_DIRS})
  target_link_libraries(${EXECUTABLE_NAME} ${PNG_LIBRARIES})
endif()

# miniaudio: Threads on all platforms; dl + libm on Unix only
find_package(Threads REQUIRED)
target_link_libraries(${EXECUTABLE_NAME} Threads::Threads)
if(UNIX AND NOT APPLE)
  target_link_libraries(${EXECUTABLE_NAME} ${CMAKE_DL_LIBS} m)
endif()
if(WIN32)
  # WASAPI / winmm used by miniaudio backends; ws2_32 for multiplayer TCP
  target_link_libraries(${EXECUTABLE_NAME} winmm ole32 ws2_32)
endif()

install(TARGETS VTuberCombatChess RUNTIME DESTINATION bin)
