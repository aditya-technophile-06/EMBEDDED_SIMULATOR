#ifndef TASK_H
#define TASK_H

#include <functional>
#include <string>
#include <chrono>
#include <atomic>
#include <memory>
#include <mutex>

/**
 * @brief RTOS Task Class
 * 
 * Simulates a real-time operating system task with features commonly found in embedded RTOS:
 * - Priority-based scheduling (0 = highest priority)
 * - Task states (Ready, Running, Blocked, Suspended, Terminated)
 * - Periodic and aperiodic task execution
 * - Task timing constraints (deadline, period, execution time)
 * - Stack monitoring and overflow detection
 * - Task communication via shared resources
 * - Context switching simulation
 */
class Task {
public:
    enum class State {
        READY,      // Task is ready to run
        RUNNING,    // Task is currently executing
        BLOCKED,    // Task is waiting for a resource
        SUSPENDED,  // Task is suspended by user/system
        TERMINATED, // Task has finished execution
        SLEEPING    // Task is sleeping for a specific time
    };
    
    enum class Priority {
        IDLE = 255,     // Lowest priority (idle task)
        VERY_LOW = 200,
        LOW = 150,
        NORMAL = 100,
        HIGH = 50,
        VERY_HIGH = 25,
        CRITICAL = 10,
        INTERRUPT = 0   // Highest priority (interrupt tasks)
    };
    
    enum class TaskType {
        PERIODIC,   // Runs at regular intervals
        APERIODIC,  // Runs when triggered
        SPORADIC,   // Runs irregularly but with minimum inter-arrival time
        ONE_SHOT    // Runs once and terminates
    };
    
    struct TaskTiming {
        std::chrono::milliseconds period;           // Task period (for periodic tasks)
        std::chrono::milliseconds deadline;         // Task deadline
        std::chrono::milliseconds execution_time;   // Estimated execution time
        std::chrono::milliseconds worst_case_time;  // Worst-case execution time (WCET)
    };
    
    struct TaskStatistics {
        size_t executions_count;
        size_t missed_deadlines;
        size_t context_switches;
        std::chrono::microseconds total_execution_time;
        std::chrono::microseconds max_execution_time;
        std::chrono::microseconds min_execution_time;
        std::chrono::steady_clock::time_point creation_time;
        std::chrono::steady_clock::time_point last_execution;
    };
    
private:
    static std::atomic<int> next_task_id;
    
    int task_id;
    std::string name;
    Priority priority;
    std::atomic<State> current_state;
    TaskType task_type;
    
    // Task function and context
    std::function<void()> task_function;
    size_t stack_size;
    std::atomic<bool> stack_overflow_detected;
    
    // Timing information
    TaskTiming timing;
    std::chrono::steady_clock::time_point next_release_time;
    std::chrono::steady_clock::time_point deadline_time;
    std::chrono::steady_clock::time_point execution_start_time;
    
    // Task control
    std::atomic<bool> enabled;
    std::atomic<bool> delete_requested;
    
    // Statistics
    TaskStatistics statistics;
    
    // Synchronization
    mutable std::mutex task_mutex;
    
public:
    Task(const std::string& task_name, 
         Priority prio, 
         std::function<void()> func,
         TaskType type = TaskType::PERIODIC,
         const TaskTiming& timing_info = {std::chrono::milliseconds(1000), 
                                         std::chrono::milliseconds(1000), 
                                         std::chrono::milliseconds(10),
                                         std::chrono::milliseconds(50)},
         size_t stack_sz = 8192);
    
    ~Task() = default;
    
    // Task control
    void execute();
    bool isReadyToRun() const;
    void suspend();
    void resume();
    void terminate();
    void sleep(std::chrono::milliseconds duration);
    
    // Getters
    int getId() const { return task_id; }
    const std::string& getName() const { return name; }
    Priority getPriority() const { return priority; }
    State getState() const { return current_state.load(); }
    TaskType getTaskType() const { return task_type; }
    bool isEnabled() const { return enabled.load(); }
    bool isDeleteRequested() const { return delete_requested.load(); }
    
    // Setters
    bool setPriority(Priority new_priority);
    bool setPeriod(std::chrono::milliseconds new_period);
    bool setDeadline(std::chrono::milliseconds new_deadline);
    
    // Timing and scheduling
    std::chrono::steady_clock::time_point getNextReleaseTime() const { return next_release_time; }
    std::chrono::steady_clock::time_point getDeadlineTime() const { return deadline_time; }
    bool hasDeadlinePassed() const;
    void updateNextReleaseTime();
    
    // Stack monitoring
    bool checkStackOverflow() const { return stack_overflow_detected.load(); }
    size_t getStackSize() const { return stack_size; }
    
    // Statistics
    const TaskStatistics& getStatistics() const { return statistics; }
    void resetStatistics();
    double getAverageExecutionTime() const;
    double getCPUUtilization(std::chrono::milliseconds window) const;
    
    // Comparison operators for priority queue
    bool operator<(const Task& other) const {
        // Higher priority number = lower priority (reverse comparison for priority queue)
        return static_cast<int>(priority) > static_cast<int>(other.priority);
    }
    
    // Utility methods
    std::string stateToString() const;
    std::string priorityToString() const;
    std::string taskTypeToString() const;
    
    static std::string stateToString(State state);
    static std::string priorityToString(Priority priority);
    static std::string taskTypeToString(TaskType type);
    
    // Friends for scheduler access
    friend class RTOSScheduler;
    
private:
    void setState(State new_state);
    void recordExecutionStart();
    void recordExecutionEnd();
    void incrementContextSwitches();
    void checkDeadlineMiss();
};

#endif // TASK_H