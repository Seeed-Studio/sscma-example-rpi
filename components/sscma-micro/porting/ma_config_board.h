#ifndef MA_CONFIG_H
#define MA_CONFIG_H

#define MA_OSAL_PTHREAD                 1
#define MA_USE_FILESYSTEM               1

#define MA_USE_EXCEPTION                1
#define MA_DEBUG_LEVEL                  3

#define PRODUCT_NAME_PREFIX          "raspberrypi"
#define PRODUCT_NAME_SUFFIX          "raspberrypi"
#define PORT_DEVICE_NAME             "raspberrypi"

#define CONFIG_MA_FILESYSTEM            1
#define CONFIG_MA_FILESYSTEM_POSIX      1

#define MA_SEVER_AT_EXECUTOR_STACK_SIZE 80 * 1024
#define MA_SEVER_AT_EXECUTOR_TASK_PRIO  2

#define MA_NODE_CONFIG_FILE             "/etc/sscma.conf"

#define MA_USE_ENGINE_HAILO             1

#define ma_malloc                       malloc
#define ma_calloc                       calloc
#define ma_realloc                      realloc
#define ma_free                         free
#define ma_printf                       printf
#define ma_abort                        abort
#define ma_reset                        abort

#endif  // MA_CONFIG_H