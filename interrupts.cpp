#include "interrupts.hpp"
#include <fstream>
#include <tuple>
#include <vector>
#include <string>
#include <sstream>

// helper to format PCB status table
std::string format_system_status(int time, const std::string& trace_desc, PCB &current, std::vector<PCB> &wait_queue) {
    std::stringstream ss;
    ss << "time: " << time << "; current trace: " << trace_desc << "\n";
    ss << "+------------------------------------------------------+\n";
    ss << "| PID |program name |partition number | size | state |\n";
    ss << "+------------------------------------------------------+\n";
    ss << "| " << current.PID << " | " << current.program_name << " | "
       << current.partition_number << " | " << current.size << " | running |\n";
    for (auto &p : wait_queue) {
        ss << "| " << p.PID << " | " << p.program_name << " | "
           << p.partition_number << " | " << p.size << " | waiting |\n";
    }
    ss << "+------------------------------------------------------+\n";
    return ss.str();
}

// simulate trace recursively
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
        auto [activity, duration, program_name] = parse_trace(trace);

        if (activity == "CPU") {
            execution += std::to_string(current_time) + ", " + std::to_string(duration) + ", CPU Burst\n";
            current_time += duration;
        } else if (activity == "SYSCALL") {
            execution += std::to_string(current_time) + ", 1, switch to kernel mode\n";
            current_time += 1;
            execution += std::to_string(current_time) + ", 10, context saved\n";
            current_time += 10;
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if (activity == "END_IO") {
            execution += std::to_string(current_time) + ", 1, switch to kernel mode\n";
            current_time += 1;
            execution += std::to_string(current_time) + ", 10, context saved\n";
            current_time += 10;
            execution += std::to_string(current_time) + ", " + std::to_string(duration) + ", ENDIO ISR\n";
            current_time += duration;
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if (activity == "FORK") {
            execution += std::to_string(current_time) + ", 1, switch to kernel mode\n";
            current_time += 1;
            execution += std::to_string(current_time) + ", 10, context saved\n";
            current_time += 10;
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;

            int child_pid = current.PID + 1 + wait_queue.size();
            PCB child(child_pid, current.PID, current.program_name, current.size, current.partition_number - 1);
            wait_queue.push_back(current); // parent waiting

            system_status += format_system_status(current_time, "FORK, " + std::to_string(duration), child, wait_queue);

            // collect child trace
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
            execution += std::to_string(current_time) + ", 1, switch to kernel mode\n";
            current_time += 1;
            execution += std::to_string(current_time) + ", 10, context saved\n";
            current_time += 10;
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;

            current.program_name = program_name;
            current.size = get_size(program_name, external_files);
            allocate_memory(&current);

            system_status += format_system_status(current_time, "EXEC " + program_name + ", " + std::to_string(duration), current, wait_queue);

            // exec may have its own trace file
            std::ifstream exec_file(program_name + ".txt");
            std::vector<std::string> exec_traces;
            std::string line;
            while (std::getline(exec_file, line)) exec_traces.push_back(line);
            if (!exec_traces.empty()) {
                auto [exec_exec, exec_status, new_time] = simulate_trace(exec_traces, current_time, vectors, delays, external_files, current, wait_queue);
                execution += exec_exec;
                system_status += exec_status;
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
    allocate_memory(&current);

    std::vector<PCB> wait_queue;
    std::vector<std::string> trace_file;
    std::string line;
    while (std::getline(input_file, line)) trace_file.push_back(line);

    auto [execution, system_status, _] = simulate_trace(trace_file, 0, vectors, delays, external_files, current, wait_queue);

    write_output(execution, "ouputs/execution.txt");
    write_output(system_status, "ouputs/system_status.txt");

    return 0;
}

