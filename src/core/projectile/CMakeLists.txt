add_library(sphy_core_projectile INTERFACE)
target_sources(sphy_core_projectile INTERFACE
    projectile.cpp
)

target_include_directories(sphy_core_projectile
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(sphy_core_projectile
    INTERFACE
)
