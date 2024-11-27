set(SOPHGO_PLATFORM ON)

include(${ROOT_DIR}/cmake/macro.cmake)

set(CMAKE_CXX_STANDARD 17)

file(GLOB COMPONENTS LIST_DIRECTORIES  true ${ROOT_DIR}/components/*)

include(${PROJECT_DIR}/main/CMakeLists.txt)

foreach(component IN LISTS COMPONENTS)
    get_filename_component(component_name ${component} NAME)  
    if(EXISTS "${component}/CMakeLists.txt" AND component_name IN_LIST REQUIREDS)
        include("${component}/CMakeLists.txt")
    endif()
endforeach()

include(${ROOT_DIR}/cmake/build.cmake)

include(${ROOT_DIR}/cmake/package.cmake)
