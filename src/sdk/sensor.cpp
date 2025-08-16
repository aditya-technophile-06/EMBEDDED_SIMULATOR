#include "sdk/sensor.h"
#include <iostream>
#include <sstream>
#include <random>
#include <algorithm>
#include <numeric>
#include <cmath>

Sensor::Sensor(const std::string& name, SensorType type)
    : Peripheral(name),
      sensor_type(type),
      sampling_enabled(false),
      sampling_rate_hz(10), // Default 10 Hz
      adc_resolution(12),   // Default 12-bit ADC
      buffer_size(1000),    // Default 1000 samples
      buffer_index(0),
      filter_type(FilterType::NONE),
      filter_window_size(5),
      calibration_offset(0.0f),
      calibration_scale(1.0f),
      high_threshold(1000.0f),
      low_threshold(-1000.0f),
      alerts_enabled(false),
      sampling_running(false),
      min_value(std::numeric_limits<float>::max()),
      max_value(std::numeric_limits<float>::lowest()),
      avg_value(0.0f),
      sample_count(0) {
    
    data_buffer.resize(buffer_size.load());
    filter_buffer.reserve(filter_window_size.load());
}

Sensor::~Sensor() {
    if (initialized) {
        cleanup();
    }
}

bool Sensor::initialize() {
    std::lock_guard<std::mutex> lock(sensor_mutex);
    
    // Reset all values
    sampling_enabled = false;
    buffer_index = 0;
    data_buffer.clear();
    data_buffer.resize(buffer_size.load());
    filter_buffer.clear();
    
    // Reset statistics
    min_value = std::numeric_limits<float>::max();
    max_value = std::numeric_limits<float>::lowest();
    avg_value = 0.0f;
    sample_count = 0;
    
    // Create device file with initial state
    if (!writeToDeviceFile(formatDeviceData())) {
        std::cerr << "Error: Failed to initialize Sensor device file" << std::endl;
        return false;
    }
    
    initialized = true;
    std::cout << "Sensor '" << device_name << "' (" << sensorTypeToString() 
              << ") initialized successfully" << std::endl;
    return true;
}

bool Sensor::cleanup() {
    std::lock_guard<std::mutex> lock(sensor_mutex);
    
    // Stop sampling
    stopSampling();
    
    // Clear buffers
    data_buffer.clear();
    filter_buffer.clear();
    
    // Disable alerts
    alerts_enabled = false;
    alert_callback = nullptr;
    
    // Update device file
    writeToDeviceFile(formatDeviceData());
    
    initialized = false;
    std::cout << "Sensor '" << device_name << "' cleaned up" << std::endl;
    return true;
}

std::string Sensor::getStatus() const {
    std::lock_guard<std::mutex> lock(sensor_mutex);
    std::stringstream ss;
    
    ss << "Sensor '" << device_name << "' (" << sensorTypeToString() << ") - ";
    ss << "Sampling: " << (sampling_enabled.load() ? "ON" : "OFF") << ", ";
    ss << "Rate: " << sampling_rate_hz.load() << "Hz, ";
    ss << "ADC: " << adc_resolution.load() << "-bit, ";
    ss << "Filter: " << filterTypeToString(filter_type) << ", ";
    ss << "Samples: " << sample_count.load() << "/" << buffer_size.load();
    
    if (sample_count.load() > 0) {
        ss << ", Range: [" << min_value.load() << " - " << max_value.load() << "]";
        ss << ", Avg: " << avg_value.load();
    }
    
    if (alerts_enabled.load()) {
        ss << ", Alerts: ENABLED [" << low_threshold.load() 
           << " - " << high_threshold.load() << "]";
    }
    
    return ss.str();
}

