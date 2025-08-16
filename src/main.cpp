#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <signal.h>

// SDK Headers
#include "sdk/peripheral.h"
#include "sdk/led.h"
#include "sdk/button.h"
#include "sdk/sensor.h"
#include "sdk/uart.h"

// Driver Headers
#include "drivers/virtual_device.h"

// RTOS Headers
#include "rtos/task.h"

/**
 * @brief Comprehensive Embedded Systems Simulator Demo
 * 
 * This demo showcases ALL the embedded systems concepts from your resume:
 * 1. Virtual SDK (C++ classes for peripherals)
 * 2. Device Driver simulation (/dev/virtualdevice)
 * 3. RTOS Task scheduling with priorities
 * 4. Interrupt handling simulation
 * 5. Real-time constraints and timing
 * 6. Multithreading and synchronization
 * 7. Protocol software (simulated)
 */

class EmbeddedSystemDemo {
private:
    // Hardware simulation
    std::unique_ptr<LED> status_led;
    std::unique_ptr<LED> activity_led;
    std::unique_ptr<Button> user_button;
    std::unique_ptr<Sensor> temperature_sensor;
    std::unique_ptr<Sensor> pressure_sensor;
    std::unique_ptr<UART> debug_uart;
    
    // Device driver
    std::unique_ptr<VirtualDeviceDriver> device_driver;
    
    // RTOS Tasks
    std::vector<std::unique_ptr<Task>> system_tasks;
    
    // System control
    std::atomic<bool> system_running;
    std::atomic<bool> emergency_stop;
    
    // Statistics
    std::atomic<size_t> led_blinks;
    std::atomic<size_t> button_presses;
    std::atomic<size_t> sensor_readings;
    
public:
    EmbeddedSystemDemo() : system_running(false), emergency_stop(false),
                          led_blinks(0), button_presses(0), sensor_readings(0) {}
    
    bool initialize() {
        std::cout << "\n=== EMBEDDED SYSTEMS SIMULATOR DEMO ===" << std::endl;
        std::cout << "Initializing virtual embedded system..." << std::endl;
        
        // 1. Initialize Device Driver (Linux character device simulation)
        std::cout << "\n[1] Loading Virtual Device Driver..." << std::endl;
        device_driver = std::make_unique<VirtualDeviceDriver>();
        if (!device_driver->loadDriver()) {
            std::cerr << "Failed to load device driver!" << std::endl;
            return false;
        }
        
        // Register devices with driver
        device_driver->registerDevice("status_led", VirtualDeviceDriver::DeviceType::LED_DEVICE);
        device_driver->registerDevice("activity_led", VirtualDeviceDriver::DeviceType::LED_DEVICE);
        device_driver->registerDevice("user_button", VirtualDeviceDriver::DeviceType::BUTTON_DEVICE);
        device_driver->registerDevice("temp_sensor", VirtualDeviceDriver::DeviceType::SENSOR_DEVICE);
        device_driver->registerDevice("debug_uart", VirtualDeviceDriver::DeviceType::UART_DEVICE);
        
        // 2. Initialize SDK Peripherals
        std::cout << "\n[2] Initializing SDK Peripherals..." << std::endl;
        
        status_led = std::make_unique<LED>("status_led");
        activity_led = std::make_unique<LED>("activity_led");
        user_button = std::make_unique<Button>("user_button");
        temperature_sensor = std::make_unique<Sensor>("temp_sensor", Sensor::SensorType::TEMPERATURE);
        pressure_sensor = std::make_unique<Sensor>("pressure_sensor", Sensor::SensorType::PRESSURE);
        debug_uart = std::make_unique<UART>("debug_uart");
        
        if (!status_led->initialize() || !activity_led->initialize() ||
            !user_button->initialize() || !temperature_sensor->initialize() ||
            !pressure_sensor->initialize() || !debug_uart->initialize()) {
            std::cerr << "Failed to initialize peripherals!" << std::endl;
            return false;
        }
        
        // 3. Configure Sensors
        std::cout << "\n[3] Configuring Sensors..." << std::endl;
        temperature_sensor->setSamplingRate(5); // 5 Hz
        temperature_sensor->setFilter(Sensor::FilterType::MOVING_AVERAGE, 10);
        temperature_sensor->setThresholds(-10.0f, 50.0f);
        
        pressure_sensor->setSamplingRate(2); // 2 Hz
        pressure_sensor->setFilter(Sensor::FilterType::LOW_PASS, 5);
        pressure_sensor->setThresholds(90.0f, 120.0f);
        
        // 4. Setup Button Interrupt Handler
        std::cout << "\n[4] Configuring Interrupt Handlers..." << std::endl;
        user_button->enableInterrupt(Button::EdgeType::FALLING, 
            [this](Button::State state, std::chrono::milliseconds duration) {
                button_presses.fetch_add(1);
                std::cout << "INTERRUPT: Button " << (state == Button::State::PRESSED ? "PRESSED" : "RELEASED")
                          << " (duration: " << duration.count() << "ms)" << std::endl;
                
                // Toggle activity LED on button press
                if (state == Button::State::RELEASED) {
                    activity_led->toggle();
                }
                
                // Emergency stop on long press
                if (duration.count() > 3000) {
                    emergency_stop = true;
                    std::cout << "EMERGENCY STOP TRIGGERED!" << std::endl;
                }
            });
        
        // 5. Setup Sensor Alert Callbacks
        temperature_sensor->enableAlerts([this](float value, const std::string& message) {
            std::cout << "SENSOR ALERT: " << message << " (Value: " << value << "°C)" << std::endl;
            status_led->startBlinking(200); // Fast blink on alert
        });
        
        pressure_sensor->enableAlerts([this](float value, const std::string& message) {
            std::cout << "SENSOR ALERT: " << message << " (Value: " << value << " kPa)" << std::endl;
            status_led->startBlinking(100); // Very fast blink on pressure alert
        });
        
        // 6. Create RTOS Tasks
        std::cout << "\n[5] Creating RTOS Tasks..." << std::endl;
        createRTOSTasks();
        
        std::cout << "\nSystem initialization complete!" << std::endl;
        return true;
    }
    
