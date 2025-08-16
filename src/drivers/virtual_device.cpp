#include "drivers/virtual_device.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <sys/stat.h>
#include <chrono>
#include <thread>

VirtualDeviceDriver::VirtualDeviceDriver()
    : driver_loaded(false),
      driver_name("virtual_device"),
      driver_version("1.0.0"),
      next_major_number(200), // Starting major number for virtual devices
      next_file_descriptor(1000),
      total_reads(0),
      total_writes(0),
      total_ioctls(0),
      total_irqs(0) {
}

VirtualDeviceDriver::~VirtualDeviceDriver() {
    if (driver_loaded) {
        unloadDriver();
    }
}

bool VirtualDeviceDriver::loadDriver() {
    std::lock_guard<std::mutex> lock(driver_mutex);
    
    if (driver_loaded) {
        std::cout << "Driver already loaded" << std::endl;
        return true;
    }
    
    // Create device files directory if it doesn't exist
    struct stat st;
    memset(&st, 0, sizeof(st));
    if (stat("device_files", &st) == -1) {
        if (mkdir("device_files", 0755) != 0) {
            std::cerr << "Error: Failed to create device_files directory" << std::endl;
            return false;
        }
    }
    
    driver_loaded = true;
    
    std::cout << "Virtual Device Driver '" << driver_name 
              << "' v" << driver_version << " loaded successfully" << std::endl;
    
    return true;
}

bool VirtualDeviceDriver::unloadDriver() {
    std::lock_guard<std::mutex> lock(driver_mutex);
    
    if (!driver_loaded) {
        return true;
    }
    
    // Close all open devices and unregister all devices
    for (auto& [name, device_info] : device_registry) {
        if (device_info->state == DeviceState::OPENED) {
            device_info->state = DeviceState::INITIALIZED;
        }
        removeDeviceFile(name);
    }
    
    device_registry.clear();
    fd_to_device_map.clear();
    irq_handlers.clear();
    irq_enabled.clear();
    device_memory.clear();
    
    driver_loaded = false;
    
    std::cout << "Virtual Device Driver unloaded" << std::endl;
    return true;
}

bool VirtualDeviceDriver::registerDevice(const std::string& name, DeviceType type, size_t memory_size) {
    std::lock_guard<std::mutex> lock(driver_mutex);
    
    if (!driver_loaded) {
        std::cerr << "Error: Driver not loaded" << std::endl;
        return false;
    }
    
    if (device_registry.find(name) != device_registry.end()) {
        std::cerr << "Error: Device '" << name << "' already registered" << std::endl;
        return false;
    }
    
    // Create device info
    auto device_info = std::make_unique<DeviceInfo>();
    device_info->name = name;
    device_info->type = type;
    device_info->state = DeviceState::INITIALIZED;
    device_info->major_number = next_major_number++;
    device_info->minor_number = 0;
    device_info->memory_size = memory_size;
    device_info->supports_mmap = true;
    device_info->supports_irq = (type != DeviceType::GENERIC_DEVICE);
    device_info->driver_version = driver_version;
    device_info->device_file_path = getDeviceFilePath(name);
    
    // Create device file
    if (!createDeviceFile(name)) {
        std::cerr << "Error: Failed to create device file for '" << name << "'" << std::endl;
        return false;
    }
    
    device_registry[name] = std::move(device_info);
    
    std::cout << "Device '" << name << "' (" << deviceTypeToString(type) 
              << ") registered successfully" << std::endl;
    
    return true;
}

bool VirtualDeviceDriver::unregisterDevice(const std::string& name) {
    std::lock_guard<std::mutex> lock(driver_mutex);
    
    auto it = device_registry.find(name);
    if (it == device_registry.end()) {
        std::cerr << "Error: Device '" << name << "' not found" << std::endl;
        return false;
    }
    
    // Close device if it's open
    if (it->second->state == DeviceState::OPENED) {
        std::cerr << "Error: Cannot unregister device '" << name << "' - device is open" << std::endl;
        return false;
    }
    
    // Remove device file
    removeDeviceFile(name);
    
    // Remove from registry
    device_registry.erase(it);
    
    std::cout << "Device '" << name << "' unregistered successfully" << std::endl;
    return true;
}