bool Sensor::setSensorType(SensorType type) {
    std::lock_guard<std::mutex> lock(sensor_mutex);
    if (!initialized) {
        std::cerr << "Error: Sensor not initialized" << std::endl;
        return false;
    }
    
    if (sampling_enabled.load()) {
        std::cerr << "Error: Cannot change sensor type while sampling" << std::endl;
        return false;
    }
    
    sensor_type = type;
    
    // Reset calibration for new sensor type
    calibration_offset = 0.0f;
    calibration_scale = 1.0f;
    
    // Set appropriate thresholds based on sensor type
    switch (type) {
        case SensorType::TEMPERATURE:
            low_threshold = -50.0f;
            high_threshold = 100.0f;
            break;
        case SensorType::PRESSURE:
            low_threshold = 0.0f;
            high_threshold = 1200.0f;
            break;
        case SensorType::HUMIDITY:
            low_threshold = 0.0f;
            high_threshold = 100.0f;
            break;
        case SensorType::ACCELEROMETER:
            low_threshold = -2.5f;
            high_threshold = 2.5f;
            break;
        case SensorType::LIGHT:
            low_threshold = 0.0f;
            high_threshold = 70000.0f;
            break;
        case SensorType::VOLTAGE:
            low_threshold = 0.0f;
            high_threshold = 3.5f;
            break;
    }
    
    writeToDeviceFile(formatDeviceData());
    return true;
}

bool Sensor::setSamplingRate(int hz) {
    if (hz <= 0 || hz > 10000) {
        std::cerr << "Error: Sampling rate must be between 1-10000 Hz" << std::endl;
        return false;
    }
    
    sampling_rate_hz = hz;
    return true;
}

bool Sensor::setADCResolution(int bits) {
    if (bits < 8 || bits > 16) {
        std::cerr << "Error: ADC resolution must be between 8-16 bits" << std::endl;
        return false;
    }
    
    adc_resolution = bits;
    return true;
}

bool Sensor::setBufferSize(size_t size) {
    std::lock_guard<std::mutex> lock(sensor_mutex);
    if (!initialized) {
        std::cerr << "Error: Sensor not initialized" << std::endl;
        return false;
    }
    
    if (size < 10 || size > 100000) {
        std::cerr << "Error: Buffer size must be between 10-100000 samples" << std::endl;
        return false;
    }
    
    buffer_size = size;
    data_buffer.resize(size);
    buffer_index = 0;
    
    return true;
}

bool Sensor::setFilter(FilterType type, int window_size) {
    if (window_size < 1 || window_size > 100) {
        std::cerr << "Error: Filter window size must be between 1-100" << std::endl;
        return false;
    }
    
    std::lock_guard<std::mutex> lock(sensor_mutex);
    
    filter_type = type;
    filter_window_size = window_size;
    filter_buffer.clear();
    filter_buffer.reserve(window_size);
    
    return true;
}

bool Sensor::setCalibration(float offset, float scale) {
    if (scale == 0.0f) {
        std::cerr << "Error: Calibration scale cannot be zero" << std::endl;
        return false;
    }
    
    calibration_offset = offset;
    calibration_scale = scale;
    
    std::cout << "Sensor '" << device_name << "' calibration set: offset=" 
              << offset << ", scale=" << scale << std::endl;
    
    return true;
}

bool Sensor::setThresholds(float low, float high) {
    if (low >= high) {
        std::cerr << "Error: Low threshold must be less than high threshold" << std::endl;
        return false;
    }
    
    low_threshold = low;
    high_threshold = high;
    
    return true;
}

bool Sensor::enableAlerts(AlertCallback callback) {
    std::lock_guard<std::mutex> lock(sensor_mutex);
    if (!initialized) {
        std::cerr << "Error: Sensor not initialized" << std::endl;
        return false;
    }
    
    if (!callback) {
        std::cerr << "Error: Invalid callback function" << std::endl;
        return false;
    }
    
    alert_callback = callback;
    alerts_enabled = true;
    
    std::cout << "Sensor '" << device_name << "' alerts enabled" << std::endl;
    return true;
}

bool Sensor::disableAlerts() {
    std::lock_guard<std::mutex> lock(sensor_mutex);
    
    alerts_enabled = false;
    alert_callback = nullptr;
    
    std::cout << "Sensor '" << device_name << "' alerts disabled" << std::endl;
    return true;
}

bool Sensor::startSampling() {
    std::lock_guard<std::mutex> lock(sensor_mutex);
    if (!initialized) {
        std::cerr << "Error: Sensor not initialized" << std::endl;
        return false;
    }
    
    if (sampling_enabled.load()) {
        return true; // Already sampling
    }
    
    sampling_enabled = true;
    sampling_running = true;
    
    // Start sampling thread
    sampling_thread = std::thread(&Sensor::samplingLoop, this);
    
    std::cout << "Sensor '" << device_name << "' started sampling at " 
              << sampling_rate_hz.load() << "Hz" << std::endl;
    
    return true;
}

