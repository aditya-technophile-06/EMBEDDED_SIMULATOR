#ifndef UART_H
#define UART_H

#include "sdk/peripheral.h"
#include <mutex>
#include <atomic>
#include <queue>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <functional>

/**
 * @brief UART Peripheral Class
 * 
 * Simulates a real UART peripheral with embedded systems features:
 * - Configurable baud rates, data bits, parity, stop bits
 * - Hardware flow control (RTS/CTS) simulation
 * - Interrupt-driven TX/RX with FIFOs
 * - Error detection (framing, parity, overrun)
 * - RS-232 and RS-485 mode simulation
 * - Loop-back testing mode
 * - DMA simulation for high-speed transfers
 */
class UART : public Peripheral {
public:
    enum class BaudRate {
        BAUD_9600 = 9600,
        BAUD_19200 = 19200,
        BAUD_38400 = 38400,
        BAUD_57600 = 57600,
        BAUD_115200 = 115200,
        BAUD_230400 = 230400,
        BAUD_460800 = 460800,
        BAUD_921600 = 921600
    };
    
    enum class DataBits {
        DATA_5 = 5,
        DATA_6 = 6,
        DATA_7 = 7,
        DATA_8 = 8,
        DATA_9 = 9
    };
    
    enum class Parity {
        NONE,
        EVEN,
        ODD,
        MARK,  // Always 1
        SPACE  // Always 0
    };
    
    enum class StopBits {
        ONE = 1,
        ONE_HALF = 15, // 1.5 bits (encoded as 15)
        TWO = 2
    };
    
    enum class FlowControl {
        NONE,
        RTS_CTS,  // Hardware flow control
        XON_XOFF  // Software flow control
    };
    
    enum class Mode {
        RS232,
        RS485_HALF_DUPLEX,
        RS485_FULL_DUPLEX,
        LOOPBACK
    };
    
    struct UARTConfig {
        BaudRate baud_rate;
        DataBits data_bits;
        Parity parity;
        StopBits stop_bits;
        FlowControl flow_control;
        Mode mode;
        size_t tx_fifo_size;
        size_t rx_fifo_size;
        bool enable_dma;
    };
    
    struct UARTStatus {
        bool tx_empty;
        bool tx_full;
        bool rx_empty;
        bool rx_full;
        bool framing_error;
        bool parity_error;
        bool overrun_error;
        bool break_detected;
        bool cts_state;
        bool rts_state;
    };
    
    // Callback function types
    using DataReceivedCallback = std::function<void(const std::vector<uint8_t>& data)>;
    using ErrorCallback = std::function<void(const std::string& error_type, const std::string& description)>;
    using StatusChangeCallback = std::function<void(const UARTStatus& status)>;
    
private:
    UARTConfig config;
    UARTStatus status;
    mutable std::mutex uart_mutex;
    
    // FIFOs for TX and RX
    std::queue<uint8_t> tx_fifo;
    std::queue<uint8_t> rx_fifo;
    std::atomic<size_t> tx_fifo_size;
    std::atomic<size_t> rx_fifo_size;
    
    // Threading for interrupt simulation
    std::thread tx_thread;
    std::thread rx_thread;
    std::atomic<bool> tx_running;
    std::atomic<bool> rx_running;
    std::condition_variable tx_cv;
    std::condition_variable rx_cv;
    
    // Callbacks
    DataReceivedCallback data_received_callback;
    ErrorCallback error_callback;
    StatusChangeCallback status_change_callback;
    
    // Flow control state
    std::atomic<bool> rts_state;
    std::atomic<bool> cts_state;
    std::atomic<bool> tx_enabled;
    
    // Statistics
    std::atomic<size_t> bytes_transmitted;
    std::atomic<size_t> bytes_received;
    std::atomic<size_t> transmission_errors;
    std::atomic<size_t> reception_errors;
    
    // DMA simulation
    std::atomic<bool> dma_tx_active;
    std::atomic<bool> dma_rx_active;
    
    // Internal helper methods
    std::string formatDeviceData() const;
    void transmissionLoop();
    void receptionLoop();
    bool calculateParity(uint8_t data) const;
    bool validateFraming(uint8_t data) const;
    void updateStatus();
    void triggerErrorCallback(const std::string& error_type, const std::string& description);
    uint32_t calculateTransmissionTime(size_t bytes) const; // in microseconds
    
    // Simulation methods
    void simulateRS485Direction(bool transmit);
    void simulateLoopback();
    
public:
    UART(const std::string& name);
    ~UART();
    
    // Inherited from Peripheral
    bool initialize() override;
    bool cleanup() override;
    std::string getStatus() const override;
    
    // Configuration
    bool configure(const UARTConfig& new_config);
    UARTConfig getConfiguration() const { return config; }
    
    bool setBaudRate(BaudRate rate);
    bool setDataFormat(DataBits data, Parity parity, StopBits stop);
    bool setFlowControl(FlowControl flow);
    bool setMode(Mode mode);
    bool enableDMA(bool enable);
    
    // Data transmission
    bool transmit(uint8_t byte);
    bool transmit(const std::vector<uint8_t>& data);
    bool transmit(const std::string& text);
    
    // Data reception
    bool receive(uint8_t& byte);
    std::vector<uint8_t> receive(size_t max_bytes = 0); // 0 = all available
    std::string receiveString(size_t max_chars = 0);
    
    // FIFO control
    bool clearTxFifo();
    bool clearRxFifo();
    size_t getTxFifoCount() const;
    size_t getRxFifoCount() const;
    bool isTxFifoFull() const;
    bool isRxFifoFull() const;
    bool isTxFifoEmpty() const;
    bool isRxFifoEmpty() const;
    
    // Flow control
    bool setRTS(bool state);
    bool getRTS() const { return rts_state.load(); }
    bool getCTS() const { return cts_state.load(); }
    
    // Status and errors
    UARTStatus getUARTStatus() const { return status; }
    bool hasErrors() const;
    void clearErrors();
    
    // Statistics
    struct Statistics {
        size_t bytes_tx;
        size_t bytes_rx;
        size_t errors_tx;
        size_t errors_rx;
        double throughput_bps;
        std::chrono::steady_clock::time_point start_time;
    };
    
    Statistics getStatistics() const;
    void resetStatistics();
    
    // Callbacks
    bool setDataReceivedCallback(DataReceivedCallback callback);
    bool setErrorCallback(ErrorCallback callback);
    bool setStatusChangeCallback(StatusChangeCallback callback);
    
    // Testing and debugging
    bool enableLoopback(bool enable);
    bool injectError(const std::string& error_type);
    bool sendBreak(uint32_t duration_ms = 10);
    
    // Hardware register simulation
    struct UARTRegisters {
        uint32_t control;      // Control register
        uint32_t status;       // Status register  
        uint32_t data;         // Data register
        uint32_t baud_rate;    // Baud rate register
        uint32_t fifo_control; // FIFO control register
        uint32_t interrupt;    // Interrupt register
        uint32_t dma_control;  // DMA control register
        uint32_t error_status; // Error status register
    };
    
    UARTRegisters getRegisters() const;
    bool setRegisters(const UARTRegisters& regs);
    
    // Utility methods
    static std::string baudRateToString(BaudRate rate);
    static std::string parityToString(Parity parity);
    static std::string flowControlToString(FlowControl flow);
    static std::string modeToString(Mode mode);
};

#endif // UART_H