#include "sdk/peripheral.h"
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <errno.h>

Peripheral::Peripheral(const std::string& name) 
    : device_name(name), initialized(false) {
    // Create device file path - simulates /dev/peripheral_name
    device_file = "device_files/" + name;
    
    // Create device_files directory if it doesn't exist
    struct stat st;
    if (stat("device_files", &st) == -1) {
        if (mkdir("device_files", 0755) != 0 && errno != EEXIST) {
            std::cerr << "Warning: Could not create device_files directory: " << strerror(errno) << std::endl;
        }
    }
    
    updateLastAccess();
}

bool Peripheral::writeToDeviceFile(const std::string& data) {
    // Ensure directory exists before writing
    struct stat st;
    if (stat("device_files", &st) == -1) {
        if (mkdir("device_files", 0755) != 0) {
            std::cerr << "Error: Cannot create device_files directory: " << strerror(errno) << std::endl;
            return false;
        }
    }
    
    std::ofstream file(device_file, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot write to device file " << device_file 
                  << " - " << strerror(errno) << std::endl;
        return false;
    }
    
    file << data << std::endl;
    if (file.fail()) {
        std::cerr << "Error: Failed to write data to device file " << device_file << std::endl;
        return false;
    }
    
    file.close();
    updateLastAccess();
    return true;
}

bool Peripheral::readFromDeviceFile(std::string& data) {
    if (!initialized) {
        std::cerr << "Error: Peripheral " << device_name << " not initialized" << std::endl;
        return false;
    }
    
    std::ifstream file(device_file);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot read from device file " << device_file << std::endl;
        return false;
    }
    
    std::getline(file, data);
    file.close();
    updateLastAccess();
    return true;
}

void Peripheral::updateLastAccess() {
    last_access = std::chrono::steady_clock::now();
}