include(${ROOT_DIR}/cmake/macro.cmake)

set(CMAKE_CXX_STANDARD 17)

set(SSCMA_EXTENSION_BYTETRACK on)

file(GLOB COMPONENTS LIST_DIRECTORIES  true ${ROOT_DIR}/components/*)

foreach(component IN LISTS COMPONENTS)
    if(EXISTS ${component}/CMakeLists.txt)
        include(${component}/CMakeLists.txt)
    endif()
endforeach()

include(${PROJECT_DIR}/main/CMakeLists.txt)

include(${ROOT_DIR}/cmake/build.cmake)

include(${ROOT_DIR}/cmake/package.cmake)
