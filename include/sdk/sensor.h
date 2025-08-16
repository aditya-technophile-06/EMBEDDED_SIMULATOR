#ifndef SENSOR_H
#define SENSOR_H

#include "sdk/peripheral.h"
#include <mutex>
#include <atomic>
#include <vector>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <functional>

/**
 * @brief Sensor Peripheral Class
 * 
 * Simulates a real sensor peripheral with embedded systems features:
 * - ADC simulation (Analog-to-Digital Conversion)
 * - Multiple sensor types (temperature, pressure, accelerometer, etc.)
 * - Configurable sampling rates and resolution
 * - Data filtering and calibration
 * - Threshold-based alerts/interrupts
 * - Ring buffer for data storage
 * - Statistical analysis (min, max, average)
 */
class Sensor : public Peripheral {
public:
    enum class SensorType {
        TEMPERATURE,   // Temperature sensor (-40°C to +85°C)
        PRESSURE,      // Pressure sensor (0-1000 kPa)
        HUMIDITY,      // Humidity sensor (0-100% RH)
        ACCELEROMETER, // 3-axis accelerometer (-2g to +2g)
        LIGHT,         // Light sensor (0-65535 lux)
        VOLTAGE        // Voltage sensor (0-3.3V)
    };
    
    enum class FilterType {
        NONE,          // No filtering
        MOVING_AVERAGE, // Simple moving average
        LOW_PASS,      // Low-pass filter
        HIGH_PASS      // High-pass filter
    };
    
    struct SensorData {
        std::chrono::steady_clock::time_point timestamp;
        float raw_value;        // Raw ADC value
        float calibrated_value; // Calibrated sensor value
        bool threshold_exceeded; // Alert flag
    };
    
    // Alert callback function type
    using AlertCallback = std::function<void(float value, const std::string& message)>;
    
private:
    SensorType sensor_type;
    std::atomic<bool> sampling_enabled;
    std::atomic<int> sampling_rate_hz;  // Samples per second
    std::atomic<int> adc_resolution;    // ADC resolution in bits (8, 10, 12, 16)
    
    mutable std::mutex sensor_mutex;
    
    // Data storage (ring buffer)
    std::vector<SensorData> data_buffer;
    std::atomic<size_t> buffer_size;
    std::atomic<size_t> buffer_index;
    
    // Filtering
    FilterType filter_type;
    std::atomic<int> filter_window_size;
    std::vector<float> filter_buffer;
    
    // Calibration
    std::atomic<float> calibration_offset;
    std::atomic<float> calibration_scale;
    
    // Thresholds and alerts
    std::atomic<float> high_threshold;
    std::atomic<float> low_threshold;
    std::atomic<bool> alerts_enabled;
    AlertCallback alert_callback;
    
    // Background sampling thread
    std::thread sampling_thread;
    std::atomic<bool> sampling_running;
    std::condition_variable sampling_cv;
    
    // Statistics
    mutable std::atomic<float> min_value;
    mutable std::atomic<float> max_value;
    mutable std::atomic<float> avg_value;
    mutable std::atomic<size_t> sample_count;
    
    // Helper methods
    std::string formatDeviceData() const;
    void samplingLoop();
    float generateRawValue() const;
    float applyCalibration(float raw_value) const;
    float applyFilter(float value);
    bool checkThresholds(float value);
    void updateStatistics(float value);
    std::string sensorTypeToString() const;
    
public:
    Sensor(const std::string& name, SensorType type);
    ~Sensor();
    
    // Inherited from Peripheral
    bool initialize() override;
    bool cleanup() override;
    std::string getStatus() const override;
    
    // Sensor configuration
    bool setSensorType(SensorType type);
    SensorType getSensorType() const { return sensor_type; }
    
    bool setSamplingRate(int hz);
    int getSamplingRate() const { return sampling_rate_hz.load(); }
    
    bool setADCResolution(int bits);
    int getADCResolution() const { return adc_resolution.load(); }
    
    bool setBufferSize(size_t size);
    size_t getBufferSize() const { return buffer_size.load(); }
    
    // Filtering configuration
    bool setFilter(FilterType type, int window_size = 5);
    FilterType getFilterType() const { return filter_type; }
    int getFilterWindowSize() const { return filter_window_size.load(); }
    
    // Calibration
    bool setCalibration(float offset, float scale);
    float getCalibrationOffset() const { return calibration_offset.load(); }
    float getCalibrationScale() const { return calibration_scale.load(); }
    
    // Threshold configuration
    bool setThresholds(float low, float high);
    float getLowThreshold() const { return low_threshold.load(); }
    float getHighThreshold() const { return high_threshold.load(); }
    
    bool enableAlerts(AlertCallback callback);
    bool disableAlerts();
    bool areAlertsEnabled() const { return alerts_enabled.load(); }
    
    // Sampling control
    bool startSampling();
    bool stopSampling();
    bool isSampling() const { return sampling_enabled.load(); }
    
    // Data access
    bool readLatestSample(SensorData& data) const;
    std::vector<SensorData> readBuffer(size_t num_samples = 0) const; // 0 = all
    bool clearBuffer();
    
    // Single sample (for manual reading)
    bool readSingle(float& raw_value, float& calibrated_value);
    
    // Statistics
    struct Statistics {
        float min_val;
        float max_val;
        float avg_val;
        size_t count;
        float std_deviation;
    };
    
    Statistics getStatistics() const;
    bool resetStatistics();
    
    // Hardware register simulation
    struct SensorRegisters {
        uint16_t control;      // Control register
        uint16_t status;       // Status register
        uint16_t data_high;    // Data register high
        uint16_t data_low;     // Data register low
        uint16_t threshold_h;  // High threshold
        uint16_t threshold_l;  // Low threshold
        uint16_t config;       // Configuration register
        uint16_t calibration;  // Calibration register
    };
    
    SensorRegisters getRegisters() const;
    bool setRegisters(const SensorRegisters& regs);
    
    // Utility methods
    static std::string sensorTypeToString(SensorType type);
    static std::string filterTypeToString(FilterType type);
};

#endif // SENSOR_H