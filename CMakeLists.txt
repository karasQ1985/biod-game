cmake_minimum_required(VERSION 3.16)
project(Biod LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Try Qt6 first, fall back to Qt5
find_package(Qt6 COMPONENTS Widgets OpenGLWidgets QUIET)
if(Qt6_FOUND)
    set(QT_LIBS Qt6::Widgets Qt6::OpenGLWidgets)
    message(STATUS "Using Qt6")
else()
    find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets)
    set(QT_LIBS Qt5::Widgets)
    message(STATUS "Using Qt5")
endif()

find_package(OpenGL REQUIRED)

set(SOURCES
    src/main.cpp
    src/core/ParamRegistry.cpp
    src/simulation/SpatialHash.cpp
    src/simulation/PlantSpatialHash.cpp
    src/simulation/Simulation.cpp
    src/simulation/SimulationConfig.cpp
    src/renderer/Renderer.cpp
    src/ui/GLWidget.cpp
    src/ui/MainWindow.cpp
)

set(HEADERS
    src/core/ParamDef.h
    src/core/ParamRegistry.h
    src/simulation/FlockData.h
    src/simulation/SpatialHash.h
    src/simulation/PlantSpatialHash.h
    src/simulation/Simulation.h
    src/renderer/Renderer.h
    src/ui/GLWidget.h
    src/ui/MainWindow.h
)

add_executable(Biod ${SOURCES} ${HEADERS})
target_include_directories(Biod PRIVATE src)
target_link_libraries(Biod PRIVATE ${QT_LIBS} OpenGL::GL)

# MSVC specific: suppress some warnings
if(MSVC)
    target_compile_options(Biod PRIVATE /W3 /wd4996)
endif()

# ---- Test target (纯 Simulation 层测试，不依赖 Qt/OpenGL) ----
set(TEST_SOURCES
    tests/test_runner.cpp
    src/simulation/SpatialHash.cpp
    src/simulation/PlantSpatialHash.cpp
    src/simulation/Simulation.cpp
)

add_executable(test_runner ${TEST_SOURCES})
target_include_directories(test_runner PRIVATE src)
# test_runner 只测试 Simulation 核心逻辑，无需链接 Qt 或 OpenGL
