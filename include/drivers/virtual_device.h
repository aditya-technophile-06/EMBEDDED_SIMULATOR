#ifndef VIRTUAL_DEVICE_H
#define VIRTUAL_DEVICE_H

#include <string>
#include <map>
#include <mutex>
#include <fstream>
#include <memory>
#include <functional>
#include <vector>
#include <atomic>

/**
 * @brief Virtual Device Driver Class
 * 
 * Simulates a real Linux character device driver with features commonly found in embedded systems:
 * - Device registration and management (/dev/virtualdevice simulation)
 * - IOCTL commands for device control
 * - Read/Write operations with proper error handling
 * - Interrupt handling simulation
 * - Memory mapping simulation (mmap)
 * - Device power management
 * - Sysfs attribute simulation
 * - Multiple device instance support
 */
class VirtualDeviceDriver {
public:
    // Device types supported by this driver
    enum class DeviceType {
        LED_DEVICE,
        BUTTON_DEVICE,
        SENSOR_DEVICE,
        UART_DEVICE,
        GENERIC_DEVICE
    };
    
    // Device states
    enum class DeviceState {
        UNINITIALIZED,
        INITIALIZED,
        OPENED,
        BUSY,
        ERROR,
        SUSPENDED
    };
    
    // IOCTL commands (simulating real ioctl values)
    enum IOCTLCommands {
        IOCTL_GET_INFO = 0x1000,
        IOCTL_SET_CONFIG = 0x1001,
        IOCTL_GET_STATUS = 0x1002,
        IOCTL_RESET = 0x1003,
        IOCTL_SET_POWER = 0x1004,
        IOCTL_GET_REGISTERS = 0x1005,
        IOCTL_SET_REGISTERS = 0x1006,
        IOCTL_ENABLE_IRQ = 0x1007,
        IOCTL_DISABLE_IRQ = 0x1008
    };
    
    // Device information structure
    struct DeviceInfo {
        std::string name;
        DeviceType type;
        DeviceState state;
        uint32_t major_number;
        uint32_t minor_number;
        size_t memory_size;
        bool supports_mmap;
        bool supports_irq;
        std::string driver_version;
        std::string device_file_path;
    };
    
    // Power management states
    enum class PowerState {
        ON,
        STANDBY,
        SUSPEND,
        OFF
    };
    
    // IRQ (Interrupt Request) simulation
    using IRQHandler = std::function<void(int device_fd, uint32_t irq_flags)>;
    
private:
    std::map<std::string, std::unique_ptr<DeviceInfo>> device_registry;
    std::map<int, std::string> fd_to_device_map; // File descriptor to device name mapping
    mutable std::mutex driver_mutex;
    bool driver_loaded;
    
    // Driver information
    std::string driver_name;
    std::string driver_version;
    uint32_t next_major_number;
    std::atomic<int> next_file_descriptor;
    
    // IRQ handling
    std::map<int, IRQHandler> irq_handlers;
    std::map<int, bool> irq_enabled;
    
    // Memory management simulation
    std::map<int, std::vector<uint8_t>> device_memory;
    
    // Statistics
    std::atomic<size_t> total_reads;
    std::atomic<size_t> total_writes;
    std::atomic<size_t> total_ioctls;
    std::atomic<size_t> total_irqs;
    
    // Helper methods
    bool createDeviceFile(const std::string& device_name);
    bool removeDeviceFile(const std::string& device_name);
    std::string getDeviceFilePath(const std::string& device_name) const;
    bool isValidFileDescriptor(int fd) const;
    DeviceInfo* getDeviceByFd(int fd);
    
public:
    VirtualDeviceDriver();
    ~VirtualDeviceDriver();
    
    // Driver lifecycle
    bool loadDriver();
    bool unloadDriver();
    bool isDriverLoaded() const { return driver_loaded; }
    
    // Device management
    bool registerDevice(const std::string& name, DeviceType type, size_t memory_size = 4096);
    bool unregisterDevice(const std::string& name);
    bool isDeviceRegistered(const std::string& name) const;
    
    // Device operations (simulating Linux device driver interface)
    int openDevice(const std::string& device_name, int flags = 0);
    bool closeDevice(int device_fd);
    
    ssize_t readDevice(int device_fd, void* buffer, size_t count, off_t offset = 0);
    ssize_t writeDevice(int device_fd, const void* buffer, size_t count, off_t offset = 0);
    
    // IOCTL operations (device control)
    int ioctlDevice(int device_fd, unsigned int cmd, void* arg);
    
    // Memory mapping simulation (mmap)
    void* mmapDevice(int device_fd, size_t length, off_t offset);
    bool munmapDevice(void* addr, size_t length);
    
    // Interrupt handling simulation
    bool enableIRQ(int device_fd, IRQHandler handler);
    bool disableIRQ(int device_fd);
    bool triggerIRQ(int device_fd, uint32_t irq_flags);
    
    // Power management
    bool setPowerState(const std::string& device_name, PowerState state);
    PowerState getPowerState(const std::string& device_name) const;
    
    // Device information and status
    std::vector<DeviceInfo> listDevices() const;
    DeviceInfo getDeviceInfo(const std::string& device_name) const;
    bool getDeviceStatus(const std::string& device_name, DeviceState& state) const;
    
    // Sysfs simulation (Linux device attributes)
    bool createSysfsAttribute(const std::string& device_name, const std::string& attr_name, const std::string& value);
    bool readSysfsAttribute(const std::string& device_name, const std::string& attr_name, std::string& value) const;
    bool writeSysfsAttribute(const std::string& device_name, const std::string& attr_name, const std::string& value);
    
    // Debug and monitoring
    std::string getDriverInfo() const;
    void printDeviceList() const;
    
    struct DriverStatistics {
        size_t devices_registered;
        size_t devices_opened;
        size_t total_reads;
        size_t total_writes;
        size_t total_ioctls;
        size_t total_irqs;
        std::string uptime;
    };
    
    DriverStatistics getStatistics() const;
    void resetStatistics();
    
    // Error handling
    enum class ErrorCode {
        SUCCESS = 0,
        DEVICE_NOT_FOUND = -1,
        DEVICE_BUSY = -2,
        INVALID_OPERATION = -3,
        PERMISSION_DENIED = -4,
        OUT_OF_MEMORY = -5,
        HARDWARE_ERROR = -6,
        TIMEOUT = -7
    };
    
    static std::string errorCodeToString(ErrorCode code);
    
    // Utility methods
    static std::string deviceTypeToString(DeviceType type);
    static std::string deviceStateToString(DeviceState state);
    static std::string powerStateToString(PowerState state);
};

#endif // VIRTUAL_DEVICE_H