#ifndef BUTTON_H
#define BUTTON_H

#include "sdk/peripheral.h"
#include <mutex>
#include <atomic>
#include <functional>
#include <thread>
#include <condition_variable>
#include <chrono>

/**
 * @brief Button Peripheral Class
 * 
 * Simulates a real button peripheral with embedded systems features:
 * - Interrupt-style callback system
 * - Debouncing (critical for real buttons)
 * - Press/Release detection
 * - Long press detection
 * - Pull-up/Pull-down simulation
 * - Thread-safe interrupt handling
 */
class Button : public Peripheral {
public:
    enum class State {
        RELEASED = 0,
        PRESSED = 1
    };
    
    enum class EdgeType {
        RISING,   // Press event
        FALLING,  // Release event
        BOTH      // Both press and release
    };
    
    enum class PullMode {
        PULLUP,   // Button connects to ground when pressed
        PULLDOWN, // Button connects to VCC when pressed
        NONE      // No internal pull resistor
    };
    
    // Interrupt callback function type
    using InterruptCallback = std::function<void(State current_state, std::chrono::milliseconds press_duration)>;
    
private:
    std::atomic<State> current_state;
    std::atomic<State> last_state;
    mutable std::mutex button_mutex;
    
    // Interrupt simulation
    InterruptCallback interrupt_callback;
    std::atomic<bool> interrupt_enabled;
    EdgeType edge_trigger;
    
    // Debouncing
    std::atomic<int> debounce_time_ms;
    std::chrono::steady_clock::time_point last_change_time;
    
    // Long press detection
    std::atomic<int> long_press_threshold_ms;
    std::chrono::steady_clock::time_point press_start_time;
    std::atomic<bool> long_press_detected;
    
    // Pull mode configuration
    PullMode pull_mode;
    
    // Background thread for simulation
    std::thread simulation_thread;
    std::atomic<bool> simulation_running;
    std::condition_variable simulation_cv;
    
    // Helper methods
    std::string formatDeviceData() const;
    void simulationLoop();
    bool isDebounced() const;
    void triggerInterrupt();
    
public:
    Button(const std::string& name, PullMode mode = PullMode::PULLUP);
    ~Button();
    
    // Inherited from Peripheral
    bool initialize() override;
    bool cleanup() override;
    std::string getStatus() const override;
    
    // Button configuration
    bool setPullMode(PullMode mode);
    PullMode getPullMode() const { return pull_mode; }
    
    bool setDebounceTime(int ms);
    int getDebounceTime() const { return debounce_time_ms.load(); }
    
    bool setLongPressThreshold(int ms);
    int getLongPressThreshold() const { return long_press_threshold_ms.load(); }
    
    // Interrupt configuration
    bool enableInterrupt(EdgeType edge, InterruptCallback callback);
    bool disableInterrupt();
    bool isInterruptEnabled() const { return interrupt_enabled.load(); }
    
    // State queries
    State getState() const { return current_state.load(); }
    bool isPressed() const { return current_state.load() == State::PRESSED; }
    bool isReleased() const { return current_state.load() == State::RELEASED; }
    bool wasLongPress() const { return long_press_detected.load(); }
    
    // Manual simulation methods (for testing)
    bool simulatePress();
    bool simulateRelease();
    bool simulateClick(); // Quick press and release
    
    // Hardware register simulation
    struct ButtonRegisters {
        uint8_t status;     // Current state and flags
        uint8_t config;     // Pull mode and interrupt config
        uint8_t debounce;   // Debounce time
        uint8_t threshold;  // Long press threshold
    };
    
    ButtonRegisters getRegisters() const;
    bool setRegisters(const ButtonRegisters& regs);
};

#endif // BUTTON_H