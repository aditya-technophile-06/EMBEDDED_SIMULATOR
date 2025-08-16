#include "rtos/task.h"
#include <iostream>
#include <sstream>

std::atomic<int> Task::next_task_id{1};

Task::Task(const std::string& task_name, 
           Priority prio, 
           std::function<void()> func,
           TaskType type,
           const TaskTiming& timing_info,
           size_t stack_sz)
    : task_id(next_task_id.fetch_add(1)),
      name(task_name),
      priority(prio),
      current_state(State::READY),
      task_type(type),
      task_function(func),
      stack_size(stack_sz),
      stack_overflow_detected(false),
      timing(timing_info),
      enabled(true),
      delete_requested(false) {
    
    // Initialize timing
    auto now = std::chrono::steady_clock::now();
    next_release_time = now;
    deadline_time = now + timing.deadline;
    
    // Initialize statistics
    statistics = {};
    statistics.creation_time = now;
    statistics.last_execution = now;
    
    std::cout << "Task '" << name << "' (ID: " << task_id << ", Priority: " 
              << static_cast<int>(priority) << ") created" << std::endl;
}

void Task::execute() {
    std::lock_guard<std::mutex> lock(task_mutex);
    
    if (!enabled.load() || current_state.load() != State::READY) {
        return;
    }
    
    recordExecutionStart();
    setState(State::RUNNING);
    
    try {
        // Execute the task function
        if (task_function) {
            task_function();
        }
        
        statistics.executions_count++;
        
        // Check for deadline miss
        checkDeadlineMiss();
        
        // Update next release time for periodic tasks
        if (task_type == TaskType::PERIODIC) {
            updateNextReleaseTime();
            setState(State::READY);
        } else if (task_type == TaskType::ONE_SHOT) {
            setState(State::TERMINATED);
        } else {
            setState(State::READY);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error in task '" << name << "': " << e.what() << std::endl;
        setState(State::TERMINATED);
    }
    
    recordExecutionEnd();
}

bool Task::isReadyToRun() const {
    std::lock_guard<std::mutex> lock(task_mutex);
    
    if (!enabled.load() || current_state.load() != State::READY) {
        return false;
    }
    
    // For periodic tasks, check if it's time to run
    if (task_type == TaskType::PERIODIC) {
        return std::chrono::steady_clock::now() >= next_release_time;
    }
    
    return true;
}

void Task::suspend() {
    std::lock_guard<std::mutex> lock(task_mutex);
    
    if (current_state.load() == State::RUNNING) {
        std::cerr << "Warning: Cannot suspend running task '" << name << "'" << std::endl;
        return;
    }
    
    setState(State::SUSPENDED);
    std::cout << "Task '" << name << "' suspended" << std::endl;
}

void Task::resume() {
    std::lock_guard<std::mutex> lock(task_mutex);
    
    if (current_state.load() == State::SUSPENDED) {
        setState(State::READY);
        std::cout << "Task '" << name << "' resumed" << std::endl;
    }
}

void Task::terminate() {
    std::lock_guard<std::mutex> lock(task_mutex);
    
    setState(State::TERMINATED);
    enabled = false;
    delete_requested = true;
    
    std::cout << "Task '" << name << "' terminated" << std::endl;
}

void Task::sleep(std::chrono::milliseconds duration) {
    std::lock_guard<std::mutex> lock(task_mutex);
    
    setState(State::SLEEPING);
    next_release_time = std::chrono::steady_clock::now() + duration;
    
    // Task will become ready again after sleep duration
}

bool Task::setPriority(Priority new_priority) {
    std::lock_guard<std::mutex> lock(task_mutex);
    
    if (current_state.load() == State::RUNNING) {
        std::cerr << "Warning: Cannot change priority of running task" << std::endl;
        return false;
    }
    
    Priority old_priority = priority;
    priority = new_priority;
    
    std::cout << "Task '" << name << "' priority changed from " 
              << static_cast<int>(old_priority) << " to " 
              << static_cast<int>(new_priority) << std::endl;
    
    return true;
}

bool Task::setPeriod(std::chrono::milliseconds new_period) {
    std::lock_guard<std::mutex> lock(task_mutex);
    
    if (task_type != TaskType::PERIODIC) {
        std::cerr << "Error: Cannot set period for non-periodic task" << std::endl;
        return false;
    }
    
    timing.period = new_period;
    updateNextReleaseTime();
    
    return true;
}

bool Task::hasDeadlinePassed() const {
    return std::chrono::steady_clock::now() > deadline_time;
}

void Task::updateNextReleaseTime() {
    if (task_type == TaskType::PERIODIC) {
        next_release_time += timing.period;
        deadline_time = next_release_time + timing.deadline;
    }
}

void Task::setState(State new_state) {
    State old_state = current_state.load();
    current_state = new_state;
    
    if (old_state != new_state) {
        statistics.context_switches++;
    }
}

void Task::recordExecutionStart() {
    execution_start_time = std::chrono::steady_clock::now();
    statistics.last_execution = execution_start_time;
}

void Task::recordExecutionEnd() {
    auto execution_end_time = std::chrono::steady_clock::now();
    auto execution_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        execution_end_time - execution_start_time);
    
    statistics.total_execution_time += execution_duration;
    
    if (statistics.executions_count == 1) {
        statistics.max_execution_time = execution_duration;
        statistics.min_execution_time = execution_duration;
    } else {
        if (execution_duration > statistics.max_execution_time) {
            statistics.max_execution_time = execution_duration;
        }
        if (execution_duration < statistics.min_execution_time) {
            statistics.min_execution_time = execution_duration;
        }
    }
}

