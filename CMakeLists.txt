# https://zhuanlan.zhihu.com/p/608615780、https://doc.qt.io/qt-6/cmake-get-started.html
cmake_minimum_required(VERSION 3.16)
project(pvz-cheater VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

add_subdirectory(src/app)
add_subdirectory(src/cheater)