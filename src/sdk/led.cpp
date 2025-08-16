#include "sdk/led.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstdint>

LED::LED(const std::string& name) 
    : Peripheral(name), state(false), brightness(0), is_blinking(false), blink_period_ms(1000) {
}

LED::~LED() {
    if (initialized) {
        cleanup();
    }
}

bool LED::initialize() {
    std::lock_guard<std::mutex> lock(led_mutex);
    
    // Initialize LED state to off
    state = false;
    brightness = 0;
    is_blinking = false;
    
    // Create device file with initial state
    if (!writeToDeviceFile(formatDeviceData())) {
        std::cerr << "Error: Failed to initialize LED device file" << std::endl;
        return false;
    }
    
    initialized = true;
    std::cout << "LED '" << device_name << "' initialized successfully" << std::endl;
    return true;
}

bool LED::cleanup() {
    std::lock_guard<std::mutex> lock(led_mutex);
    
    // Turn off LED and stop any blinking
    state = false;
    brightness = 0;
    is_blinking = false;
    
    // Update device file
    writeToDeviceFile(formatDeviceData());
    
    initialized = false;
    std::cout << "LED '" << device_name << "' cleaned up" << std::endl;
    return true;
}

std::string LED::getStatus() const {
    std::lock_guard<std::mutex> lock(led_mutex);
    std::stringstream ss;
    
    ss << "LED '" << device_name << "' - ";
    ss << "State: " << (state.load() ? "ON" : "OFF") << ", ";
    ss << "Brightness: " << brightness.load() << "%, ";
    ss << "Blinking: " << (is_blinking.load() ? "YES" : "NO");
    
    if (is_blinking.load()) {
        ss << " (Period: " << blink_period_ms.load() << "ms)";
    }
    
    return ss.str();
}

bool LED::turnOn() {
    std::lock_guard<std::mutex> lock(led_mutex);
    if (!initialized) {
        std::cerr << "Error: LED not initialized" << std::endl;
        return false;
    }
    
    state = true;
    // If brightness is 0, set to full brightness
    if (brightness.load() == 0) {
        brightness = 100;
    }
    
    // Stop blinking when manually controlled
    is_blinking = false;
    
    // Update device file
    bool result = writeToDeviceFile(formatDeviceData());
    
    if (result) {
        std::cout << "LED '" << device_name << "' turned ON (brightness: " 
                  << brightness.load() << "%)" << std::endl;
    }
    
    return result;
}

bool LED::turnOff() {
    std::lock_guard<std::mutex> lock(led_mutex);
    if (!initialized) {
        std::cerr << "Error: LED not initialized" << std::endl;
        return false;
    }
    
    state = false;
    is_blinking = false;
    
    // Update device file
    bool result = writeToDeviceFile(formatDeviceData());
    
    if (result) {
        std::cout << "LED '" << device_name << "' turned OFF" << std::endl;
    }
    
    return result;
}

bool LED::toggle() {
    return state.load() ? turnOff() : turnOn();
}

bool LED::setBrightness(int level) {
    std::lock_guard<std::mutex> lock(led_mutex);
    if (!initialized) {
        std::cerr << "Error: LED not initialized" << std::endl;
        return false;
    }
    
    if (level < 0 || level > 100) {
        std::cerr << "Error: Brightness must be between 0-100%" << std::endl;
        return false;
    }
    
    brightness = level;
    
    // If setting brightness > 0, turn LED on
    if (level > 0) {
        state = true;
    } else {
        state = false;
    }
    
    // Stop blinking when manually controlling brightness
    is_blinking = false;
    
    // Update device file
    bool result = writeToDeviceFile(formatDeviceData());
    
    if (result) {
        std::cout << "LED '" << device_name << "' brightness set to " 
                  << level << "%" << std::endl;
    }
    
    return result;
}

bool LED::startBlinking(int period_ms) {
    std::lock_guard<std::mutex> lock(led_mutex);
    if (!initialized) {
        std::cerr << "Error: LED not initialized" << std::endl;
        return false;
    }
    
    if (period_ms <= 0) {
        std::cerr << "Error: Blink period must be positive" << std::endl;
        return false;
    }
    
    blink_period_ms = period_ms;
    is_blinking = true;
    
    // Set default brightness if not set
    if (brightness.load() == 0) {
        brightness = 100;
    }
    
    std::cout << "LED '" << device_name << "' started blinking with period " 
              << period_ms << "ms" << std::endl;
    
    return writeToDeviceFile(formatDeviceData());
}

bool LED::stopBlinking() {
    std::lock_guard<std::mutex> lock(led_mutex);
    if (!initialized) {
        std::cerr << "Error: LED not initialized" << std::endl;
        return false;
    }
    
    is_blinking = false;
    state = false; // Turn off when stopping blink
    
    std::cout << "LED '" << device_name << "' stopped blinking" << std::endl;
    
    return writeToDeviceFile(formatDeviceData());
}

std::string LED::formatDeviceData() const {
    std::stringstream ss;
    ss << "state:" << (state.load() ? 1 : 0) << ",";
    ss << "brightness:" << brightness.load() << ",";
    ss << "blinking:" << (is_blinking.load() ? 1 : 0) << ",";
    ss << "blink_period:" << blink_period_ms.load();
    return ss.str();
}

LED::LEDRegisters LED::getRegisters() const {
    std::lock_guard<std::mutex> lock(led_mutex);
    LEDRegisters regs;
    
    // Simulate hardware register format
    regs.control = (state.load() ? 0x01 : 0x00) | 
                   (is_blinking.load() ? 0x02 : 0x00);
    regs.brightness = static_cast<uint8_t>(brightness.load() * 255 / 100);
    regs.status = initialized ? 0x01 : 0x00;
    
    return regs;
}

bool LED::setRegisters(const LEDRegisters& regs) {
    std::lock_guard<std::mutex> lock(led_mutex);
    if (!initialized) {
        return false;
    }
    
    // Update state from register values
    state = (regs.control & 0x01) != 0;
    is_blinking = (regs.control & 0x02) != 0;
    brightness = static_cast<int>(regs.brightness * 100 / 255);
    
    return writeToDeviceFile(formatDeviceData());
}