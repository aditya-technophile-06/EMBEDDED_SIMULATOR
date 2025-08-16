#include "sdk/uart.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <random>

UART::UART(const std::string& name)
    : Peripheral(name),
      tx_fifo_size(64),
      rx_fifo_size(64),
      tx_running(false),
      rx_running(false),
      rts_state(false),
      cts_state(true), // Default CTS active
      tx_enabled(true),
      bytes_transmitted(0),
      bytes_received(0),
      transmission_errors(0),
      reception_errors(0),
      dma_tx_active(false),
      dma_rx_active(false) {
    
    // Default configuration
    config.baud_rate = BaudRate::BAUD_115200;
    config.data_bits = DataBits::DATA_8;
    config.parity = Parity::NONE;
    config.stop_bits = StopBits::ONE;
    config.flow_control = FlowControl::NONE;
    config.mode = Mode::RS232;
    config.tx_fifo_size = 64;
    config.rx_fifo_size = 64;
    config.enable_dma = false;
    
    // Initialize status
    status = {};
    status.cts_state = true;
    status.tx_empty = true;
    status.rx_empty = true;
}

UART::~UART() {
    if (initialized) {
        cleanup();
    }
}

bool UART::initialize() {
    std::lock_guard<std::mutex> lock(uart_mutex);
    
    // Clear FIFOs
    while (!tx_fifo.empty()) tx_fifo.pop();
    while (!rx_fifo.empty()) rx_fifo.pop();
    
    // Reset statistics
    bytes_transmitted = 0;
    bytes_received = 0;
    transmission_errors = 0;
    reception_errors = 0;
    
    // Start threads
    tx_running = true;
    rx_running = true;
    tx_thread = std::thread(&UART::transmissionLoop, this);
    rx_thread = std::thread(&UART::receptionLoop, this);
    
    updateStatus();
    
    if (!writeToDeviceFile(formatDeviceData())) {
        std::cerr << "Error: Failed to initialize UART device file" << std::endl;
        return false;
    }
    
    initialized = true;
    std::cout << "UART '" << device_name << "' initialized at " 
              << static_cast<int>(config.baud_rate) << " baud" << std::endl;
    return true;
}

bool UART::cleanup() {
    std::lock_guard<std::mutex> lock(uart_mutex);
    
    // Stop threads
    tx_running = false;
    rx_running = false;
    tx_cv.notify_all();
    rx_cv.notify_all();
    
    if (tx_thread.joinable()) tx_thread.join();
    if (rx_thread.joinable()) rx_thread.join();
    
    // Clear callbacks
    data_received_callback = nullptr;
    error_callback = nullptr;
    status_change_callback = nullptr;
    
    // Clear FIFOs
    while (!tx_fifo.empty()) tx_fifo.pop();
    while (!rx_fifo.empty()) rx_fifo.pop();
    
    writeToDeviceFile(formatDeviceData());
    
    initialized = false;
    std::cout << "UART '" << device_name << "' cleaned up" << std::endl;
    return true;
}

std::string UART::getStatus() const {
    std::lock_guard<std::mutex> lock(uart_mutex);
    std::stringstream ss;
    
    ss << "UART '" << device_name << "' - ";
    ss << "Baud: " << static_cast<int>(config.baud_rate) << ", ";
    ss << "Config: " << static_cast<int>(config.data_bits) << parityToString(config.parity)[0] 
       << static_cast<int>(config.stop_bits) << ", ";
    ss << "Mode: " << modeToString(config.mode) << ", ";
    ss << "TX FIFO: " << tx_fifo.size() << "/" << config.tx_fifo_size << ", ";
    ss << "RX FIFO: " << rx_fifo.size() << "/" << config.rx_fifo_size;
    
    if (hasErrors()) {
        ss << " [ERRORS]";
    }
    
    return ss.str();
}

