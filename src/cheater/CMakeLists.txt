qt_add_library(cheater STATIC
  cheater.cpp
)

target_link_libraries(cheater PRIVATE Qt6::Widgets)
target_include_directories(cheater INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})