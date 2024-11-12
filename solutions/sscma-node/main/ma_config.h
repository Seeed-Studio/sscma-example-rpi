#ifndef MA_CONFIG_H
#define MA_CONFIG_H

#define MA_OSAL_PTHREAD                    1
#define MA_USE_FILESYSTEM                  1

#define MA_USE_EXCEPTION                   1
#define MA_DEBUG_LEVEL                     3

#define CONFIG_MA_FILESYSTEM               1
#define CONFIG_MA_FILESYSTEM_POSIX         1

#define MA_SEVER_AT_EXECUTOR_STACK_SIZE    80 * 1024
#define MA_SEVER_AT_EXECUTOR_TASK_PRIO     2

#define MA_NODE_CONFIG_FILE                "/etc/sscma.conf"

#include "logger.hpp"

#endif  // MA_CONFIG_H