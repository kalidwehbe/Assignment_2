/**
 *
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 *
 */

#include <interrupts.hpp>
#include <fstream>
#include <sstream>
#include <tuple>
#include <vector>
#include <string>
#include <algorithm>

// Forward declaration
std::tuple<std::string, std::string, int> simulate_trace(
    std::vector<std::string> trace_file,
    int time,
    std::vector<std::string> vectors,
    std::vector<int> delays,
    std::vector<external_file> external_files,
    PCB current,
    std::vector<PCB> wait_queue
) {
    std::string execution = "";
    std::string system_status = "";
    int current_time = time;

    for (size_t i = 0; i < trace_file.size(); i++) {
        auto trace = trace_file[i];
        auto [activity, duration_intr, program_name] = parse_trace(trace);

        if (activity == "CPU") {
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", CPU Burst\n";
            current_time += duration_intr;

        } else if (activity == "SYSCALL") {
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = time;
            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", SYSCALL ISR\n";
            current_time += delays[duration_intr];
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;

        } else if (activity == "END_IO") {
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = time;
            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", ENDIO ISR\n";
            current_time += delays[duration_intr];
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;

        } else if (activity == "FORK") {
            auto [intr, time] = intr_boilerplate(current_time, 2, 10, vectors);
            execution += intr;
            current_time = time;

            int child_pid = current.PID + 1 + wait_queue.size();
            PCB child(child_pid, current.PID, current.program_name, current.size, current.partition_number - 1);

            wait_queue.push_back(current); // parent goes to waiting queue

            // system_status
            system_status += "time: " + std::to_string(current_time) + "; current trace: FORK, " + std::to_string(duration_intr) + "\n";
            system_status += "+------------------------------------------------------+\n";
            system_status += "| PID |program name |partition number | size | state |\n";
            system_status += "+------------------------------------------------------+\n";

            system_status += "| " + std::to_string(child.PID) + " | " + child.program_name + " | " +
                             std::to_string(child.partition_number) + " | " + std::to_string(child.size) + " | running |\n";

            system_status += "| " + std::to_string(current.PID) + " | " + current.program_name + " | " +
                             std::to_string(current.partition_number) + " | " + std::to_string(current.size) + " | waiting |\n";

            system_status += "+------------------------------------------------------+\n";

            // Child trace collection
            std::vector<std::string> child_trace;
            bool skip = true;
            bool exec_flag = false;
            int parent_index = 0;

            for (size_t j = i; j < trace_file.size(); j++) {
                auto [_activity, _duration, _pn] = parse_trace(trace_file[j]);
                if (skip && _activity == "IF_CHILD") { skip = false; continue; }
                else if (_activity == "IF_PARENT") { skip = true; parent_index = j; if (exec_flag) break; }
                else if (skip && _activity == "ENDIF") { skip = false; continue; }
                else if (!skip && _activity == "EXEC") { skip = true; child_trace.push_back(trace_file[j]); exec_flag = true; }
                if (!skip) child_trace.push_back(trace_file[j]);
            }
            i = parent_index;

            auto [child_exec, child_status, new_time] = simulate_trace(child_trace, current_time, vectors, delays, external_files, child, wait_queue);
            execution += child_exec;
            system_status += child_status;
            current_time = new_time;

        } else if (activity == "EXEC") {
            auto [intr, time] = intr_boilerplate(current_time, 3, 10, vectors);
            execution += intr;
            current_time = time;

            current.program_name = program_name;
            current.size = get_size(program_name, external_files);
            allocate_memory(&current);

            system_status += "time: " + std::to_string(current_time) + "; current trace: EXEC " + program_name + ", " + std::to_string(duration_intr) + "\n";
            system_status += "+------------------------------------------------------+\n";
            system_status += "| PID |program name |partition number | size | state |\n";
            system_status += "+------------------------------------------------------+\n";

            system_status += "| " + std::to_string(current.PID) + " | " + current.program_name + " | " +
                             std::to_string(current.partition_number) + " | " + std::to_string(current.size) + " | running |\n";

            for (auto &p : wait_queue) {
                system_status += "| " + std::to_string(p.PID) + " | " + p.program_name + " | " +
                                 std::to_string(p.partition_number) + " | " + std::to_string(p.size) + " | waiting |\n";
            }

            system_status += "+------------------------------------------------------+\n";

            std::ifstream exec_trace_file(program_name + ".txt");
            std::vector<std::string> exec_traces;
            std::string exec_trace;
            while (std::getline(exec_trace_file, exec_trace)) exec_traces.push_back(exec_trace);

            if (!exec_traces.empty()) {
                auto [child_exec, child_status, new_time] = simulate_trace(exec_traces, current_time, vectors, delays, external_files, current, wait_queue);
                execution += child_exec;
                system_status += child_status;
                current_time = new_time;
            }

            break;
        }
    }

    return {execution, system_status, current_time};
}

int main(int argc, char** argv) {
    auto [vectors, delays, external_files] = parse_args(argc, argv);
    std::ifstream input_file("inputfiles/trace.txt");

    print_external_files(external_files);

    PCB current(0, -1, "init", 1, 6);
    if(!allocate_memory(&current)) { std::cerr << "ERROR! Memory allocation failed!\n"; }

    std::vector<PCB> wait_queue;

    std::vector<std::string> trace_file;
    std::string trace;
    while(std::getline(input_file, trace)) trace_file.push_back(trace);

    auto [execution, system_status, _] = simulate_trace(trace_file, 0, vectors, delays, external_files, current, wait_queue);

    input_file.close();

    write_output(execution, "ouputs/execution.txt");
    write_output(system_status, "ouputs/system_status.txt");

    return 0;
}
