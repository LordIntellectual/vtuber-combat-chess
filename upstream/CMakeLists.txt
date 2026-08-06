cmake_minimum_required (VERSION 3.5)
set(CMAKE_CXX_STANDARD 11)

# vTuber Combat Chess — Lord Intellectual (ToonChess-derived)
project(VTuberCombatChess)

set(ToonChess_VERSION_MAJOR 0)
set(ToonChess_VERSION_MINOR 2)

set(TOONCHESS_SHARE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/share)

# Mesh shaders still live under share/toonchess (base assets)
set(TOONCHESS_SHARE_PATH "\"${CMAKE_INSTALL_PREFIX}/share/toonchess/\"")
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
  ${CMAKE_SOURCE_DIR}/src/UI/MainMenu.cxx
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

set(EXECUTABLE_NAME "VTuberCombatChess")
add_executable(${EXECUTABLE_NAME} src/NightfireChessArena.cxx ${CXX_FILES})
target_include_directories(${EXECUTABLE_NAME} PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/third_party
)

# Download and unpack glfw
configure_file(CMakeLists-glfw.txt.in glfw-download/CMakeLists.txt)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
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

# miniaudio needs pthread / dl on Linux
find_package(Threads REQUIRED)
target_link_libraries(${EXECUTABLE_NAME} Threads::Threads ${CMAKE_DL_LIBS} m)

install(TARGETS VTuberCombatChess RUNTIME DESTINATION bin)