    void createRTOSTasks() {
        // Task 1: Status LED Heartbeat (High Priority, Periodic)
        auto heartbeat_task = std::make_unique<Task>(
            "heartbeat",
            Task::Priority::HIGH,
            [this]() {
                status_led->toggle();
                led_blinks.fetch_add(1);
            },
            Task::TaskType::PERIODIC,
            Task::TaskTiming{
                std::chrono::milliseconds(1000), // 1 second period
                std::chrono::milliseconds(50),   // 50ms deadline
                std::chrono::milliseconds(5),    // 5ms execution time
                std::chrono::milliseconds(10)    // 10ms worst case
            }
        );
        
        // Task 2: Sensor Data Collection (Normal Priority, Periodic)
        auto sensor_task = std::make_unique<Task>(
            "sensor_collection",
            Task::Priority::NORMAL,
            [this]() {
                float temp_raw, temp_cal, press_raw, press_cal;
                
                if (temperature_sensor->readSingle(temp_raw, temp_cal)) {
                    sensor_readings.fetch_add(1);
                }
                
                if (pressure_sensor->readSingle(press_raw, press_cal)) {
                    sensor_readings.fetch_add(1);
                }
                
                // Send data via UART (simulated)
                std::string data = "TEMP:" + std::to_string(temp_cal) + 
                                  ",PRESS:" + std::to_string(press_cal) + "\n";
                debug_uart->transmit(data);
            },
            Task::TaskType::PERIODIC,
            Task::TaskTiming{
                std::chrono::milliseconds(500),  // 500ms period
                std::chrono::milliseconds(100),  // 100ms deadline
                std::chrono::milliseconds(20),   // 20ms execution time
                std::chrono::milliseconds(40)    // 40ms worst case
            }
        );
        
        // Task 3: System Monitoring (Low Priority, Periodic)
        auto monitor_task = std::make_unique<Task>(
            "system_monitor",
            Task::Priority::LOW,
            [this]() {
                static int counter = 0;
                counter++;
                
                if (counter % 10 == 0) { // Every 10 executions (20 seconds)
                    printSystemStatus();
                }
                
                // Check for emergency conditions
                if (emergency_stop.load()) {
                    std::cout << "EMERGENCY STOP - Shutting down system!" << std::endl;
                    shutdown();
                }
            },
            Task::TaskType::PERIODIC,
            Task::TaskTiming{
                std::chrono::milliseconds(2000), // 2 second period
                std::chrono::milliseconds(500),  // 500ms deadline
                std::chrono::milliseconds(50),   // 50ms execution time
                std::chrono::milliseconds(100)   // 100ms worst case
            }
        );
        
        // Task 4: Activity LED Blinker (Normal Priority, Periodic)
        auto activity_task = std::make_unique<Task>(
            "activity_blink",
            Task::Priority::NORMAL,
            [this]() {
                activity_led->toggle();
            },
            Task::TaskType::PERIODIC,
            Task::TaskTiming{
                std::chrono::milliseconds(250),  // 250ms period (4 Hz)
                std::chrono::milliseconds(10),   // 10ms deadline
                std::chrono::milliseconds(2),    // 2ms execution time
                std::chrono::milliseconds(5)     // 5ms worst case
            }
        );
        
        system_tasks.push_back(std::move(heartbeat_task));
        system_tasks.push_back(std::move(sensor_task));
        system_tasks.push_back(std::move(monitor_task));
        system_tasks.push_back(std::move(activity_task));
        
        std::cout << "Created " << system_tasks.size() << " RTOS tasks" << std::endl;
    }
    