int VirtualDeviceDriver::openDevice(const std::string& device_name, int /* flags */) {
    std::lock_guard<std::mutex> lock(driver_mutex);
    
    if (!driver_loaded) {
        std::cerr << "Error: Driver not loaded" << std::endl;
        return -1;
    }
    
    auto it = device_registry.find(device_name);
    if (it == device_registry.end()) {
        std::cerr << "Error: Device '" << device_name << "' not found" << std::endl;
        return -1;
    }
    
    DeviceInfo* device = it->second.get();
    
    if (device->state == DeviceState::OPENED) {
        std::cerr << "Error: Device '" << device_name << "' already open" << std::endl;
        return -1;
    }
    
    if (device->state != DeviceState::INITIALIZED) {
        std::cerr << "Error: Device '" << device_name << "' not in initialized state" << std::endl;
        return -1;
    }
    
    // Generate file descriptor
    int fd = next_file_descriptor.fetch_add(1);
    
    // Update device state
    device->state = DeviceState::OPENED;
    fd_to_device_map[fd] = device_name;
    
    // Initialize device memory if not already done
    if (device_memory.find(fd) == device_memory.end()) {
        device_memory[fd] = std::vector<uint8_t>(device->memory_size, 0);
    }
    
    std::cout << "Device '" << device_name << "' opened (fd=" << fd << ")" << std::endl;
    return fd;
}

bool VirtualDeviceDriver::closeDevice(int device_fd) {
    std::lock_guard<std::mutex> lock(driver_mutex);
    
    if (!isValidFileDescriptor(device_fd)) {
        std::cerr << "Error: Invalid file descriptor " << device_fd << std::endl;
        return false;
    }
    
    std::string device_name = fd_to_device_map[device_fd];
    DeviceInfo* device = device_registry[device_name].get();
    
    // Disable IRQ if enabled
    if (irq_enabled.find(device_fd) != irq_enabled.end() && irq_enabled[device_fd]) {
        disableIRQ(device_fd);
    }
    
    // Update device state
    device->state = DeviceState::INITIALIZED;
    
    // Clean up mappings
    fd_to_device_map.erase(device_fd);
    device_memory.erase(device_fd);
    irq_handlers.erase(device_fd);
    irq_enabled.erase(device_fd);
    
    std::cout << "Device '" << device_name << "' closed" << std::endl;
    return true;
}

ssize_t VirtualDeviceDriver::readDevice(int device_fd, void* buffer, size_t count, off_t offset) {
    std::lock_guard<std::mutex> lock(driver_mutex);
    
    if (!isValidFileDescriptor(device_fd)) {
        return -1;
    }
    
    DeviceInfo* device = getDeviceByFd(device_fd);
    if (!device || device->state != DeviceState::OPENED) {
        return -1;
    }
    
    // Check bounds
    if (offset < 0 || static_cast<size_t>(offset) >= device->memory_size) {
        return -1;
    }
    
    // Calculate actual read size
    size_t available = device->memory_size - offset;
    size_t read_size = std::min(count, available);
    
    // Read from device memory
    const auto& memory = device_memory[device_fd];
    std::memcpy(buffer, memory.data() + offset, read_size);
    
    total_reads.fetch_add(1);
    
    return static_cast<ssize_t>(read_size);
}

ssize_t VirtualDeviceDriver::writeDevice(int device_fd, const void* buffer, size_t count, off_t offset) {
    std::lock_guard<std::mutex> lock(driver_mutex);
    
    if (!isValidFileDescriptor(device_fd)) {
        return -1;
    }
    
    DeviceInfo* device = getDeviceByFd(device_fd);
    if (!device || device->state != DeviceState::OPENED) {
        return -1;
    }
    
    // Check bounds
    if (offset < 0 || static_cast<size_t>(offset) >= device->memory_size) {
        return -1;
    }
    
    // Calculate actual write size
    size_t available = device->memory_size - offset;
    size_t write_size = std::min(count, available);
    
    // Write to device memory
    auto& memory = device_memory[device_fd];
    std::memcpy(memory.data() + offset, buffer, write_size);
    
    total_writes.fetch_add(1);
    
    return static_cast<ssize_t>(write_size);
}

