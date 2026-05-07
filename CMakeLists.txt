cmake_minimum_required(VERSION 3.18)

project(AuttoJetty LANGUAGES CXX)

if (NOT UNIX OR APPLE)
    message(FATAL_ERROR "AuttoJetty is configured for Linux only.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenCV REQUIRED)
find_package(Torch REQUIRED)
find_package(X11 REQUIRED COMPONENTS X11 Xtst)

if (NOT X11_Xtst_FOUND)
    message(FATAL_ERROR "XTest was not found. Install the XTest development package, e.g. libxtst-dev.")
endif()

add_executable(AuttoJetty
    AuttoJetty/src/AuttoJetty.cpp
    AuttoJetty/src/frameCapture.cpp
    AuttoJetty/src/jettyAI.cpp
    AuttoJetty/src/jettyPlayer.cpp
)

target_include_directories(AuttoJetty PRIVATE
    AuttoJetty/include
    ${OpenCV_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
)

target_link_libraries(AuttoJetty PRIVATE
    ${OpenCV_LIBS}
    "${TORCH_LIBRARIES}"
    X11::X11
    X11::Xtst
)