void Task::checkDeadlineMiss() {
    if (hasDeadlinePassed()) {
        statistics.missed_deadlines++;
        std::cerr << "WARNING: Task '" << name << "' missed deadline!" << std::endl;
    }
}

double Task::getAverageExecutionTime() const {
    std::lock_guard<std::mutex> lock(task_mutex);
    
    if (statistics.executions_count == 0) {
        return 0.0;
    }
    
    return static_cast<double>(statistics.total_execution_time.count()) / 
           static_cast<double>(statistics.executions_count);
}

double Task::getCPUUtilization(std::chrono::milliseconds /* window */) const {
    std::lock_guard<std::mutex> lock(task_mutex);
    
    if (task_type != TaskType::PERIODIC || statistics.executions_count == 0) {
        return 0.0;
    }
    
    // Calculate utilization = execution_time / period
    double avg_execution_ms = getAverageExecutionTime() / 1000.0; // Convert to milliseconds
    double period_ms = static_cast<double>(timing.period.count());
    
    return (avg_execution_ms / period_ms) * 100.0; // Return as percentage
}

void Task::resetStatistics() {
    std::lock_guard<std::mutex> lock(task_mutex);
    
    statistics = {};
    statistics.creation_time = std::chrono::steady_clock::now();
    statistics.last_execution = statistics.creation_time;
}

// String conversion methods
std::string Task::stateToString() const {
    return stateToString(current_state.load());
}

std::string Task::priorityToString() const {
    return priorityToString(priority);
}

std::string Task::taskTypeToString() const {
    return taskTypeToString(task_type);
}

std::string Task::stateToString(State state) {
    switch (state) {
        case State::READY: return "READY";
        case State::RUNNING: return "RUNNING";
        case State::BLOCKED: return "BLOCKED";
        case State::SUSPENDED: return "SUSPENDED";
        case State::TERMINATED: return "TERMINATED";
        case State::SLEEPING: return "SLEEPING";
        default: return "UNKNOWN";
    }
}

std::string Task::priorityToString(Priority priority) {
    switch (priority) {
        case Priority::IDLE: return "IDLE";
        case Priority::VERY_LOW: return "VERY_LOW";
        case Priority::LOW: return "LOW";
        case Priority::NORMAL: return "NORMAL";
        case Priority::HIGH: return "HIGH";
        case Priority::VERY_HIGH: return "VERY_HIGH";
        case Priority::CRITICAL: return "CRITICAL";
        case Priority::INTERRUPT: return "INTERRUPT";
        default: return "CUSTOM_" + std::to_string(static_cast<int>(priority));
    }
}

std::string Task::taskTypeToString(TaskType type) {
    switch (type) {
        case TaskType::PERIODIC: return "PERIODIC";
        case TaskType::APERIODIC: return "APERIODIC";
        case TaskType::SPORADIC: return "SPORADIC";
        case TaskType::ONE_SHOT: return "ONE_SHOT";
        default: return "UNKNOWN";
    }
}