    void run() {
        std::cout << "\n=== STARTING EMBEDDED SYSTEM DEMO ===" << std::endl;
        std::cout << "System is now running..." << std::endl;
        std::cout << "Press Ctrl+C to stop or simulate button press for interaction" << std::endl;
        
        system_running = true;
        
        // Start sensor sampling
        temperature_sensor->startSampling();
        pressure_sensor->startSampling();
        
        // Simple task scheduler (priority-based)
        std::vector<std::thread> task_threads;
        
        for (auto& task : system_tasks) {
            task_threads.emplace_back([&task, this]() {
                while (system_running.load() && !emergency_stop.load()) {
                    if (task->isReadyToRun()) {
                        task->execute();
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            });
        }
        
        // Simulate button presses for demo
        std::thread button_simulator([this]() {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            while (system_running.load() && !emergency_stop.load()) {
                // Simulate button press every 8-12 seconds
                std::this_thread::sleep_for(std::chrono::seconds(10));
                
                if (system_running.load()) {
                    std::cout << "\n[SIMULATION] Simulating button press..." << std::endl;
                    user_button->simulateClick();
                }
            }
        });
        
        // Main loop - wait for shutdown signal
        while (system_running.load() && !emergency_stop.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Clean shutdown
        std::cout << "\nShutting down system..." << std::endl;
        system_running = false;
        
        // Join all threads
        for (auto& thread : task_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        if (button_simulator.joinable()) {
            button_simulator.join();
        }
        
        shutdown();
    }
    
    void shutdown() {
        std::cout << "\n=== SYSTEM SHUTDOWN ===" << std::endl;
        
        // Stop sensors
        temperature_sensor->stopSampling();
        pressure_sensor->stopSampling();
        
        // Turn off LEDs
        status_led->turnOff();
        activity_led->turnOff();
        
        // Print final statistics
        printFinalStatistics();
        
        // Cleanup peripherals
        status_led->cleanup();
        activity_led->cleanup();
        user_button->cleanup();
        temperature_sensor->cleanup();
        pressure_sensor->cleanup();
        debug_uart->cleanup();
        
        // Unload driver
        device_driver->unloadDriver();
        
        std::cout << "System shutdown complete." << std::endl;
    }
    
    void printSystemStatus() {
        std::cout << "\n--- SYSTEM STATUS ---" << std::endl;
        std::cout << "LED Blinks: " << led_blinks.load() << std::endl;
        std::cout << "Button Presses: " << button_presses.load() << std::endl;
        std::cout << "Sensor Readings: " << sensor_readings.load() << std::endl;
        
        auto temp_stats = temperature_sensor->getStatistics();
        auto press_stats = pressure_sensor->getStatistics();
        
        std::cout << "Temperature: " << temp_stats.avg_val << "°C (samples: " << temp_stats.count << ")" << std::endl;
        std::cout << "Pressure: " << press_stats.avg_val << " kPa (samples: " << press_stats.count << ")" << std::endl;
        
        std::cout << "Device Driver Stats:" << std::endl;
        auto driver_stats = device_driver->getStatistics();
        std::cout << "  Devices: " << driver_stats.devices_registered << std::endl;
        std::cout << "  Reads: " << driver_stats.total_reads << std::endl;
        std::cout << "  Writes: " << driver_stats.total_writes << std::endl;
        std::cout << "  IOCTLs: " << driver_stats.total_ioctls << std::endl;
        std::cout << "--------------------\n" << std::endl;
    }
    
    void printFinalStatistics() {
        std::cout << "\n=== FINAL SYSTEM STATISTICS ===" << std::endl;
        
        std::cout << "\nPeripheral Statistics:" << std::endl;
        std::cout << "  Total LED blinks: " << led_blinks.load() << std::endl;
        std::cout << "  Total button presses: " << button_presses.load() << std::endl;
        std::cout << "  Total sensor readings: " << sensor_readings.load() << std::endl;
        
        std::cout << "\nTask Statistics:" << std::endl;
        for (const auto& task : system_tasks) {
            const auto& stats = task->getStatistics();
            std::cout << "  " << task->getName() << ":" << std::endl;
            std::cout << "    Executions: " << stats.executions_count << std::endl;
            std::cout << "    Missed Deadlines: " << stats.missed_deadlines << std::endl;
            std::cout << "    Context Switches: " << stats.context_switches << std::endl;
            std::cout << "    Avg Execution Time: " << task->getAverageExecutionTime() << " μs" << std::endl;
            std::cout << "    CPU Utilization: " << task->getCPUUtilization(std::chrono::seconds(1)) << "%" << std::endl;
        }
        
        std::cout << "\nSensor Statistics:" << std::endl;
        auto temp_stats = temperature_sensor->getStatistics();
        auto press_stats = pressure_sensor->getStatistics();
        
        std::cout << "  Temperature Sensor:" << std::endl;
        std::cout << "    Samples: " << temp_stats.count << std::endl;
        std::cout << "    Range: [" << temp_stats.min_val << " - " << temp_stats.max_val << "] °C" << std::endl;
        std::cout << "    Average: " << temp_stats.avg_val << " °C" << std::endl;
        
        std::cout << "  Pressure Sensor:" << std::endl;
        std::cout << "    Samples: " << press_stats.count << std::endl;
        std::cout << "    Range: [" << press_stats.min_val << " - " << press_stats.max_val << "] kPa" << std::endl;
        std::cout << "    Average: " << press_stats.avg_val << " kPa" << std::endl;
        
        auto driver_stats = device_driver->getStatistics();
        std::cout << "\nDevice Driver Statistics:" << std::endl;
        std::cout << "  Devices Registered: " << driver_stats.devices_registered << std::endl;
        std::cout << "  Total Reads: " << driver_stats.total_reads << std::endl;
        std::cout << "  Total Writes: " << driver_stats.total_writes << std::endl;
        std::cout << "  Total IOCTLs: " << driver_stats.total_ioctls << std::endl;
        std::cout << "  Total IRQs: " << driver_stats.total_irqs << std::endl;
        
        std::cout << "\n===============================" << std::endl;
    }
};

// Global demo instance for signal handling
std::unique_ptr<EmbeddedSystemDemo> g_demo;

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ". Shutting down gracefully..." << std::endl;
    if (g_demo) {
        g_demo->shutdown();
    }
    exit(0);
}

int main() {
    // Setup signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        g_demo = std::make_unique<EmbeddedSystemDemo>();
        
        if (!g_demo->initialize()) {
            std::cerr << "Failed to initialize embedded system demo!" << std::endl;
            return 1;
        }
        
        g_demo->run();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}