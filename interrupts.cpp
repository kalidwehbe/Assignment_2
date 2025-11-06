/**
 *
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 *
 */

#include "interrupts.hpp"

std::tuple<std::string, std::string, int> simulate_trace(std::vector<std::string> trace_file, int time, std::vector<std::string> vectors, std::vector<int> delays, std::vector<external_file> external_files, PCB current, std::vector<PCB> wait_queue) {

    std::string execution = "";      //!< string to accumulate the execution output
    std::string system_status = "";  //!< string to accumulate the system status output
    int current_time = time;

    for(size_t i = 0; i < trace_file.size(); i++) {
        auto trace = trace_file[i];
        auto [activity, duration_intr, program_name] = parse_trace(trace);

        if(activity == "CPU") {
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", CPU Burst\n";
            current_time += duration_intr;

        } else if(activity == "SYSCALL") {
            auto [intr, t] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = t;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", SYSCALL ISR\n";
            current_time += delays[duration_intr];

            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;

        } else if(activity == "END_IO") {
            auto [intr, t] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = t;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", ENDIO ISR\n";
            current_time += delays[duration_intr];

            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;

        } else if(activity == "FORK") {
            // ----------------- FORK ISR -----------------
            execution += std::to_string(current_time) + ", 1, switch to kernel mode //fork encountered, "
                         + std::to_string(wait_queue.size() + 1) + " processes in PCB\n";
            execution += std::to_string(current_time + 1) + ", 10, context saved\n";
            current_time += 11;

            execution += std::to_string(current_time) + ", 1, find vector 2 in memory position 0x0004\n";
            execution += std::to_string(current_time + 1) + ", 1, load address 0X0695 into the PC\n";
            execution += std::to_string(current_time + 2) + ", " + std::to_string(duration_intr) + ", cloning the PCB\n";
            current_time += duration_intr;

            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;

            PCB child(wait_queue.size() + 1, current.PID, current.program_name, current.size, current.partition_number);
            wait_queue.push_back(current); // parent goes to wait queue
            current = child; // child runs immediately

            system_status += "time: " + std::to_string(current_time) + "; current trace: FORK, " + std::to_string(duration_intr) + "\n";
            system_status += print_PCB(current, wait_queue);

            // ----------------- Collect child trace -----------------
            std::vector<std::string> child_trace;
            bool skip = true;
            bool exec_flag = false;
            int parent_index = 0;

            for(size_t j = i; j < trace_file.size(); j++) {
                auto [_activity, _duration, _pn] = parse_trace(trace_file[j]);
                if(skip && _activity == "IF_CHILD") { skip = false; continue; }
                else if(_activity == "IF_PARENT") { skip = true; parent_index = j; if(exec_flag) break; }
                else if(skip && _activity == "ENDIF") { skip = false; continue; }
                else if(!skip && _activity == "EXEC") { skip = true; child_trace.push_back(trace_file[j]); exec_flag = true; }

                if(!skip) child_trace.push_back(trace_file[j]);
            }
            i = parent_index;

            // ----------------- Recursive child execution -----------------
            auto [child_exec, child_status, child_time] = simulate_trace(child_trace, current_time, vectors, delays, external_files, current, wait_queue);
            execution += child_exec;
            system_status += child_status;
            current_time = child_time;

            // Restore parent
            current = wait_queue.back();
            wait_queue.pop_back();

        } else if(activity == "EXEC") {
            // ----------------- EXEC ISR -----------------
            execution += std::to_string(current_time) + ", 1, switch to kernel mode //exec encountered\n";
            execution += std::to_string(current_time + 1) + ", 10, context saved\n";
            current_time += 11;

            unsigned int prog_size = get_size(program_name, external_files);
            current.program_name = program_name;
            current.size = prog_size;

            if(!allocate_memory(&current)) {
                std::cerr << "ERROR! Memory allocation for EXEC failed!\n";
            }

            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", Program is " + std::to_string(prog_size) + " Mb large\n";
            current_time += duration_intr;

            execution += std::to_string(current_time) + ", " + std::to_string(prog_size * 15) + ", loading program into memory\n";
            current_time += prog_size * 15;

            execution += std::to_string(current_time) + ", 3, marking partition as occupied\n";
            current_time += 3;

            execution += std::to_string(current_time) + ", 6, updating PCB\n";
            current_time += 6;

            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;

            system_status += "time: " + std::to_string(current_time) + "; current trace: EXEC " + program_name + ", " + std::to_string(duration_intr) + "\n";
            system_status += print_PCB(current, wait_queue);

            // ----------------- EXEC Recursive Child Execution -----------------
            std::ifstream exec_trace_file(program_name + ".txt");
            std::vector<std::string> exec_traces;
            std::string line;
            while(std::getline(exec_trace_file, line)) exec_traces.push_back(line);

            auto [exec_exec, exec_status, exec_time] = simulate_trace(exec_traces, current_time, vectors, delays, external_files, current, wait_queue);
            execution += exec_exec;
            system_status += exec_status;
            current_time = exec_time;

            break; // Important: stop processing remaining lines at this level
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
        std::cerr << "ERROR! Memory allocation failed!\n";
    }

    std::vector<PCB> wait_queue;

    // Convert trace file to vector
    std::vector<std::string> trace_file;
    std::string trace;
    while(std::getline(input_file, trace)) {
        trace_file.push_back(trace);
    }

    auto [execution, system_status, _] = simulate_trace(trace_file, 0, vectors, delays, external_files, current, wait_queue);

    input_file.close();

    write_output(execution, "execution.txt");
    write_output(system_status, "system_status.txt");

    return 0;
}
