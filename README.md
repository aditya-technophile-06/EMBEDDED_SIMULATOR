# Embedded Systems Simulator

![Build Status](https://img.shields.io/badge/build-passing-brightgreen)
![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey)

A comprehensive **Virtual Embedded Systems Simulator** that demonstrates professional-level embedded systems programming concepts, perfect for showcasing embedded software development skills to potential employers.

## 🎯 **Project Overview**

This project simulates a complete embedded system environment, demonstrating ALL the key concepts and technologies mentioned in embedded systems job requirements:

### **✅ Skills Demonstrated:**
- **C/C++** - Modern C++17 implementation with RAII and smart pointers
- **Linux Device Drivers** - Character device simulation with `/dev/virtualdevice`
- **RTOS Concepts** - Priority-based task scheduling and real-time constraints  
- **SDK/API Development** - Complete peripheral abstraction layer
- **Protocol Software** - TCP/UDP networking and serial communication
- **Socket Programming** - Client-server architecture
- **Multithreading** - POSIX threads with proper synchronization
- **Interrupt Handling** - Callback-based interrupt simulation
- **Device Drivers** - Virtual device driver with IOCTL support
- **Hardware Abstraction** - Peripheral register simulation

## 🏗️ **Architecture**

```
Embedded Systems Simulator
├── Virtual SDK Layer           (Peripheral abstraction)
├── Device Driver Layer         (Linux character device simulation)
├── RTOS Layer                  (Real-time task scheduling)
├── Protocol Layer              (TCP/UDP networking)
└── Application Layer           (Demo and examples)
```

## 📁 **Project Structure**

```
embedded_simulator/
├── include/                    # Header files
│   ├── sdk/                   # Peripheral SDK headers
│   │   ├── peripheral.h       # Base peripheral class
│   │   ├── led.h             # LED peripheral
│   │   ├── button.h          # Button with interrupt handling
│   │   ├── sensor.h          # ADC sensor simulation
│   │   └── uart.h            # UART communication
│   ├── drivers/               # Device driver headers
│   │   └── virtual_device.h  # Virtual device driver
│   ├── rtos/                  # RTOS headers
│   │   ├── task.h            # Task management
│   │   └── scheduler.h       # Priority scheduler
│   └── protocol/              # Network protocol headers
├── src/                       # Source files
│   ├── sdk/                   # SDK implementation
│   ├── drivers/               # Driver implementation
│   ├── rtos/                  # RTOS implementation
│   ├── protocol/              # Protocol implementation
│   └── main.cpp               # Comprehensive demo
├── tests/                     # Unit tests
├── examples/                  # Example applications
├── device_files/              # Simulated device files
├── Makefile                   # Professional build system
├── CMakeLists.txt            # CMake build system
└── README.md                 # This file
```

## 🚀 **Quick Start**

### **Prerequisites**
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install build-essential cmake git g++ libpthread-stubs0-dev

# macOS
xcode-select --install
brew install cmake

# Windows (using MinGW)
# Install MSYS2 and then:
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake
```

### **Build and Run**
```bash
# Clone the repository
git clone <your-repo-url>
cd embedded_simulator

# Build using Makefile (recommended for Linux/macOS)
make debug          # Debug build with sanitizers
make release        # Optimized release build
make help           # See all available targets

# OR build using CMake
mkdir build && cd build
cmake ..
make

# Run the simulator
./embedded_simulator
```

## 🔧 **Features Demonstrated**

### **1. Virtual SDK (C++ Classes for Peripherals)**
- **LED Peripheral**: GPIO control, PWM brightness, blinking modes
- **Button Peripheral**: Interrupt-driven input, debouncing, long-press detection  
- **Sensor Peripheral**: ADC simulation, data filtering, threshold alerts
- **UART Peripheral**: Serial communication, baud rates, flow control

**Code Example:**
```cpp
// LED Control
LED status_led("status_led");
status_led.initialize();
status_led.setBrightness(75);
status_led.startBlinking(500);

// Button Interrupt
button.enableInterrupt(Button::EdgeType::FALLING, 
    [](Button::State state, std::chrono::milliseconds duration) {
        std::cout << "Button pressed for " << duration.count() << "ms" << std::endl;
    });
```

### **2. Device Driver Simulation (/dev/virtualdevice)**
- **Character Device Interface**: Open, close, read, write operations
- **IOCTL Commands**: Device control and configuration
- **Memory Mapping**: Virtual mmap support
- **Interrupt Handling**: IRQ simulation with callbacks

**Code Example:**
```cpp
VirtualDeviceDriver driver;
driver.loadDriver();
driver.registerDevice("my_device", DeviceType::LED_DEVICE);

int fd = driver.openDevice("my_device");
driver.writeDevice(fd, data, sizeof(data));
driver.ioctlDevice(fd, IOCTL_SET_CONFIG, &config);
```

### **3. RTOS Scheduler (Multithreading, Priorities)**
- **Priority-Based Scheduling**: Preemptive task scheduling
- **Real-Time Constraints**: Deadline monitoring and WCET analysis
- **Task States**: Ready, Running, Blocked, Suspended
- **Synchronization**: Mutexes and condition variables

**Code Example:**
```cpp
Task sensor_task("sensor_collection", Task::Priority::HIGH,
    [this]() {
        float temp = sensor->readTemperature();
        processData(temp);
    },
    Task::TaskType::PERIODIC,
    {std::chrono::milliseconds(100), // period
     std::chrono::milliseconds(10),  // deadline
     std::chrono::milliseconds(5),   // execution time
     std::chrono::milliseconds(8)}   // worst case
);
```

### **4. Protocol Software (TCP/UDP Client-Server)**
- **TCP Server**: Multi-client handling, thread-per-connection
- **UDP Client**: Connectionless communication
- **Protocol Management**: Message framing and error handling
- **Network Statistics**: Throughput and error monitoring

## 🧪 **Running Examples**

### **Basic LED Control**
```bash
# Run the main demo
./embedded_simulator

# Output shows:
# LED 'status_led' initialized
# Button interrupt enabled
# RTOS tasks created
# System running...
```

### **Advanced Features**
```bash
# Build with different configurations
make debug      # Debug with sanitizers
make release    # Optimized build
make profile    # Profiling enabled
make test       # Run unit tests
make analyze    # Static analysis
make memcheck   # Memory leak detection
```

## 📊 **Real-Time Performance Monitoring**

The simulator provides comprehensive performance metrics:

```
=== SYSTEM STATUS ===
LED Blinks: 1247
Button Presses: 15
Sensor Readings: 2890

Task Statistics:
  heartbeat: 1247 executions, 0 missed deadlines, 45.2% CPU
  sensor_collection: 2890 executions, 2 missed deadlines, 12.8% CPU
  system_monitor: 124 executions, 0 missed deadlines, 2.1% CPU

Device Driver Stats:
  Devices: 5, Reads: 15420, Writes: 8932, IOCTLs: 156
```

## 🔬 **Technical Deep Dive**

### **Embedded Systems Concepts Covered:**

1. **Hardware Abstraction Layer (HAL)**
   - Peripheral base class with common interface
   - Register-level simulation
   - Hardware-specific implementations

2. **Interrupt Service Routines (ISR)**
   - Edge and level triggering
   - Interrupt priority handling
   - Context switching simulation

3. **Real-Time Operating System (RTOS)**
   - Rate Monotonic Scheduling
   - Deadline Miss Detection
   - CPU Utilization Analysis

4. **Device Driver Development**
   - Character device interface
   - Kernel-space simulation
   - IOCTL command handling

5. **Communication Protocols**
   - UART serial communication
   - TCP/UDP networking
   - Protocol state machines

## 🛠️ **Build System Features**

### **Makefile Targets**
```bash
make debug      # Debug build with sanitizers
make release    # Optimized production build
make test       # Build and run unit tests
make docs       # Generate documentation
make install    # System-wide installation
make package    # Create distribution package
make analyze    # Static code analysis
make memcheck   # Memory leak detection
make format     # Code formatting
```

### **CMake Support**
- Cross-platform builds (Linux, macOS, Windows)
- Automatic dependency detection
- CTest integration for testing
- CPack for packaging

## 📚 **Learning Outcomes**

This project demonstrates proficiency in:

### **Core Embedded Concepts:**
- ✅ Memory-mapped I/O simulation
- ✅ Interrupt-driven programming
- ✅ Real-time task scheduling
- ✅ Device driver architecture
- ✅ Hardware abstraction layers
- ✅ Protocol implementation
- ✅ Synchronization primitives

### **Software Engineering:**
- ✅ Modern C++17 features
- ✅ Object-oriented design patterns
- ✅ Thread-safe programming
- ✅ Professional build systems
- ✅ Comprehensive testing
- ✅ Documentation practices

### **System Programming:**
- ✅ POSIX threading
- ✅ Socket programming
- ✅ File I/O operations
- ✅ Signal handling
- ✅ Memory management

## 🏢 **Industry Relevance**

This project directly maps to job requirements for:
- **Embedded Systems Engineer**
- **Firmware Developer**
- **Device Driver Developer**
- **RTOS Developer**
- **IoT Software Engineer**

### **Skills Validation:**
Every concept in this simulator is used in real embedded systems:
- **Nokia, Ericsson**: RTOS and protocol software
- **Intel, ARM**: Device driver development
- **Automotive (BMW, Tesla)**: Real-time constraints
- **IoT Companies**: SDK and hardware abstraction

## 🤝 **Contributing**

1. Fork the repository
2. Create a feature branch
3. Add comprehensive tests
4. Ensure all builds pass
5. Submit a pull request

## 📄 **License**

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 📧 **Contact**

For questions about embedded systems concepts or implementation details:
- **GitHub**: [Your GitHub Profile]
- **LinkedIn**: [Your LinkedIn Profile]
- **Email**: [Your Email]

---

**💡 This project demonstrates real-world embedded systems expertise through practical implementation of industry-standard concepts and patterns.**