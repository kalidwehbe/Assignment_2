/**
 *
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 *
 */

#include<interrupts.hpp>

std::tuple<std::string, std::string, int> simulate_trace(
    std::vector<std::string> trace_file, int time, std::vector<std::string> vectors,
    std::vector<int> delays, std::vector<external_file> external_files,
    PCB current, std::vector<PCB> wait_queue) {

    std::string trace;      //!< string to store single line of trace file
    std::string execution = "";  //!< string to accumulate the execution output
    std::string system_status = "";  //!< string to accumulate the system status output
    int current_time = time;

    for(size_t i = 0; i < trace_file.size(); i++) {
        auto trace = trace_file[i];
        auto [activity, duration_intr, program_name] = parse_trace(trace);

        if(activity == "CPU") {
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", CPU Burst\n";
            current_time += duration_intr;
        } else if(activity == "SYSCALL") {
            auto [intr, time2] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = time2;
            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", SYSCALL ISR\n";
            current_time += delays[duration_intr];
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "END_IO") {
            auto [intr, time2] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = time2;
            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", ENDIO ISR\n";
            current_time += delays[duration_intr];
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "FORK") {
            auto [intr, time2] = intr_boilerplate(current_time, 2, 10, vectors);
            execution += intr;
            current_time = time2;

            // ------------------- FORK output -------------------
            PCB child(current.PID + 1, current.PID, current.program_name, current.size, -1);
            allocate_memory(&child);

            wait_queue.push_back(current);  // parent goes waiting
            current = child;                // child becomes running

            system_status += "time: " + std::to_string(current_time) + "; current trace: " + trace + " //fork cloned parent\n";
            system_status += print_PCB(current, wait_queue);
            // ---------------------------------------------------

            // Collect child trace for recursion
            std::vector<std::string> child_trace;
            bool skip = true;
            bool exec_flag = false;
            int parent_index = 0;

            for(size_t j = i; j < trace_file.size(); j++) {
                auto [_activity, _duration, _pn] = parse_trace(trace_file[j]);
                if(skip && _activity == "IF_CHILD") { skip = false; continue; }
                else if(_activity == "IF_PARENT"){
                    skip = true; parent_index = j;
                    if(exec_flag) break;
                } else if(skip && _activity == "ENDIF") { skip = false; continue; }
                else if(!skip && _activity == "EXEC") { skip = true; child_trace.push_back(trace_file[j]); exec_flag = true; }
                if(!skip) child_trace.push_back(trace_file[j]);
            }
            i = parent_index;

            // Recursively run the child trace
            if(!child_trace.empty()){
                auto [child_exec, child_status, child_time] = simulate_trace(child_trace, current_time, vectors, delays, external_files, current, wait_queue);
                execution += child_exec;
                system_status += child_status;
                current_time = child_time;
            }

        } else if(activity == "EXEC") {
            auto [intr, time2] = intr_boilerplate(current_time, 3, 10, vectors);
            execution += intr;
            current_time = time2;

            // ------------------- EXEC output -------------------
            unsigned int new_size = get_size(program_name, external_files);
            free_memory(&current);
            current.program_name = program_name;
            current.size = new_size;
            allocate_memory(&current);

            system_status += "time: " + std::to_string(current_time) + "; current trace: " + trace + " //exec updated process\n";
            system_status += print_PCB(current, wait_queue);
            // ---------------------------------------------------

            std::ifstream exec_trace_file(program_name + ".txt");
            std::vector<std::string> exec_traces;
            std::string exec_trace;
            while(std::getline(exec_trace_file, exec_trace)) exec_traces.push_back(exec_trace);

            if(!exec_traces.empty()){
                auto [exec_exec, exec_status, exec_time] = simulate_trace(exec_traces, current_time, vectors, delays, external_files, current, wait_queue);
                execution += exec_exec;
                system_status += exec_status;
                current_time = exec_time;
            }

            break; // only run this exec once
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

    std::vector<PCB> wait_queue;

    std::vector<std::string> trace_file;
    std::string trace;
    while(std::getline(input_file, trace)) trace_file.push_back(trace);

    auto [execution, system_status, _] = simulate_trace(trace_file, 0, vectors, delays, external_files, current, wait_queue);

    input_file.close();

    write_output(execution, "execution_test_2");
    write_output(system_status, "system_status_2");

    return 0;
}
