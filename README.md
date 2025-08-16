# Virtual Embedded Systems Simulator

![Build Status](https://img.shields.io/badge/build-passing-brightgreen)
![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey)

A comprehensive embedded systems simulator demonstrating core embedded programming concepts through software simulation, ideal for learning and showcasing embedded systems expertise.

## Overview

This project simulates a complete embedded system environment without requiring physical hardware. It demonstrates key embedded systems concepts including device drivers, real-time operating systems, hardware abstraction, and protocol communication.

### Core Components

- **Virtual SDK** - C++ classes for peripheral simulation (LED, Button, Sensor, UART)
- **Device Driver Simulation** - Linux character device interface with `/dev/virtualdevice`
- **RTOS Scheduler** - Priority-based task scheduling with real-time constraints
- **Protocol Software** - TCP/UDP networking and serial communication concepts

## Project Structure

```
embedded_simulator/
├── include/
│   ├── sdk/          # Peripheral SDK (LED, Button, Sensor, UART)
│   ├── drivers/      # Virtual device driver
│   └── rtos/         # Real-time task management
├── src/              # Implementation files
├── docs/             # Documentation
├── tests/            # Unit tests
├── Makefile          # Build system
└── CMakeLists.txt    # Cross-platform build
```

## Quick Start

### Prerequisites

**Linux/Ubuntu:**
```bash
sudo apt update
sudo apt install build-essential cmake git g++
```

**macOS:**
```bash
xcode-select --install
brew install cmake
```

### Build and Run

```bash
# Clone and navigate to project
git clone <repository-url>
cd embedded_simulator

# Build the project
make clean
make release

# Run the simulator
./embedded_simulator
```

### Alternative CMake Build

```bash
mkdir build && cd build
cmake ..
make -j4
./embedded_simulator
```

## Features

### 1. Virtual SDK

**LED Peripheral**
- GPIO control simulation (on/off states)
- PWM brightness control (0-100%)
- Blinking functionality with configurable timing

**Button Peripheral**
- Interrupt-driven input handling
- Debouncing simulation
- Long-press detection

**Sensor Peripheral**
- ADC simulation for multiple sensor types
- Data filtering and threshold alerts
- Statistical analysis

**UART Peripheral**
- Serial communication simulation
- Configurable baud rates and protocols
- FIFO buffer management

### 2. Device Driver Simulation

- Linux character device interface
- IOCTL commands for device control
- Memory mapping simulation
- Device registration and management

### 3. RTOS Scheduler

- Priority-based preemptive scheduling
- Real-time deadline monitoring
- Task state management (Ready, Running, Blocked, Suspended)
- CPU utilization tracking

## Usage Examples

### Basic LED Control
```cpp
LED status_led("status_led");
status_led.initialize();
status_led.setBrightness(75);
status_led.startBlinking(500);
```

### Button Interrupt Handling
```cpp
button.enableInterrupt(Button::EdgeType::FALLING, 
    [](Button::State state, std::chrono::milliseconds duration) {
        std::cout << "Button pressed for " << duration.count() << "ms" << std::endl;
    });
```

### Device Driver Operations
```cpp
VirtualDeviceDriver driver;
driver.loadDriver();
driver.registerDevice("my_device", DeviceType::LED_DEVICE);

int fd = driver.openDevice("my_device");
driver.writeDevice(fd, data, sizeof(data));
driver.closeDevice(fd);
```

### RTOS Task Creation
```cpp
Task sensor_task("sensor_collection", Task::Priority::HIGH,
    []() {
        // Task implementation
        float temp = sensor->readTemperature();
        processData(temp);
    },
    Task::TaskType::PERIODIC,
    {std::chrono::milliseconds(100),  // period
     std::chrono::milliseconds(10),   // deadline
     std::chrono::milliseconds(5),    // execution time
     std::chrono::milliseconds(8)}    // worst case
);
```

## Build System

### Makefile Targets
```bash
make debug      # Debug build with sanitizers
make release    # Optimized production build
make clean      # Remove build artifacts
make help       # Show all available targets
```

### CMake Features
- Cross-platform compilation
- Automatic dependency detection
- Testing framework integration
- Package generation

## Skills Demonstrated

### Embedded Systems Concepts
- Hardware Abstraction Layer (HAL) design
- Interrupt Service Routine (ISR) simulation
- Real-time task scheduling and deadline management
- Device driver architecture and implementation
- Memory-mapped I/O simulation
- Communication protocol implementation

### Software Engineering
- Modern C++17 programming
- Object-oriented design patterns
- Thread-safe programming with POSIX threads
- Professional build system configuration
- Comprehensive error handling
- Clean code architecture

### System Programming
- Linux device driver concepts
- POSIX threading and synchronization
- Socket programming foundations
- File I/O operations
- Memory management best practices

## Technical Specifications

- **Language**: C++17
- **Platforms**: Linux, macOS, Windows
- **Threading**: POSIX threads
- **Build System**: Make, CMake
- **Code Lines**: 3,500+ lines
- **Components**: 4 peripheral types, device driver, RTOS scheduler

## Documentation

- `COMPLETE_PROJECT_GUIDE.md` - Comprehensive development and setup guide
- `PROJECT_SUMMARY.md` - Project achievements and job application guidance
- Header files contain detailed API documentation

## Testing

The project includes functionality verification:
- Device driver simulation testing
- Peripheral operation validation
- Build system verification across platforms

## Industry Relevance

This simulator demonstrates concepts directly applicable to:
- **Embedded Systems Engineering** - Complete embedded stack simulation
- **Device Driver Development** - Linux character device interface
- **RTOS Development** - Real-time task scheduling and management
- **Firmware Engineering** - Hardware abstraction and peripheral control
- **IoT Software Development** - Complete embedded software stack

## License

This project is provided for educational and portfolio purposes. See individual source files for specific licensing information.

---

**This project demonstrates practical embedded systems knowledge through hands-on implementation of industry-standard concepts and design patterns.**