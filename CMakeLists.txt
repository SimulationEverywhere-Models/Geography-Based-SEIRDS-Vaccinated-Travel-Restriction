cmake_minimum_required (VERSION 3.16 FATAL_ERROR)

### <GCC> ##
    set (CMAKE_CXX_STANDARD 17)
    set (CMAKE_C_COMPILER "gcc")
    set (CMAKE_CXX_COMPILER "g++")
    if ("${VERBOSE}" STREQUAL "N")
        add_compile_options(-w)
    endif()

    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
        add_compile_options(-g)
    endif()

    if("${WALL}" STREQUAL "Y")
        set(CMAKE_CXX_FLAGS "-Wall")
    endif()

    if("${PROFILER}" STREQUAL "Y")
        set(CMAKE_CXX_FLAGS "-pg")
    endif()
### <GCC> ##

project(pandemic-geographical_model)

# Out Directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

### <CADMIUM> ###
    get_filename_component(cadmium "../cadmium/json" REALPATH)

    if(EXISTS "${cadmium}")
        include_directories(../cadmium/include)
        include_directories(../cadmium/json/include)
    else()
        get_filename_component(cadmium "../../cadmium/json" REALPATH)
        if (EXISTS "${cadmium}")
            include_directories(../../cadmium/include)
            include_directories(../../cadmium/json/include)
        else()
            message(FATAL_ERROR "Could not find cadmium!")
        endif()
    endif()
### </CADMIUM> ###

### <Boost> ###
    set(Boost_USE_MULTITHREADED TRUE)
    set(Boost_USE_STATIC_RUNTIME OFF)
    set(Boost_NO_SYSTEM_PATHS TRUE)

    if(WIN32)
        list(APPEND CMAKE_PREFIX_PATH "C:\\Program Files\\Boost")
        set(Boost_THREADAPI "win32")
    endif()

    if (Boost_NO_SYSTEM_PATHS)
        set(Boost_INCLUDE_DIRS "${BOOST_ROOT}/include")
        set(BOOST_LIBRARY_DIRS "${BOOST_ROOT}/lib")
    endif()

    find_package(Boost COMPONENTS system thread REQUIRED)

    if(Boost_FOUND)
        include_directories(${Boost_INCLUDE_DIRS})
    else()
        message(FATAL_ERROR "Could Not Find Boost!!")
    endif()
### </Boost> ###

file(MAKE_DIRECTORY logs)
add_executable(pandemic-geographical_model src/main.cpp)
target_link_libraries(pandemic-geographical_model PUBLIC ${Boost_LIBRARIES})