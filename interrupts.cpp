/**
 *
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 *
 */

#include "interrupts.hpp"
#include <fstream>
#include <sstream>

std::tuple<std::string, std::string, int> simulate_trace(
        std::vector<std::string> trace_file, int time, 
        std::vector<std::string> vectors, std::vector<int> delays, 
        std::vector<external_file> external_files, PCB current, 
        std::vector<PCB>& wait_queue) // pass wait_queue by reference
{
    std::string execution = "";   // store execution log
    std::string system_status = ""; // store PCB snapshot
    int current_time = time;

    std::vector<std::string> child_trace; // for fork child
    std::vector<std::string> exec_traces; // for exec program traces

    for (size_t i = 0; i < trace_file.size(); i++) {
        auto trace = trace_file[i];
        auto [activity, duration_intr, program_name] = parse_trace(trace);

        if(activity == "CPU") {
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", CPU Burst\n";
            current_time += duration_intr;
        } 
        else if(activity == "SYSCALL") {
            auto [intr, time_after] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = time_after;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", SYSCALL ISR\n";
            current_time += delays[duration_intr];

            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } 
        else if(activity == "END_IO") {
            auto [intr, time_after] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = time_after;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", ENDIO ISR\n";
            current_time += delays[duration_intr];

            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } 
        else if(activity == "FORK") {
            auto [intr, time_after] = intr_boilerplate(current_time, 2, 10, vectors);
            execution += intr;
            current_time = time_after;

            // Fork ISR
            execution += std::to_string(current_time) + ", 1, switch to kernel mode //fork encountered, "
                        + std::to_string(wait_queue.size() + 1) + " processes in PCB\n";
            execution += std::to_string(current_time + 1) + ", 10, context saved\n";
            current_time += 11;

            execution += std::to_string(current_time) + ", 1, find vector 2 in memory position 0x0004\n";
            execution += std::to_string(current_time + 1) + ", 1, load address 0X0695 into the PC\n";
            execution += std::to_string(current_time + 2) + ", 17, cloning the PCB\n";
            current_time += 20;

            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;

            // Clone PCB
            PCB child(current.PID + 1, current.PID, current.program_name, current.size, current.partition_number);
            wait_queue.push_back(child);

            // Snapshot PCB
            system_status += print_PCB(current, wait_queue);

            // Collect child trace (between IF_CHILD and ENDIF)
            child_trace.clear();
            for(size_t j = i+1; j < trace_file.size(); j++) {
                auto [_activity, _, __] = parse_trace(trace_file[j]);
                if(_activity == "ENDIF") { i = j; break; }
                if(_activity != "IF_CHILD") child_trace.push_back(trace_file[j]);
            }

            // Run child recursively
            auto [child_exec, child_status, child_time] = simulate_trace(child_trace, current_time, vectors, delays, external_files, child, wait_queue);
            execution += child_exec;
            system_status += child_status;
            current_time = child_time;
        } 
        else if(activity == "EXEC") {
            auto [intr, time_after] = intr_boilerplate(current_time, 3, 10, vectors);
            execution += intr;
            current_time = time_after;

            // EXEC ISR
            execution += std::to_string(current_time) + ", 1, switch to kernel mode //exec encountered by child\n";
            execution += std::to_string(current_time + 1) + ", 10, context saved\n";
            current_time += 11;

            unsigned int prog_size = get_size(program_name, external_files);
            execution += std::to_string(current_time) + ", 42, Program is " + std::to_string(prog_size) + " Mb large\n";
            current_time += 42;

            int load_duration = prog_size * 15;
            execution += std::to_string(current_time) + ", " + std::to_string(load_duration) + ", loading program into memory\n";
            current_time += load_duration;

            current.size = prog_size;
            allocate_memory(&current);

            execution += std::to_string(current_time) + ", 3, marking partition as occupied\n";
            execution += std::to_string(current_time + 3) + ", 6, updating PCB\n";
            current_time += 6;

            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;

            // Snapshot PCB after EXEC
            system_status += print_PCB(current, wait_queue);

            // Read external trace
            exec_traces.clear();
            {
                std::ifstream file(program_name + ".txt");
                std::string line;
                while(std::getline(file, line)) exec_traces.push_back(line);
            }

            auto [prog_exec, prog_status, prog_time] = simulate_trace(exec_traces, current_time, vectors, delays, external_files, current, wait_queue);
            execution += prog_exec;
            system_status += prog_status;
            current_time = prog_time;

            break; // stop after EXEC as in template
        }
    }

    return {execution, system_status, current_time};
}


int main(int argc, char** argv) {

    auto [vectors, delays, external_files] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);

    print_external_files(external_files);

    PCB current(0, -1, "init", 1, -1);
    if(!allocate_memory(&current)) {
        std::cerr << "ERROR! Memory allocation failed!" << std::endl;
    }

    // ----------------- Variables added here -----------------
    std::vector<PCB> wait_queue;
    std::vector<std::string> trace_file;
    std::string trace_line;
    // --------------------------------------------------------

    while(std::getline(input_file, trace_line)) {
        trace_file.push_back(trace_line);
    }

    auto [execution, system_status, _] = simulate_trace(trace_file, 0, vectors, delays, external_files, current, wait_queue);

    input_file.close();

    write_output(execution, "execution.txt");
    write_output(system_status, "system_status.txt");

    return 0;
}