bool Sensor::stopSampling() {
    std::lock_guard<std::mutex> lock(sensor_mutex);
    
    if (!sampling_enabled.load()) {
        return true; // Already stopped
    }
    
    sampling_enabled = false;
    sampling_running = false;
    sampling_cv.notify_all();
    
    if (sampling_thread.joinable()) {
        sampling_thread.join();
    }
    
    std::cout << "Sensor '" << device_name << "' stopped sampling" << std::endl;
    return true;
}

bool Sensor::readLatestSample(SensorData& data) const {
    std::lock_guard<std::mutex> lock(sensor_mutex);
    if (!initialized || sample_count.load() == 0) {
        return false;
    }
    
    size_t latest_index = (buffer_index.load() + buffer_size.load() - 1) % buffer_size.load();
    data = data_buffer[latest_index];
    return true;
}

std::vector<Sensor::SensorData> Sensor::readBuffer(size_t num_samples) const {
    std::lock_guard<std::mutex> lock(sensor_mutex);
    std::vector<SensorData> result;
    
    if (!initialized || sample_count.load() == 0) {
        return result;
    }
    
    size_t samples_to_read = (num_samples == 0) ? 
        std::min(sample_count.load(), buffer_size.load()) : 
        std::min(num_samples, sample_count.load());
    
    result.reserve(samples_to_read);
    
    size_t start_index = (buffer_index.load() + buffer_size.load() - samples_to_read) % buffer_size.load();
    
    for (size_t i = 0; i < samples_to_read; ++i) {
        size_t idx = (start_index + i) % buffer_size.load();
        result.push_back(data_buffer[idx]);
    }
    
    return result;
}

bool Sensor::clearBuffer() {
    std::lock_guard<std::mutex> lock(sensor_mutex);
    if (!initialized) {
        return false;
    }
    
    buffer_index = 0;
    sample_count = 0;
    
    // Reset statistics
    min_value = std::numeric_limits<float>::max();
    max_value = std::numeric_limits<float>::lowest();
    avg_value = 0.0f;
    
    return true;
}

bool Sensor::readSingle(float& raw_value, float& calibrated_value) {
    std::lock_guard<std::mutex> lock(sensor_mutex);
    if (!initialized) {
        std::cerr << "Error: Sensor not initialized" << std::endl;
        return false;
    }
    
    raw_value = generateRawValue();
    calibrated_value = applyCalibration(raw_value);
    
    return true;
}

void Sensor::samplingLoop() {
    auto next_sample_time = std::chrono::steady_clock::now();
    auto sample_interval = std::chrono::microseconds(1000000 / sampling_rate_hz.load());
    
    while (sampling_running.load()) {
        // Generate sample
        float raw_value = generateRawValue();
        float filtered_value = applyFilter(raw_value);
        float calibrated_value = applyCalibration(filtered_value);
        
        // Create sensor data
        SensorData sample;
        sample.timestamp = std::chrono::steady_clock::now();
        sample.raw_value = raw_value;
        sample.calibrated_value = calibrated_value;
        sample.threshold_exceeded = checkThresholds(calibrated_value);
        
        // Store in buffer
        {
            std::lock_guard<std::mutex> lock(sensor_mutex);
            data_buffer[buffer_index.load()] = sample;
            buffer_index = (buffer_index.load() + 1) % buffer_size.load();
            sample_count = sample_count.load() + 1;
            
            updateStatistics(calibrated_value);
            writeToDeviceFile(formatDeviceData());
        }
        
        // Check for alerts
        if (sample.threshold_exceeded && alerts_enabled.load() && alert_callback) {
            std::string message = "Sensor '" + device_name + "' threshold exceeded: " + 
                                std::to_string(calibrated_value);
            
            // Call alert callback in separate thread
            std::thread([this, calibrated_value, message]() {
                try {
                    alert_callback(calibrated_value, message);
                } catch (const std::exception& e) {
                    std::cerr << "Error in sensor alert callback: " << e.what() << std::endl;
                }
            }).detach();
        }
        
        // Sleep until next sample
        next_sample_time += sample_interval;
        std::this_thread::sleep_until(next_sample_time);
    }
}

