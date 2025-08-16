#ifndef PERIPHERAL_H
#define PERIPHERAL_H

#include <string>
#include <fstream>
#include <memory>
#include <chrono>

/**
 * @brief Base class for all virtual peripherals
 * 
 * This class simulates the common interface that embedded peripherals share:
 * - Initialization and cleanup
 * - Status reporting
 * - Device file management (simulating /dev/peripheral interaction)
 */
class Peripheral {
protected:
    std::string device_name;
    std::string device_file;
    bool initialized;
    std::chrono::steady_clock::time_point last_access;
    
    // Protected method for derived classes to write to device file
    bool writeToDeviceFile(const std::string& data);
    bool readFromDeviceFile(std::string& data);
    
public:
    Peripheral(const std::string& name);
    virtual ~Peripheral() = default;
    
    // Pure virtual methods that all peripherals must implement
    virtual bool initialize() = 0;
    virtual bool cleanup() = 0;
    virtual std::string getStatus() const = 0;
    
    // Common interface methods
    bool isInitialized() const { return initialized; }
    const std::string& getName() const { return device_name; }
    const std::string& getDeviceFile() const { return device_file; }
    
    // Update last access time (for debugging/monitoring)
    void updateLastAccess();
    std::chrono::steady_clock::time_point getLastAccess() const { return last_access; }
};

#endif // PERIPHERAL_H