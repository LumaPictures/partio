SET(MODULE_NAME "partio")

find_package(Clarisse)

add_definitions(-Wall -Wsign-compare -fPIC -std=c++11 -fno-rtti)

# hardcoding eh
set(PARTIO_INCLUDE_DIR "/Volumes/sv-dev01/soft/rez-release/partio/0.9.9/platform-linux/gcc-4.1.2/include/")

gen_cmas(MODULE_CMAS_PARTIO)

include_directories(${CLARISSE_INCLUDE_DIR})
link_directories(${CLARISSE_LIBRARY_DIR})

include_directories(${PARTIO_INCLUDE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
link_directories(${PARTIO_LIB_DIR})
add_library(${MODULE_NAME} SHARED main.cpp ${MODULE_CMAS_PARTIO})
set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "")
target_link_libraries(${MODULE_NAME} "/Volumes/sv-dev01/soft/rez-release/partio/0.9.9/platform-linux/gcc-4.1.2/lib/libpartio.a" ix_core ix_gmath ix_geometry ix_of ix_point_cloud ix_module ix_poly ix_particle) #link the clarisse libs here

install(TARGETS ${MODULE_NAME}
    LIBRARY DESTINATION modules)
