# SSCMA Example for RPI

This repository provides a compilation framework to support running applications on the Raspberry platform.

## Prerequisites

This project depends on the **HailoRT**  For installation instructions, see the [Hailo Raspberry Pi 5 installation guide](https://github.com/hailo-ai/hailo-rpi5-examples/blob/main/doc/install-raspberry-pi5.md#how-to-set-up-raspberry-pi-5-and-hailo).


## Compilation Instructions

Follow the steps below to compile and package the project:

### 1. Clone the Repository
```bash
git clone https://github.com/Seeed-Studio/sscma-example-rpi
cd sscma-example-rpi
git submodule update --init
```

### 2. Build the Application

Navigate to the solution directory, configure the build, and compile the project:

```bash
cd solutions/helloworld
cmake -B build -DCMAKE_BUILD_TYPE=Release .
cmake --build build
```

### 3. Package the Application

To package the application, run the following command in the build directory:

```bash
cd build && cpack
```

### 4. Install the Application

To install the application, run the following command in the device:

You need to upload the package to your device first.

```bash
dpkg --install helloworld-1.0.0-1.deb
```
