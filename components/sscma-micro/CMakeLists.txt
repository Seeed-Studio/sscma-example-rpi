set(SSCMA_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/sscma-micro)
set(SSCMA_PORTING_DIR ${CMAKE_CURRENT_LIST_DIR}/porting)

include(${SSCMA_ROOT_DIR}/3rdparty/json/CMakeLists.txt)

include(${SSCMA_ROOT_DIR}/3rdparty/eigen/CMakeLists.txt)

file(GLOB_RECURSE CORE_SOURCES ${SSCMA_ROOT_DIR}/sscma/core/*.c ${SSCMA_ROOT_DIR}/sscma/core/*.cpp)

file(GLOB_RECURSE PORT_SOURCES ${SSCMA_ROOT_DIR}/sscma/porting/posix/*.c ${SSCMA_ROOT_DIR}/sscma/porting/posix/*.cpp  ${SSCMA_ROOT_DIR}/sscma/porting/osal/ma_osal_pthread.cpp)

file(GLOB_RECURSE PORTING_SOURCES ${SSCMA_PORTING_DIR}/*.c ${SSCMA_PORTING_DIR}/*.cpp)

file(GLOB_RECURSE BYTETRACK_SSCMA_SOURCES ${SSCMA_ROOT_DIR}/sscma/extension/bytetrack/*.c ${SSCMA_ROOT_DIR}/sscma/extension/bytetrack/*.cpp)

file(GLOB_RECURSE COUNTER_SSCMA_SOURCES ${SSCMA_ROOT_DIR}/sscma/extension/counter/*.c ${SSCMA_ROOT_DIR}/sscma/extension/counter/*.cpp)

list(APPEND EXTENSION_SSCMA_SOURCES ${BYTETRACK_SSCMA_SOURCES} ${COUNTER_SSCMA_SOURCES})

set(SOURCES ${CORE_SOURCES} ${SREVER_SOURCES} ${CLIENT_SOURCES} ${PORT_SOURCES} ${PORTING_SOURCES} ${SRCS_EIGEN} ${SRCS_CJSON} ${EXTENSION_SSCMA_SOURCES}) 

set(INCS 
        ${INCS_EIGEN}
        ${INCS_CJSON}
        ${SSCMA_ROOT_DIR}/sscma
        ${SSCMA_PORTING_DIR}/
)

add_compile_options(-DCONFIG_MA_ENGINE_HAILO=1)

find_package(HailoRT 4.18.0 EXACT REQUIRED)


component_register(
    COMPONENT_NAME sscma-micro
    SRCS ${SOURCES}
    INCLUDE_DIRS ${INCS}
    PRIVATE_REQUIREDS mosquitto HailoRT::libhailort
) 