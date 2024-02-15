qt_add_executable(pvz-cheater
  main.cpp
)

target_link_libraries(pvz-cheater PRIVATE
  cheater
  Qt6::Widgets
)

set_target_properties(pvz-cheater PROPERTIES
  WIN32_EXECUTABLE TRUE
)