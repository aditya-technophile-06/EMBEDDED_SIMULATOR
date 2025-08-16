#include "sdk/button.h"
#include <iostream>
#include <sstream>
#include <random>

Button::Button(const std::string& name, PullMode mode) 
    : Peripheral(name), 
      current_state(State::RELEASED), 
      last_state(State::RELEASED),
      interrupt_enabled(false),
      edge_trigger(EdgeType::BOTH),
      debounce_time_ms(50), // Default 50ms debounce
      long_press_threshold_ms(1000), // Default 1 second for long press
      long_press_detected(false),
      pull_mode(mode),
      simulation_running(false) {
    
    // Initialize based on pull mode
    if (pull_mode == PullMode::PULLUP) {
        current_state = State::RELEASED; // High when not pressed
    } else {
        current_state = State::RELEASED; // Start in released state
    }
    
    last_state = current_state.load();
    last_change_time = std::chrono::steady_clock::now();
}

Button::~Button() {
    if (initialized) {
        cleanup();
    }
}

bool Button::initialize() {
    std::lock_guard<std::mutex> lock(button_mutex);
    
    // Reset state
    current_state = State::RELEASED;
    last_state = State::RELEASED;
    long_press_detected = false;
    interrupt_enabled = false;
    
    // Start simulation thread
    simulation_running = true;
    simulation_thread = std::thread(&Button::simulationLoop, this);
    
    // Create device file with initial state
    if (!writeToDeviceFile(formatDeviceData())) {
        std::cerr << "Error: Failed to initialize Button device file" << std::endl;
        return false;
    }
    
    initialized = true;
    std::cout << "Button '" << device_name << "' initialized successfully" << std::endl;
    return true;
}

bool Button::cleanup() {
    std::lock_guard<std::mutex> lock(button_mutex);
    
    // Stop simulation thread
    simulation_running = false;
    simulation_cv.notify_all();
    
    if (simulation_thread.joinable()) {
        simulation_thread.join();
    }
    
    // Disable interrupts
    interrupt_enabled = false;
    interrupt_callback = nullptr;
    
    // Update device file
    writeToDeviceFile(formatDeviceData());
    
    initialized = false;
    std::cout << "Button '" << device_name << "' cleaned up" << std::endl;
    return true;
}

std::string Button::getStatus() const {
    std::lock_guard<std::mutex> lock(button_mutex);
    std::stringstream ss;
    
    ss << "Button '" << device_name << "' - ";
    ss << "State: " << (current_state.load() == State::PRESSED ? "PRESSED" : "RELEASED") << ", ";
    ss << "Pull Mode: ";
    
    switch (pull_mode) {
        case PullMode::PULLUP: ss << "PULLUP"; break;
        case PullMode::PULLDOWN: ss << "PULLDOWN"; break;
        case PullMode::NONE: ss << "NONE"; break;
    }
    
    ss << ", Debounce: " << debounce_time_ms.load() << "ms, ";
    ss << "Interrupt: " << (interrupt_enabled.load() ? "ENABLED" : "DISABLED");
    
    if (long_press_detected.load()) {
        ss << ", LONG PRESS DETECTED";
    }
    
    return ss.str();
}

bool Button::setPullMode(PullMode mode) {
    std::lock_guard<std::mutex> lock(button_mutex);
    if (!initialized) {
        std::cerr << "Error: Button not initialized" << std::endl;
        return false;
    }
    
    pull_mode = mode;
    writeToDeviceFile(formatDeviceData());
    return true;
}

bool Button::setDebounceTime(int ms) {
    if (ms < 0 || ms > 1000) {
        std::cerr << "Error: Debounce time must be between 0-1000ms" << std::endl;
        return false;
    }
    
    debounce_time_ms = ms;
    return true;
}

bool Button::setLongPressThreshold(int ms) {
    if (ms < 100) {
        std::cerr << "Error: Long press threshold must be at least 100ms" << std::endl;
        return false;
    }
    
    long_press_threshold_ms = ms;
    return true;
}

bool Button::enableInterrupt(EdgeType edge, InterruptCallback callback) {
    std::lock_guard<std::mutex> lock(button_mutex);
    if (!initialized) {
        std::cerr << "Error: Button not initialized" << std::endl;
        return false;
    }
    
    if (!callback) {
        std::cerr << "Error: Invalid callback function" << std::endl;
        return false;
    }
    
    edge_trigger = edge;
    interrupt_callback = callback;
    interrupt_enabled = true;
    
    std::cout << "Button '" << device_name << "' interrupt enabled" << std::endl;
    return true;
}

bool Button::disableInterrupt() {
    std::lock_guard<std::mutex> lock(button_mutex);
    
    interrupt_enabled = false;
    interrupt_callback = nullptr;
    
    std::cout << "Button '" << device_name << "' interrupt disabled" << std::endl;
    return true;
}

bool Button::simulatePress() {
    std::lock_guard<std::mutex> lock(button_mutex);
    if (!initialized) {
        std::cerr << "Error: Button not initialized" << std::endl;
        return false;
    }
    
    if (current_state.load() == State::PRESSED) {
        return true; // Already pressed
    }
    
    last_state = current_state.load();
    current_state = State::PRESSED;
    press_start_time = std::chrono::steady_clock::now();
    last_change_time = press_start_time;
    long_press_detected = false;
    
    // Update device file
    writeToDeviceFile(formatDeviceData());
    
    std::cout << "Button '" << device_name << "' simulated PRESS" << std::endl;
    
    // Trigger interrupt if enabled
    triggerInterrupt();
    
    return true;
}