bool UART::configure(const UARTConfig& new_config) {
    std::lock_guard<std::mutex> lock(uart_mutex);
    if (!initialized) {
        std::cerr << "Error: UART not initialized" << std::endl;
        return false;
    }
    
    config = new_config;
    
    // Resize FIFOs if needed
    if (tx_fifo.size() > config.tx_fifo_size) {
        while (tx_fifo.size() > config.tx_fifo_size) {
            tx_fifo.pop();
        }
    }
    
    updateStatus();
    writeToDeviceFile(formatDeviceData());
    
    std::cout << "UART '" << device_name << "' reconfigured" << std::endl;
    return true;
}

bool UART::transmit(uint8_t byte) {
    std::lock_guard<std::mutex> lock(uart_mutex);
    if (!initialized || !tx_enabled.load()) {
        return false;
    }
    
    if (tx_fifo.size() >= config.tx_fifo_size) {
        // FIFO full
        status.tx_full = true;
        return false;
    }
    
    tx_fifo.push(byte);
    status.tx_empty = false;
    status.tx_full = (tx_fifo.size() >= config.tx_fifo_size);
    
    // Notify transmission thread
    tx_cv.notify_one();
    
    return true;
}

bool UART::transmit(const std::vector<uint8_t>& data) {
    bool success = true;
    for (uint8_t byte : data) {
        if (!transmit(byte)) {
            success = false;
            break;
        }
    }
    return success;
}

bool UART::transmit(const std::string& text) {
    std::vector<uint8_t> data(text.begin(), text.end());
    return transmit(data);
}

bool UART::receive(uint8_t& byte) {
    std::lock_guard<std::mutex> lock(uart_mutex);
    if (!initialized || rx_fifo.empty()) {
        return false;
    }
    
    byte = rx_fifo.front();
    rx_fifo.pop();
    
    status.rx_empty = rx_fifo.empty();
    status.rx_full = false;
    
    bytes_received = bytes_received.load() + 1;
    return true;
}

std::vector<uint8_t> UART::receive(size_t max_bytes) {
    std::lock_guard<std::mutex> lock(uart_mutex);
    std::vector<uint8_t> data;
    
    size_t bytes_to_read = (max_bytes == 0) ? rx_fifo.size() : 
                          std::min(max_bytes, rx_fifo.size());
    
    data.reserve(bytes_to_read);
    
    for (size_t i = 0; i < bytes_to_read; ++i) {
        data.push_back(rx_fifo.front());
        rx_fifo.pop();
        bytes_received = bytes_received.load() + 1;
    }
    
    status.rx_empty = rx_fifo.empty();
    status.rx_full = false;
    
    return data;
}

void UART::transmissionLoop() {
    while (tx_running.load()) {
        std::unique_lock<std::mutex> lock(uart_mutex);
        
        // Wait for data or stop signal
        tx_cv.wait(lock, [this] { return !tx_fifo.empty() || !tx_running.load(); });
        
        if (!tx_running.load()) break;
        
        if (!tx_fifo.empty() && tx_enabled.load()) {
            uint8_t byte = tx_fifo.front();
            tx_fifo.pop();
            
            // Simulate transmission time based on baud rate
            lock.unlock();
            
            uint32_t bit_time_us = 1000000 / static_cast<uint32_t>(config.baud_rate);
            uint32_t frame_bits = static_cast<uint32_t>(config.data_bits) + 1; // +1 for start bit
            
            if (config.parity != Parity::NONE) frame_bits++;
            frame_bits += static_cast<uint32_t>(config.stop_bits);
            
            std::this_thread::sleep_for(std::chrono::microseconds(bit_time_us * frame_bits));
            
            lock.lock();
            
            // Simulate potential transmission errors
            static int error_counter = 0;
            if (++error_counter % 10000 == 0) { // Very rare errors
                transmission_errors = transmission_errors.load() + 1;
                if (error_callback) {
                    std::thread([this]() {
                        error_callback("TRANSMISSION", "Simulated transmission error");
                    }).detach();
                }
            } else {
                bytes_transmitted = bytes_transmitted.load() + 1;
                
                // Loopback mode
                if (config.mode == Mode::LOOPBACK) {
                    if (rx_fifo.size() < config.rx_fifo_size) {
                        rx_fifo.push(byte);
                        if (data_received_callback) {
                            std::vector<uint8_t> data = {byte};
                            std::thread([this, data]() {
                                data_received_callback(data);
                            }).detach();
                        }
                    }
                }
            }
            
            updateStatus();
        }
    }
}

