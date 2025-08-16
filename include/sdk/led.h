#ifndef LED_H
#define LED_H

#include "sdk/peripheral.h"
#include <mutex>
#include <atomic>

/**
 * @brief LED Peripheral Class
 * 
 * Simulates a real LED peripheral with features commonly found in embedded systems:
 * - On/Off control (GPIO simulation)
 * - PWM brightness control (0-100%)
 * - Thread-safe operations (critical in embedded systems)
 * - Blinking functionality with configurable timing
 * - State persistence (simulates hardware registers)
 */
class LED : public Peripheral {
private:
    std::atomic<bool> state;          // LED on/off state
    std::atomic<int> brightness;      // PWM duty cycle (0-100%)
    mutable std::mutex led_mutex;     // Thread safety for operations
    
    // Blinking functionality
    std::atomic<bool> is_blinking;
    std::atomic<int> blink_period_ms; // Blink period in milliseconds
    
    // Helper method to format device file data
    std::string formatDeviceData() const;
    
public:
    LED(const std::string& name);
    ~LED();
    
    // Inherited from Peripheral
    bool initialize() override;
    bool cleanup() override;
    std::string getStatus() const override;
    
    // LED specific operations (simulating embedded GPIO/PWM functions)
    bool turnOn();
    bool turnOff();
    bool toggle();
    
    // PWM brightness control (simulates hardware PWM)
    bool setBrightness(int level); // 0-100%
    int getBrightness() const { return brightness.load(); }
    
    // Advanced features
    bool startBlinking(int period_ms = 1000);
    bool stopBlinking();
    bool isBlinking() const { return is_blinking.load(); }
    
    // State queries
    bool getState() const { return state.load(); }
    
    // Simulated hardware register access
    struct LEDRegisters {
        uint8_t control;    // Control register
        uint8_t brightness; // Brightness register
        uint8_t status;     // Status register
    };
    
    LEDRegisters getRegisters() const;
    bool setRegisters(const LEDRegisters& regs);
};

#endif // LED_H