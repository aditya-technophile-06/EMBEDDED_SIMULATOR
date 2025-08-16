# Embedded Systems Simulator Makefile
# Demonstrates professional build system knowledge

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -Iinclude -pthread
DEBUG_FLAGS = -g -DDEBUG -O0 -fsanitize=address -fsanitize=undefined
RELEASE_FLAGS = -O2 -DNDEBUG -march=native
PROFILE_FLAGS = -pg -O2

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
TEST_DIR = tests
EXAMPLE_DIR = examples
DOC_DIR = docs

# Find all source files
SDK_SOURCES = $(wildcard $(SRC_DIR)/sdk/*.cpp)
DRIVER_SOURCES = $(wildcard $(SRC_DIR)/drivers/*.cpp)
RTOS_SOURCES = $(wildcard $(SRC_DIR)/rtos/*.cpp)
PROTOCOL_SOURCES = $(wildcard $(SRC_DIR)/protocol/*.cpp)
COMMON_SOURCES = $(wildcard $(SRC_DIR)/common/*.cpp)
MAIN_SOURCE = $(SRC_DIR)/main.cpp

# Object files
SDK_OBJECTS = $(SDK_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
DRIVER_OBJECTS = $(DRIVER_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
RTOS_OBJECTS = $(RTOS_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
PROTOCOL_OBJECTS = $(PROTOCOL_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
COMMON_OBJECTS = $(COMMON_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
MAIN_OBJECT = $(BUILD_DIR)/main.o

ALL_OBJECTS = $(SDK_OBJECTS) $(DRIVER_OBJECTS) $(RTOS_OBJECTS) $(PROTOCOL_OBJECTS) $(COMMON_OBJECTS) $(MAIN_OBJECT)
LIB_OBJECTS = $(SDK_OBJECTS) $(DRIVER_OBJECTS) $(RTOS_OBJECTS) $(PROTOCOL_OBJECTS) $(COMMON_OBJECTS)

# Dependency files
DEPENDS = $(ALL_OBJECTS:.o=.d)

# Targets
TARGET = embedded_simulator
LIB_TARGET = libembedded_sim.a
TEST_TARGET = test_runner

# Default target
.PHONY: all clean debug release profile test install uninstall help

all: debug

# Debug build (default)
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: $(TARGET)
	@echo "Debug build complete. Run with: ./$(TARGET)"

# Release build (optimized)
release: CXXFLAGS += $(RELEASE_FLAGS)
release: clean $(TARGET)
	@echo "Release build complete. Run with: ./$(TARGET)"

# Profile build (for performance analysis)
profile: CXXFLAGS += $(PROFILE_FLAGS)
profile: clean $(TARGET)
	@echo "Profile build complete. Run with: ./$(TARGET)"
	@echo "Generate profile: gprof ./$(TARGET) gmon.out > profile.txt"

# Main executable
$(TARGET): $(ALL_OBJECTS)
	@echo "Linking $(TARGET)..."
	$(CXX) $(ALL_OBJECTS) -o $@ $(CXXFLAGS)

# Static library (for linking with other projects)
$(LIB_TARGET): $(LIB_OBJECTS)
	@echo "Creating static library $(LIB_TARGET)..."
	ar rcs $@ $(LIB_OBJECTS)

# Generic object file rule with dependency generation
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Test target (if tests exist)
test: $(TEST_TARGET)
	@echo "Running tests..."
	./$(TEST_TARGET)

$(TEST_TARGET): $(LIB_OBJECTS)
	@if [ -f "$(TEST_DIR)/test_main.cpp" ]; then \
		echo "Building tests..."; \
		$(CXX) $(CXXFLAGS) $(TEST_DIR)/*.cpp $(LIB_OBJECTS) -o $@; \
	else \
		echo "No tests found in $(TEST_DIR)"; \
		exit 1; \
	fi

# Examples
examples: $(LIB_TARGET)
	@echo "Building examples..."
	@mkdir -p $(BUILD_DIR)/examples
	@for example in $(wildcard $(EXAMPLE_DIR)/*.cpp); do \
		name=$$(basename $$example .cpp); \
		echo "Building example: $$name"; \
		$(CXX) $(CXXFLAGS) $$example $(LIB_TARGET) -o $(BUILD_DIR)/examples/$$name; \
	done

# Documentation (if Doxygen is available)
docs:
	@if command -v doxygen >/dev/null 2>&1; then \
		echo "Generating documentation..."; \
		doxygen Doxyfile; \
	else \
		echo "Doxygen not found. Install with: sudo apt install doxygen"; \
	fi

# Installation
install: release
	@echo "Installing $(TARGET)..."
	@sudo mkdir -p /usr/local/bin
	@sudo cp $(TARGET) /usr/local/bin/
	@sudo mkdir -p /usr/local/include/embedded_sim
	@sudo cp -r $(INC_DIR)/* /usr/local/include/embedded_sim/
	@sudo mkdir -p /usr/local/lib
	@sudo cp $(LIB_TARGET) /usr/local/lib/
	@echo "Installation complete."

uninstall:
	@echo "Uninstalling $(TARGET)..."
	@sudo rm -f /usr/local/bin/$(TARGET)
	@sudo rm -rf /usr/local/include/embedded_sim
	@sudo rm -f /usr/local/lib/$(LIB_TARGET)
	@echo "Uninstallation complete."

# Cleaning
clean:
	@echo "Cleaning build files..."
	rm -rf $(BUILD_DIR) $(TARGET) $(LIB_TARGET) $(TEST_TARGET)
	rm -f gmon.out profile.txt
	@echo "Clean complete."

# Static analysis (if tools are available)
analyze:
	@echo "Running static analysis..."
	@if command -v cppcheck >/dev/null 2>&1; then \
		cppcheck --enable=all --std=c++17 --inconclusive $(SRC_DIR) $(INC_DIR); \
	else \
		echo "cppcheck not found. Install with: sudo apt install cppcheck"; \
	fi

# Memory check (requires valgrind)
memcheck: debug
	@if command -v valgrind >/dev/null 2>&1; then \
		echo "Running memory check..."; \
		valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET); \
	else \
		echo "valgrind not found. Install with: sudo apt install valgrind"; \
	fi

# Code formatting (requires clang-format)
format:
	@if command -v clang-format >/dev/null 2>&1; then \
		echo "Formatting code..."; \
		find $(SRC_DIR) $(INC_DIR) -name "*.cpp" -o -name "*.h" | xargs clang-format -i; \
		echo "Code formatting complete."; \
	else \
		echo "clang-format not found. Install with: sudo apt install clang-format"; \
	fi

# Package creation
package: release
	@echo "Creating package..."
	@mkdir -p embedded_sim_package
	@cp $(TARGET) embedded_sim_package/
	@cp README.md embedded_sim_package/ 2>/dev/null || echo "README.md not found"
	@cp -r examples embedded_sim_package/ 2>/dev/null || echo "examples not found"
	@tar -czf embedded_simulator.tar.gz embedded_sim_package/
	@rm -rf embedded_sim_package
	@echo "Package created: embedded_simulator.tar.gz"

# Help target
help:
	@echo "Embedded Systems Simulator Build System"
	@echo "========================================"
	@echo ""
	@echo "Available targets:"
	@echo "  all       - Build debug version (default)"
	@echo "  debug     - Build debug version with sanitizers"
	@echo "  release   - Build optimized release version"
	@echo "  profile   - Build with profiling enabled"
	@echo "  test      - Build and run tests"
	@echo "  examples  - Build example applications"
	@echo "  docs      - Generate documentation (requires Doxygen)"
	@echo "  install   - Install to system directories"
	@echo "  uninstall - Remove from system directories"
	@echo "  clean     - Remove all build files"
	@echo "  analyze   - Run static analysis (requires cppcheck)"
	@echo "  memcheck  - Run memory leak detection (requires valgrind)"
	@echo "  format    - Format code (requires clang-format)"
	@echo "  package   - Create distribution package"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Compiler flags:"
	@echo "  Debug:   $(DEBUG_FLAGS)"
	@echo "  Release: $(RELEASE_FLAGS)"
	@echo "  Profile: $(PROFILE_FLAGS)"

# Include dependency files
-include $(DEPENDS)

# Prevent make from deleting intermediate files
.PRECIOUS: $(BUILD_DIR)/%.o