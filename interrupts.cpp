#include <tuple>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include "interrupts.hpp"

// Helper function to parse a trace line
std::tuple<std::string, int, std::string> parse_trace(const std::string& line) {
    std::stringstream ss(line);
    std::string activity, duration_str, arg;
    std::getline(ss, activity, ',');
    std::getline(ss, duration_str, ',');
    std::getline(ss, arg);
    int duration = std::stoi(duration_str);
    // Trim whitespace
    activity.erase(0, activity.find_first_not_of(" \t"));
    arg.erase(0, arg.find_first_not_of(" \t"));
    return {activity, duration, arg};
}

// Recursive simulation function
std::tuple<std::string, std::string, int> simulate_trace(
    const std::vector<std::string>& trace_file, 
    int time, 
    const std::vector<std::string>& vectors, 
    const std::vector<int>& delays, 
    const std::vector<external_file>& external_files, 
    PCB current, 
    std::vector<PCB> wait_queue)
{
    std::string execution = "";
    std::string system_status = "";
    int current_time = time;

    for (size_t i = 0; i < trace_file.size(); i++) {
        auto trace_line = trace_file[i];
        auto [activity, duration, program_name] = parse_trace(trace_line);

        if (activity == "CPU") {
            execution += std::to_string(current_time) + ", " + std::to_string(duration) + ", CPU Burst\n";
            current_time += duration;
        }
        else if (activity == "SYSCALL" || activity == "END_IO") {
            auto [intr, new_time] = intr_boilerplate(current_time, duration, 10, vectors);
            execution += intr;
            current_time = new_time;
            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration]) + ", " + activity + " ISR\n";
            current_time += delays[duration];
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        }
        else if (activity == "FORK") {
            // Run syscall
            auto [intr, new_time] = intr_boilerplate(current_time, duration, 10, vectors);
            execution += intr;
            current_time = new_time;

            // Log FORK ISR
            execution += std::to_string(current_time) + ", " + std::to_string(duration) + ", cloning the PCB\n";
            current_time += duration;

            // Snapshot system status
            system_status += "time: " + std::to_string(current_time) + "; current trace: " + trace_line + "\n";
            system_status += "+------------------------------------------------------+\n";
            system_status += "| PID | program name | partition number | size | state |\n";
            system_status += "+------------------------------------------------------+\n";
            system_status += "| " + std::to_string(current.pid+1) + " | " + current.name + " | " + std::to_string(current.partition) + " | " + std::to_string(current.size) + " | running |\n";
            system_status += "| " + std::to_string(current.pid) + " | " + current.name + " | " + std::to_string(current.partition) + " | " + std::to_string(current.size) + " | waiting |\n";
            system_status += "+------------------------------------------------------+\n";

            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;

            // Collect child trace
            std::vector<std::string> child_trace;
            size_t parent_index = i;
            bool skip = true;
            bool exec_flag = false;

            for (size_t j = i; j < trace_file.size(); j++) {
                auto [act, dur, prog] = parse_trace(trace_file[j]);
                if (skip && act == "IF_CHILD") {
                    skip = false;
                    continue;
                }
                else if (act == "IF_PARENT") {
                    skip = true;
                    parent_index = j;
                    if (exec_flag) break;
                }
                else if (skip && act == "ENDIF") {
                    skip = false;
                    continue;
                }
                else if (!skip && act == "EXEC") {
                    skip = true;
                    child_trace.push_back(trace_file[j]);
                    exec_flag = true;
                }
                if (!skip) child_trace.push_back(trace_file[j]);
            }
            i = parent_index;

            // Run child trace recursively
            auto [child_exec, child_status, child_time] = simulate_trace(child_trace, current_time, vectors, delays, external_files, current, wait_queue);
            execution += child_exec;
            system_status += child_status;
            current_time = child_time;
        }
        else if (activity == "EXEC") {
            auto [intr, new_time] = intr_boilerplate(current_time, 3, 10, vectors);
            execution += intr;
            current_time = new_time;

            // Find program size from external_files
            int program_size = 0;
            for (auto& f : external_files) {
                if (f.name == program_name) {
                    program_size = f.size;
                    break;
                }
            }

            execution += std::to_string(current_time) + ", " + std::to_string(program_size) + ", Program is " + std::to_string(program_size) + " Mb large\n";
            execution += std::to_string(current_time + program_size*15) + ", " + std::to_string(program_size*15) + ", loading program into memory\n";
            current_time += program_size*15;

            execution += std::to_string(current_time) + ", 3, marking partition as occupied\n";
            current_time += 3;
            execution += std::to_string(current_time) + ", 6, updating PCB\n";
            current_time += 6;
            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;

            // Read exec trace from file and run recursively
            std::ifstream exec_file(program_name + ".txt");
            std::vector<std::string> exec_traces;
            std::string line;
            while (std::getline(exec_file, line)) exec_traces.push_back(line);
            exec_file.close();

            auto [exec_trace_str, exec_status_str, exec_time] = simulate_trace(exec_traces, current_time, vectors, delays, external_files, current, wait_queue);
            execution += exec_trace_str;
            system_status += exec_status_str;
            current_time = exec_time;

            break; // stop parent after EXEC
        }
    }

    return {execution, system_status, current_time};
}


int main(int argc, char** argv) {

    //vectors is a C++ std::vector of strings that contain the address of the ISR
    //delays  is a C++ std::vector of ints that contain the delays of each device
    //the index of these elemens is the device number, starting from 0
    //external_files is a C++ std::vector of the struct 'external_file'. Check the struct in 
    //interrupt.hpp to know more.
    auto [vectors, delays, external_files] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);

    //Just a sanity check to know what files you have
    print_external_files(external_files);

    //Make initial PCB (notice how partition is not assigned yet)
    PCB current(0, -1, "init", 1, -1);
    //Update memory (partition is assigned here, you must implement this function)
    if(!allocate_memory(&current)) {
        std::cerr << "ERROR! Memory allocation failed!" << std::endl;
    }

    std::vector<PCB> wait_queue;

    /******************ADD YOUR VARIABLES HERE*************************/
    // Simulated memory: six fixed partitions (in MB)
    struct Partition {
        unsigned int number;
        unsigned int size;
        std::string code; // "free", "init", or program name
    };

    std::vector<Partition> memory = {
        {1, 40, "free"},
        {2, 25, "free"},
        {3, 15, "free"},
        {4, 10, "free"},
        {5, 8,  "free"},
        {6, 2,  "free"}
    };

    // Assign the init process (PID 0) to partition 6
    memory[5].code = "init";
    current.partition_number = 6;

    // Initialize process ID counter
    int next_pid = 1;

    // Random number generator for timing (1–10 ms)
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> rand_ms(1, 10);

    // Execution log (time, duration, description)
    std::vector<std::tuple<int, int, std::string>> execution_log;

    // System status snapshots (to be written after FORK and EXEC)
    std::vector<std::string> system_status_log;

    // Global simulation clock
    int current_time = 0;



    /******************************************************************/

    //Converting the trace file into a vector of strings.
    std::vector<std::string> trace_file;
    std::string trace;
    while(std::getline(input_file, trace)) {
        trace_file.push_back(trace);
    }

    auto [execution, system_status, _] = simulate_trace(   trace_file, 
                                            0, 
                                            vectors, 
                                            delays,
                                            external_files, 
                                            current, 
                                            wait_queue);

    input_file.close();

    write_output(execution, "execution.txt");
    write_output(system_status, "system_status.txt");

    return 0;
}
