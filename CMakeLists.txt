cmake_minimum_required(VERSION 3.30)
project(JettyBootPlayer)

set(CMAKE_CXX_STANDARD 20)

add_executable(JettyBootPlayer src/autoJetty.cpp
        src/autoJetty.cpp
        include/jettyPlayer.h
        src/jettyPlayer.cpp
        src/frameCapture.cpp
        include/frameCapture.h)

set(OpenCV_DIR "C:/Users/Max/opencv")

# Find OpenCV
find_package(OpenCV REQUIRED)

# Link OpenCV libraries
target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})