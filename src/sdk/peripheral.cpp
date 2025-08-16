#include "sdk/peripheral.h"
#include <iostream>
#include <fstream>
#include <sys/stat.h>

Peripheral::Peripheral(const std::string& name) 
    : device_name(name), initialized(false) {
    // Create device file path - simulates /dev/peripheral_name
    device_file = "device_files/" + name;
    updateLastAccess();
}

bool Peripheral::writeToDeviceFile(const std::string& data) {
    if (!initialized) {
        std::cerr << "Error: Peripheral " << device_name << " not initialized" << std::endl;
        return false;
    }
    
    std::ofstream file(device_file, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot write to device file " << device_file << std::endl;
        return false;
    }
    
    file << data << std::endl;
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