int VirtualDeviceDriver::ioctlDevice(int device_fd, unsigned int cmd, void* arg) {
    std::lock_guard<std::mutex> lock(driver_mutex);
    
    if (!isValidFileDescriptor(device_fd)) {
        return -1;
    }
    
    DeviceInfo* device = getDeviceByFd(device_fd);
    if (!device || device->state != DeviceState::OPENED) {
        return -1;
    }
    
    total_ioctls.fetch_add(1);
    
    // Handle IOCTL commands
    switch (cmd) {
        case IOCTL_GET_INFO: {
            if (arg) {
                std::memcpy(arg, device, sizeof(DeviceInfo));
                return 0;
            }
            return -1;
        }
        
        case IOCTL_GET_STATUS: {
            if (arg) {
                auto state = static_cast<uint32_t>(device->state);
                std::memcpy(arg, &state, sizeof(uint32_t));
                return 0;
            }
            return -1;
        }
        
        case IOCTL_RESET: {
            // Reset device memory
            auto& memory = device_memory[device_fd];
            std::fill(memory.begin(), memory.end(), 0);
            
            std::cout << "Device '" << device->name << "' reset" << std::endl;
            return 0;
        }
        
        case IOCTL_ENABLE_IRQ: {
            if (device->supports_irq) {
                irq_enabled[device_fd] = true;
                std::cout << "IRQ enabled for device '" << device->name << "'" << std::endl;
                return 0;
            }
            return -1;
        }
        
        case IOCTL_DISABLE_IRQ: {
            irq_enabled[device_fd] = false;
            std::cout << "IRQ disabled for device '" << device->name << "'" << std::endl;
            return 0;
        }
        
        default:
            std::cerr << "Error: Unknown IOCTL command " << std::hex << cmd << std::endl;
            return -1;
    }
}

bool VirtualDeviceDriver::enableIRQ(int device_fd, IRQHandler handler) {
    std::lock_guard<std::mutex> lock(driver_mutex);
    
    if (!isValidFileDescriptor(device_fd)) {
        return false;
    }
    
    DeviceInfo* device = getDeviceByFd(device_fd);
    if (!device || !device->supports_irq) {
        return false;
    }
    
    irq_handlers[device_fd] = handler;
    irq_enabled[device_fd] = true;
    
    std::cout << "IRQ handler registered for device '" << device->name << "'" << std::endl;
    return true;
}

bool VirtualDeviceDriver::disableIRQ(int device_fd) {
    std::lock_guard<std::mutex> lock(driver_mutex);
    
    irq_enabled[device_fd] = false;
    irq_handlers.erase(device_fd);
    
    return true;
}

bool VirtualDeviceDriver::triggerIRQ(int device_fd, uint32_t irq_flags) {
    std::lock_guard<std::mutex> lock(driver_mutex);
    
    if (!isValidFileDescriptor(device_fd)) {
        return false;
    }
    
    if (irq_enabled.find(device_fd) == irq_enabled.end() || !irq_enabled[device_fd]) {
        return false;
    }
    
    auto handler_it = irq_handlers.find(device_fd);
    if (handler_it != irq_handlers.end()) {
        total_irqs.fetch_add(1);
        
        // Call IRQ handler in separate thread to simulate interrupt context
        std::thread([handler = handler_it->second, device_fd, irq_flags]() {
            try {
                handler(device_fd, irq_flags);
            } catch (const std::exception& e) {
                std::cerr << "Error in IRQ handler: " << e.what() << std::endl;
            }
        }).detach();
        
        return true;
    }
    
    return false;
}

std::vector<VirtualDeviceDriver::DeviceInfo> VirtualDeviceDriver::listDevices() const {
    std::lock_guard<std::mutex> lock(driver_mutex);
    
    std::vector<DeviceInfo> devices;
    devices.reserve(device_registry.size());
    
    for (const auto& [name, device_info] : device_registry) {
        devices.push_back(*device_info);
    }
    
    return devices;
}