float Sensor::generateRawValue() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    // Generate realistic sensor data based on type
    switch (sensor_type) {
        case SensorType::TEMPERATURE: {
            std::normal_distribution<float> dis(22.0f, 5.0f); // Room temp with variation
            return std::clamp(dis(gen), -40.0f, 85.0f);
        }
        case SensorType::PRESSURE: {
            std::normal_distribution<float> dis(101.3f, 2.0f); // Sea level pressure
            return std::clamp(dis(gen), 0.0f, 1200.0f);
        }
        case SensorType::HUMIDITY: {
            std::normal_distribution<float> dis(45.0f, 10.0f); // Typical indoor humidity
            return std::clamp(dis(gen), 0.0f, 100.0f);
        }
        case SensorType::ACCELEROMETER: {
            std::normal_distribution<float> dis(0.0f, 0.1f); // Slight vibration
            return std::clamp(dis(gen), -2.0f, 2.0f);
        }
        case SensorType::LIGHT: {
            std::uniform_real_distribution<float> dis(100.0f, 1000.0f); // Indoor lighting
            return dis(gen);
        }
        case SensorType::VOLTAGE: {
            std::normal_distribution<float> dis(3.3f, 0.05f); // 3.3V rail with noise
            return std::clamp(dis(gen), 0.0f, 3.6f);
        }
        default:
            return 0.0f;
    }
}

float Sensor::applyCalibration(float raw_value) const {
    return (raw_value + calibration_offset.load()) * calibration_scale.load();
}

float Sensor::applyFilter(float value) {
    if (filter_type == FilterType::NONE) {
        return value;
    }
    
    filter_buffer.push_back(value);
    if (filter_buffer.size() > static_cast<size_t>(filter_window_size.load())) {
        filter_buffer.erase(filter_buffer.begin());
    }
    
    switch (filter_type) {
        case FilterType::MOVING_AVERAGE: {
            float sum = std::accumulate(filter_buffer.begin(), filter_buffer.end(), 0.0f);
            return sum / filter_buffer.size();
        }
        case FilterType::LOW_PASS: {
            // Simple low-pass filter (exponential moving average)
            static float alpha = 0.1f;
            static float previous_output = value;
            previous_output = alpha * value + (1.0f - alpha) * previous_output;
            return previous_output;
        }
        case FilterType::HIGH_PASS: {
            // Simple high-pass filter
            static float previous_input = value;
            static float previous_output = 0.0f;
            static float alpha = 0.9f;
            float output = alpha * (previous_output + value - previous_input);
            previous_input = value;
            previous_output = output;
            return output;
        }
        default:
            return value;
    }
}

bool Sensor::checkThresholds(float value) {
    return (value < low_threshold.load()) || (value > high_threshold.load());
}

void Sensor::updateStatistics(float value) {
    // Update min/max
    float current_min = min_value.load();
    while (value < current_min && !min_value.compare_exchange_weak(current_min, value)) {}
    
    float current_max = max_value.load();
    while (value > current_max && !max_value.compare_exchange_weak(current_max, value)) {}
    
    // Update running average
    size_t count = sample_count.load();
    float current_avg = avg_value.load();
    float new_avg = (current_avg * (count - 1) + value) / count;
    avg_value = new_avg;
}

std::string Sensor::formatDeviceData() const {
    std::stringstream ss;
    ss << "type:" << static_cast<int>(sensor_type) << ",";
    ss << "sampling:" << (sampling_enabled.load() ? 1 : 0) << ",";
    ss << "rate:" << sampling_rate_hz.load() << ",";
    ss << "resolution:" << adc_resolution.load() << ",";
    ss << "samples:" << sample_count.load() << ",";
    ss << "min:" << min_value.load() << ",";
    ss << "max:" << max_value.load() << ",";
    ss << "avg:" << avg_value.load();
    return ss.str();
}

std::string Sensor::sensorTypeToString() const {
    return sensorTypeToString(sensor_type);
}

std::string Sensor::sensorTypeToString(SensorType type) {
    switch (type) {
        case SensorType::TEMPERATURE: return "Temperature";
        case SensorType::PRESSURE: return "Pressure";
        case SensorType::HUMIDITY: return "Humidity";
        case SensorType::ACCELEROMETER: return "Accelerometer";
        case SensorType::LIGHT: return "Light";
        case SensorType::VOLTAGE: return "Voltage";
        default: return "Unknown";
    }
}

