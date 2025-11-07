#!/bin/bash

# SYSC4001 Assignment 2 Part 3 - Test Runner Script
# This script runs all 5 test cases and outputs results to output_files folder

echo "=========================================="
echo "SYSC4001 Assignment 2 - Running Test Cases"
echo "=========================================="
echo ""

# Check if the executable exists
if [ ! -f "./bin/interrupts" ]; then
    echo "Error: interrupts executable not found."
    echo "Please run build.sh first to compile the program."
    exit 1
fi

# Create output_files directory if it doesn't exist
mkdir -p output_files

# Function to run a single test
run_test() {
    local test_num=$1
    echo "Running Test $test_num..."
    echo "------------------------"

    # Copy input files to root directory (where program expects them)
    cp input_files/test${test_num}_trace.txt trace.txt
    cp input_files/test${test_num}_external_files.txt external_files.txt

    # Copy program files
    if [ -f "input_files/test${test_num}_program1.txt" ]; then
        cp input_files/test${test_num}_program1.txt program1.txt
    fi
    if [ -f "input_files/test${test_num}_program2.txt" ]; then
        cp input_files/test${test_num}_program2.txt program2.txt
    fi

    # Run the simulator
    ./bin/interrupts trace.txt vector_table.txt device_table.txt external_files.txt

    # Move output files to output_files directory
    if [ -f "execution.txt" ]; then
        mv execution.txt output_files/test${test_num}_execution.txt
        echo "Execution file created: output_files/test${test_num}_execution.txt"
    fi

    if [ -f "system_status.txt" ]; then
        mv system_status.txt output_files/test${test_num}_system_status.txt
        echo "System status file created: output_files/test${test_num}_system_status.txt"
    fi

    echo "Test $test_num completed!"
    echo ""
}

# Run all 5 tests
for i in {1..5}; do
    run_test $i
done

# Clean up temporary files
rm -f trace.txt external_files.txt program1.txt program2.txt

echo "=========================================="
echo "All tests completed!"
echo "=========================================="
echo "Output files in the output_files/ directory:"
echo ""
ls -1 output_files/
echo ""
echo "You can now view the results"