void VirtualDeviceDriver::printDeviceList() const {
    std::lock_guard<std::mutex> lock(driver_mutex);
    
    std::cout << "\n=== Virtual Device Driver - Device List ===" << std::endl;
    std::cout << "Driver: " << driver_name << " v" << driver_version << std::endl;
    std::cout << "Devices registered: " << device_registry.size() << std::endl;
    std::cout << "-------------------------------------------" << std::endl;
    
    if (device_registry.empty()) {
        std::cout << "No devices registered." << std::endl;
    } else {
        for (const auto& [name, device_info] : device_registry) {
            std::cout << "Device: " << name << std::endl;
            std::cout << "  Type: " << deviceTypeToString(device_info->type) << std::endl;
            std::cout << "  State: " << deviceStateToString(device_info->state) << std::endl;
            std::cout << "  Major/Minor: " << device_info->major_number << "/" << device_info->minor_number << std::endl;
            std::cout << "  Memory Size: " << device_info->memory_size << " bytes" << std::endl;
            std::cout << "  Device File: " << device_info->device_file_path << std::endl;
            std::cout << "  Features: ";
            if (device_info->supports_mmap) std::cout << "mmap ";
            if (device_info->supports_irq) std::cout << "irq ";
            std::cout << std::endl << std::endl;
        }
    }
    
    std::cout << "Statistics:" << std::endl;
    std::cout << "  Total reads: " << total_reads.load() << std::endl;
    std::cout << "  Total writes: " << total_writes.load() << std::endl;
    std::cout << "  Total ioctls: " << total_ioctls.load() << std::endl;
    std::cout << "  Total IRQs: " << total_irqs.load() << std::endl;
    std::cout << "===========================================" << std::endl;
}

// Helper methods
bool VirtualDeviceDriver::createDeviceFile(const std::string& device_name) {
    std::string filepath = getDeviceFilePath(device_name);
    std::ofstream file(filepath);
    
    if (!file.is_open()) {
        return false;
    }
    
    // Write device file header
    file << "# Virtual Device File: " << device_name << std::endl;
    file << "# Created by Virtual Device Driver v" << driver_version << std::endl;
    file << "device_name=" << device_name << std::endl;
    file << "state=initialized" << std::endl;
    
    file.close();
    return true;
}

bool VirtualDeviceDriver::removeDeviceFile(const std::string& device_name) {
    std::string filepath = getDeviceFilePath(device_name);
    return (std::remove(filepath.c_str()) == 0);
}

std::string VirtualDeviceDriver::getDeviceFilePath(const std::string& device_name) const {
    return "device_files/" + device_name;
}

bool VirtualDeviceDriver::isValidFileDescriptor(int fd) const {
    return fd_to_device_map.find(fd) != fd_to_device_map.end();
}

VirtualDeviceDriver::DeviceInfo* VirtualDeviceDriver::getDeviceByFd(int fd) {
    auto it = fd_to_device_map.find(fd);
    if (it == fd_to_device_map.end()) {
        return nullptr;
    }
    
    auto device_it = device_registry.find(it->second);
    if (device_it == device_registry.end()) {
        return nullptr;
    }
    
    return device_it->second.get();
}

// Utility methods
std::string VirtualDeviceDriver::deviceTypeToString(DeviceType type) {
    switch (type) {
        case DeviceType::LED_DEVICE: return "LED";
        case DeviceType::BUTTON_DEVICE: return "BUTTON";
        case DeviceType::SENSOR_DEVICE: return "SENSOR";
        case DeviceType::UART_DEVICE: return "UART";
        case DeviceType::GENERIC_DEVICE: return "GENERIC";
        default: return "UNKNOWN";
    }
}

std::string VirtualDeviceDriver::deviceStateToString(DeviceState state) {
    switch (state) {
        case DeviceState::UNINITIALIZED: return "UNINITIALIZED";
        case DeviceState::INITIALIZED: return "INITIALIZED";
        case DeviceState::OPENED: return "OPENED";
        case DeviceState::BUSY: return "BUSY";
        case DeviceState::ERROR: return "ERROR";
        case DeviceState::SUSPENDED: return "SUSPENDED";
        default: return "UNKNOWN";
    }
}

VirtualDeviceDriver::DriverStatistics VirtualDeviceDriver::getStatistics() const {
    std::lock_guard<std::mutex> lock(driver_mutex);
    
    DriverStatistics stats;
    stats.devices_registered = device_registry.size();
    
    size_t opened_count = 0;
    for (const auto& [name, device_info] : device_registry) {
        if (device_info->state == DeviceState::OPENED) {
            opened_count++;
        }
    }
    stats.devices_opened = opened_count;
    
    stats.total_reads = total_reads.load();
    stats.total_writes = total_writes.load();
    stats.total_ioctls = total_ioctls.load();
    stats.total_irqs = total_irqs.load();
    stats.uptime = "Runtime"; // Could be calculated from load time
    
    return stats;
}