std::string Sensor::filterTypeToString(FilterType type) {
    switch (type) {
        case FilterType::NONE: return "None";
        case FilterType::MOVING_AVERAGE: return "Moving Average";
        case FilterType::LOW_PASS: return "Low Pass";
        case FilterType::HIGH_PASS: return "High Pass";
        default: return "Unknown";
    }
}

Sensor::Statistics Sensor::getStatistics() const {
    std::lock_guard<std::mutex> lock(sensor_mutex);
    Statistics stats;
    
    stats.min_val = min_value.load();
    stats.max_val = max_value.load();
    stats.avg_val = avg_value.load();
    stats.count = sample_count.load();
    
    // Calculate standard deviation
    if (stats.count > 1) {
        auto buffer_data = readBuffer();
        float sum_squared_diff = 0.0f;
        
        for (const auto& sample : buffer_data) {
            float diff = sample.calibrated_value - stats.avg_val;
            sum_squared_diff += diff * diff;
        }
        
        stats.std_deviation = std::sqrt(sum_squared_diff / (stats.count - 1));
    } else {
        stats.std_deviation = 0.0f;
    }
    
    return stats;
}

bool Sensor::resetStatistics() {
    std::lock_guard<std::mutex> lock(sensor_mutex);
    
    min_value = std::numeric_limits<float>::max();
    max_value = std::numeric_limits<float>::lowest();
    avg_value = 0.0f;
    sample_count = 0;
    
    return true;
}

Sensor::SensorRegisters Sensor::getRegisters() const {
    std::lock_guard<std::mutex> lock(sensor_mutex);
    SensorRegisters regs;
    
    // Control register
    regs.control = (sampling_enabled.load() ? 0x01 : 0x00) |
                   (static_cast<uint16_t>(sensor_type) << 1) |
                   (static_cast<uint16_t>(filter_type) << 4);
    
    // Status register
    regs.status = (initialized ? 0x01 : 0x00) |
                  (alerts_enabled.load() ? 0x02 : 0x00) |
                  ((sample_count.load() > 0) ? 0x04 : 0x00);
    
    // Latest data (simulated 16-bit ADC)
    if (sample_count.load() > 0) {
        SensorData latest;
        if (readLatestSample(latest)) {
            uint32_t adc_value = static_cast<uint32_t>(latest.raw_value * ((1 << adc_resolution.load()) - 1));
            regs.data_high = static_cast<uint16_t>(adc_value >> 16);
            regs.data_low = static_cast<uint16_t>(adc_value & 0xFFFF);
        }
    }
    
    // Thresholds (scaled to 16-bit)
    regs.threshold_h = static_cast<uint16_t>(high_threshold.load() * 100);
    regs.threshold_l = static_cast<uint16_t>(low_threshold.load() * 100);
    
    // Configuration
    regs.config = static_cast<uint16_t>(sampling_rate_hz.load()) |
                  (static_cast<uint16_t>(adc_resolution.load()) << 8);
    
    // Calibration (scaled)
    regs.calibration = static_cast<uint16_t>(calibration_scale.load() * 1000) |
                       (static_cast<uint16_t>(calibration_offset.load() * 10) << 8);
    
    return regs;
}

bool Sensor::setRegisters(const SensorRegisters& regs) {
    std::lock_guard<std::mutex> lock(sensor_mutex);
    if (!initialized) {
        return false;
    }
    
    // Update configuration from registers
    sensor_type = static_cast<SensorType>((regs.control >> 1) & 0x07);
    filter_type = static_cast<FilterType>((regs.control >> 4) & 0x03);
    sampling_rate_hz = regs.config & 0xFF;
    adc_resolution = (regs.config >> 8) & 0xFF;
    
    // Update thresholds
    high_threshold = static_cast<float>(regs.threshold_h) / 100.0f;
    low_threshold = static_cast<float>(regs.threshold_l) / 100.0f;
    
    // Update calibration
    calibration_scale = static_cast<float>(regs.calibration & 0xFF) / 1000.0f;
    calibration_offset = static_cast<float>(regs.calibration >> 8) / 10.0f;
    
    alerts_enabled = (regs.status & 0x02) != 0;
    
    return writeToDeviceFile(formatDeviceData());
}