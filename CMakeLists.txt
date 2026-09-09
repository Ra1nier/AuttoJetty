cmake_minimum_required(VERSION 3.18)

project(AuttoJetty LANGUAGES CXX)

include(GNUInstallDirs)

if (NOT UNIX OR APPLE)
    message(FATAL_ERROR "AuttoJetty is configured for Linux only.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenCV REQUIRED)
find_package(Torch REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAL_PIPEWIRE REQUIRED
    libportal
    libpipewire-0.3
    gio-2.0
    glib-2.0
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

add_executable(AuttoJetty
    AuttoJetty/src/AuttoJetty.cpp
    AuttoJetty/src/frameCapture.cpp
    AuttoJetty/src/jettyAI.cpp
    AuttoJetty/src/jettyPlayer.cpp
)

target_include_directories(AuttoJetty PRIVATE
    AuttoJetty/include
)

target_include_directories(AuttoJetty SYSTEM PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${PORTAL_PIPEWIRE_INCLUDE_DIRS}
)

target_link_libraries(AuttoJetty PRIVATE
    ${OpenCV_LIBS}
    "${TORCH_LIBRARIES}"
    ${PORTAL_PIPEWIRE_LIBRARIES}
)

target_compile_options(AuttoJetty PRIVATE ${PORTAL_PIPEWIRE_CFLAGS_OTHER})
target_link_directories(AuttoJetty PRIVATE ${PORTAL_PIPEWIRE_LIBRARY_DIRS})

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(AuttoJetty PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

install(TARGETS AuttoJetty
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