void UART::receptionLoop() {
    // Simulate incoming data for demo purposes
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 5000); // Random intervals
    
    while (rx_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
        
        if (!rx_running.load()) break;
        
        // Simulate occasional incoming data (optional for demo)
        if (config.mode != Mode::LOOPBACK) {
            std::lock_guard<std::mutex> lock(uart_mutex);
            
            // Only in specific demo scenarios
            // This would normally be driven by external hardware
        }
    }
}

void UART::updateStatus() {
    status.tx_empty = tx_fifo.empty();
    status.tx_full = (tx_fifo.size() >= config.tx_fifo_size);
    status.rx_empty = rx_fifo.empty();
    status.rx_full = (rx_fifo.size() >= config.rx_fifo_size);
    status.cts_state = cts_state.load();
    status.rts_state = rts_state.load();
}

std::string UART::formatDeviceData() const {
    std::stringstream ss;
    ss << "baud:" << static_cast<int>(config.baud_rate) << ",";
    ss << "data_bits:" << static_cast<int>(config.data_bits) << ",";
    ss << "parity:" << static_cast<int>(config.parity) << ",";
    ss << "tx_bytes:" << bytes_transmitted.load() << ",";
    ss << "rx_bytes:" << bytes_received.load() << ",";
    ss << "tx_fifo:" << tx_fifo.size() << ",";
    ss << "rx_fifo:" << rx_fifo.size();
    return ss.str();
}

bool UART::hasErrors() const {
    return status.framing_error || status.parity_error || 
           status.overrun_error || transmission_errors.load() > 0 || 
           reception_errors.load() > 0;
}

// Utility methods
std::string UART::baudRateToString(BaudRate rate) {
    return std::to_string(static_cast<int>(rate));
}

std::string UART::parityToString(Parity parity) {
    switch (parity) {
        case Parity::NONE: return "N";
        case Parity::EVEN: return "E";
        case Parity::ODD: return "O";
        case Parity::MARK: return "M";
        case Parity::SPACE: return "S";
        default: return "?";
    }
}

std::string UART::modeToString(Mode mode) {
    switch (mode) {
        case Mode::RS232: return "RS232";
        case Mode::RS485_HALF_DUPLEX: return "RS485-HD";
        case Mode::RS485_FULL_DUPLEX: return "RS485-FD";
        case Mode::LOOPBACK: return "LOOPBACK";
        default: return "UNKNOWN";
    }
}

// Simplified implementations for remaining methods
bool UART::setBaudRate(BaudRate rate) {
    config.baud_rate = rate;
    return true;
}

bool UART::clearTxFifo() {
    std::lock_guard<std::mutex> lock(uart_mutex);
    while (!tx_fifo.empty()) tx_fifo.pop();
    updateStatus();
    return true;
}

bool UART::clearRxFifo() {
    std::lock_guard<std::mutex> lock(uart_mutex);
    while (!rx_fifo.empty()) rx_fifo.pop();
    updateStatus();
    return true;
}

size_t UART::getTxFifoCount() const {
    std::lock_guard<std::mutex> lock(uart_mutex);
    return tx_fifo.size();
}

size_t UART::getRxFifoCount() const {
    std::lock_guard<std::mutex> lock(uart_mutex);
    return rx_fifo.size();
}

UART::Statistics UART::getStatistics() const {
    Statistics stats;
    stats.bytes_tx = bytes_transmitted.load();
    stats.bytes_rx = bytes_received.load();
    stats.errors_tx = transmission_errors.load();
    stats.errors_rx = reception_errors.load();
    stats.throughput_bps = static_cast<double>(config.baud_rate);
    stats.start_time = std::chrono::steady_clock::now();
    return stats;
}