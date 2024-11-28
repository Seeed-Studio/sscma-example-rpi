
#include <iostream>
#include <syslog.h>
#include <unistd.h>

#include "version.h"

#include "signal.h"

#include "node/server.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>


const std::string TAG = "sscma";

using namespace ma;
using namespace ma::node;
void show_version() {
    std::cout << PROJECT_VERSION << std::endl;
}

void show_help() {
    std::cout << "Usage: sscma-node [options]\n"
              << "Options:\n"
              << "  -v, --version        Show version\n"
              << "  -h, --help           Show this help message\n"
              << "  --start              Start the service\n"
              << "  --deamon             Run in deamon mode\n"
              << std::endl;
}

int main(int argc, char** argv) {

    openlog("sscma", LOG_CONS | LOG_PERROR, LOG_DAEMON);

    MA_LOGD("main", "version: %s build: %s", PROJECT_VERSION, __DATE__ " " __TIME__);

    Signal::install({SIGINT, SIGSEGV, SIGABRT, SIGTRAP, SIGTERM, SIGHUP, SIGQUIT, SIGPIPE}, [](int sig) {
        MA_LOGE(TAG, "received signal %d", sig);
        NodeFactory::clear();
        closelog();
        exit(1);
    });


    bool start_service = false;
    bool deamon        = false;

    if (argc < 2) {
        show_help();
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-v" || arg == "--version") {
            show_version();
            return 0;
        } else if (arg == "-h" || arg == "--help") {
            show_help();
            return 0;
        } else if (arg == "--start") {
            start_service = true;
        } else if (arg == "--deamon") {
            deamon = true;
        } else {
            std::cerr << "Error: Unknown option " << arg << std::endl;
            return 1;
        }
    }


    if (start_service) {


        MA_LOGI(TAG, "starting the service...");
        MA_LOGI(TAG, "version: %s build: %s", PROJECT_VERSION, __DATE__ " " __TIME__);

        if (deamon) {
            char* err;
            pid_t pid;

            pid = fork();
            if (pid < 0) {
                err = strerror(errno);
                MA_LOGE(TAG, "Error in fork: %s", err);
                exit(1);
            }
            if (pid > 0) {
                exit(0);
            }
            if (setsid() < 0) {
                err = strerror(errno);
                MA_LOGE(TAG, "Error in setsid: %s", err);
                exit(1);
            }
        }

        NodeServer server("pi");

        server.start("localhost", 1883);

        uint32_t count = 0;

        while (1) {
            Thread::sleep(Tick::fromSeconds(1));
        }
    }

    closelog();
    return 0;
}
