add_compile_options(-Wall -std=c++11 -DBUILD_OPTLEVEL_OPT -DBUILD_COMPONENT_SRC_PREFIX="")

# Luma's rez packages does not setup LD_LIBRARY_PATH for USD,
# and that means sometimes the libs are not loaded properly.
# Especially in some of the python usd scripts, like usdcat.
set(CMAKE_INSTALL_RPATH "${USD_LIBRARY_DIR}")

include_directories(${USD_INCLUDE_DIR})
link_directories(${USD_LIBRARY_DIR})

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost COMPONENTS regex python REQUIRED)
find_package(PythonLibs REQUIRED)
find_package(TBB REQUIRED)

include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
include_directories(SYSTEM ${PYTHON_INCLUDE_DIRS})
include_directories(SYSTEM ${TBB_INCLUDE_DIRS})

if (BUILD_PARTIO_USD)
    add_subdirectory(fileFormat)
    add_subdirectory(topologyGenerator)
    install(FILES plugInfo.json
            DESTINATION .)
endif ()

if (BUILD_PARTIO_USD_MAYA)
    add_subdirectory(primWriter)
endif ()