bool Button::simulateRelease() {
    std::lock_guard<std::mutex> lock(button_mutex);
    if (!initialized) {
        std::cerr << "Error: Button not initialized" << std::endl;
        return false;
    }
    
    if (current_state.load() == State::RELEASED) {
        return true; // Already released
    }
    
    // Calculate press duration
    auto now = std::chrono::steady_clock::now();
    auto press_duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - press_start_time);
    
    // Check for long press
    if (press_duration.count() >= long_press_threshold_ms.load()) {
        long_press_detected = true;
        std::cout << "Button '" << device_name << "' LONG PRESS detected (" 
                  << press_duration.count() << "ms)" << std::endl;
    }
    
    last_state = current_state.load();
    current_state = State::RELEASED;
    last_change_time = now;
    
    // Update device file
    writeToDeviceFile(formatDeviceData());
    
    std::cout << "Button '" << device_name << "' simulated RELEASE" << std::endl;
    
    // Trigger interrupt if enabled
    triggerInterrupt();
    
    return true;
}

bool Button::simulateClick() {
    if (!simulatePress()) return false;
    
    // Short delay to simulate realistic click timing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    return simulateRelease();
}

void Button::simulationLoop() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(5000, 15000); // Random events every 5-15 seconds
    
    while (simulation_running.load()) {
        std::unique_lock<std::mutex> lock(button_mutex);
        
        // Wait for random interval or stop signal
        simulation_cv.wait_for(lock, std::chrono::milliseconds(dis(gen)), 
                             [this] { return !simulation_running.load(); });
        
        if (!simulation_running.load()) break;
        
        // Simulate occasional random button events (optional - disabled by default)
        // This could be enabled for testing interrupt handling
        
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

bool Button::isDebounced() const {
    auto now = std::chrono::steady_clock::now();
    auto time_since_change = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_change_time);
    return time_since_change.count() >= debounce_time_ms.load();
}

void Button::triggerInterrupt() {
    if (!interrupt_enabled.load() || !interrupt_callback) {
        return;
    }
    
    bool should_trigger = false;
    State current = current_state.load();
    State last = last_state.load();
    
    switch (edge_trigger) {
        case EdgeType::RISING:
            should_trigger = (last == State::RELEASED && current == State::PRESSED);
            break;
        case EdgeType::FALLING:
            should_trigger = (last == State::PRESSED && current == State::RELEASED);
            break;
        case EdgeType::BOTH:
            should_trigger = (current != last);
            break;
    }
    
    if (should_trigger && isDebounced()) {
        // Calculate press duration
        auto now = std::chrono::steady_clock::now();
        auto press_duration = (current == State::PRESSED) ? 
            std::chrono::milliseconds(0) : 
            std::chrono::duration_cast<std::chrono::milliseconds>(now - press_start_time);
        
        // Call interrupt callback in a separate thread to avoid blocking
        std::thread([this, current, press_duration]() {
            try {
                interrupt_callback(current, press_duration);
            } catch (const std::exception& e) {
                std::cerr << "Error in button interrupt callback: " << e.what() << std::endl;
            }
        }).detach();
        
        std::cout << "Button '" << device_name << "' triggered interrupt" << std::endl;
    }
}

std::string Button::formatDeviceData() const {
    std::stringstream ss;
    ss << "state:" << static_cast<int>(current_state.load()) << ",";
    ss << "pull_mode:" << static_cast<int>(pull_mode) << ",";
    ss << "debounce:" << debounce_time_ms.load() << ",";
    ss << "long_press_threshold:" << long_press_threshold_ms.load() << ",";
    ss << "interrupt_enabled:" << (interrupt_enabled.load() ? 1 : 0) << ",";
    ss << "long_press_detected:" << (long_press_detected.load() ? 1 : 0);
    return ss.str();
}

Button::ButtonRegisters Button::getRegisters() const {
    std::lock_guard<std::mutex> lock(button_mutex);
    ButtonRegisters regs;
    
    // Status register
    regs.status = static_cast<uint8_t>(current_state.load()) |
                  (long_press_detected.load() ? 0x02 : 0x00) |
                  (interrupt_enabled.load() ? 0x04 : 0x00);
    
    // Config register
    regs.config = static_cast<uint8_t>(pull_mode) |
                  (static_cast<uint8_t>(edge_trigger) << 2);
    
    // Debounce time (scaled to fit in 8 bits)
    regs.debounce = static_cast<uint8_t>(debounce_time_ms.load() / 10);
    
    // Long press threshold (scaled to fit in 8 bits)
    regs.threshold = static_cast<uint8_t>(long_press_threshold_ms.load() / 100);
    
    return regs;
}

bool Button::setRegisters(const ButtonRegisters& regs) {
    std::lock_guard<std::mutex> lock(button_mutex);
    if (!initialized) {
        return false;
    }
    
    // Update configuration from registers
    pull_mode = static_cast<PullMode>(regs.config & 0x03);
    edge_trigger = static_cast<EdgeType>((regs.config >> 2) & 0x03);
    debounce_time_ms = static_cast<int>(regs.debounce) * 10;
    long_press_threshold_ms = static_cast<int>(regs.threshold) * 100;
    interrupt_enabled = (regs.status & 0x04) != 0;
    
    return writeToDeviceFile(formatDeviceData());
}