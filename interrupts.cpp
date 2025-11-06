/**
 *
 * @file interrupts.cpp
 * @author Sasisekhar Govind (fixed)
 *
 */

#include "interrupts.hpp"

std::tuple<std::string, std::string, int> simulate_trace(
    std::vector<std::string> trace_file, int time,
    std::vector<std::string> vectors, std::vector<int> delays,
    std::vector<external_file> external_files, PCB current,
    std::vector<PCB> wait_queue)
{
    std::string execution = "";
    std::string system_status = "";
    int current_time = time;

    for (size_t i = 0; i < trace_file.size(); i++) {
        auto trace = trace_file[i];
        auto [activity, duration_intr, program_name] = parse_trace(trace);

        // ---------- CPU ----------
        if (activity == "CPU") {
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", CPU Burst\n";
            current_time += duration_intr;
        }

        // ---------- SYSCALL ----------
        else if (activity == "SYSCALL") {
            auto [intr, t] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = t;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", SYSCALL ISR\n";
            current_time += delays[duration_intr];
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        }

        // ---------- END_IO ----------
        else if (activity == "END_IO") {
            auto [intr, t] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = t;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", ENDIO ISR\n";
            current_time += delays[duration_intr];
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        }

        // ---------- FORK ----------
        else if (activity == "FORK") {
            execution += std::to_string(current_time) + ", 1, switch to kernel mode //fork encountered, "
                      + std::to_string(wait_queue.size() + 1) + " processes in PCB\n";
            current_time += 1;

            execution += std::to_string(current_time) + ", 10, context saved\n";
            current_time += 10;

            execution += std::to_string(current_time) + ", 1, find vector 2 in memory position 0x0004\n";
            current_time += 1;

            execution += std::to_string(current_time) + ", 1, load address 0X0695 into the PC\n";
            current_time += 1;

            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", cloning the PCB\n";
            current_time += duration_intr;

            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;

            // Create child + push parent to wait queue
            PCB parent = current;
            PCB child(wait_queue.size() + 1, current.PID, current.program_name, current.size, current.partition_number);
            wait_queue.push_back(parent);
            current = child;

            // Log FORK PCB snapshot
            system_status += "time: " + std::to_string(current_time) + "; current trace: FORK, " + std::to_string(duration_intr) + "\n";
            system_status += print_PCB(current, wait_queue);

            // Extract child trace between IF_CHILD and IF_PARENT
            std::vector<std::string> child_trace;
            size_t parent_index = i;
            bool child_mode = false;

            for (size_t j = i + 1; j < trace_file.size(); j++) {
                auto [a, _, __] = parse_trace(trace_file[j]);
                if (a == "IF_CHILD") { child_mode = true; continue; }
                if (a == "IF_PARENT") { child_mode = false; parent_index = j; continue; }
                if (a == "ENDIF") { break; }
                if (child_mode) child_trace.push_back(trace_file[j]);
            }

            // Recursive simulate for child
            auto [child_exec, child_status, child_time] =
                simulate_trace(child_trace, current_time, vectors, delays, external_files, current, wait_queue);
            execution += child_exec;
            system_status += child_status;
            current_time = child_time;

            // Restore parent
            current = parent;

            // Continue parent AFTER child’s block
            i = parent_index;
        }

        // ---------- EXEC ----------
        else if (activity == "EXEC") {
            execution += std::to_string(current_time) + ", 1, switch to kernel mode //exec encountered\n";
            current_time += 1;
            execution += std::to_string(current_time) + ", 10, context saved\n";
            current_time += 10;

            unsigned int prog_size = get_size(program_name, external_files);
            current.program_name = program_name;
            current.size = prog_size;
            allocate_memory(&current);

            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr)
                      + ", Program is " + std::to_string(prog_size) + " Mb large\n";
            current_time += duration_intr;

            execution += std::to_string(current_time) + ", " + std::to_string(prog_size * 15)
                      + ", loading program into memory\n";
            current_time += prog_size * 15;
            execution += std::to_string(current_time) + ", 3, marking partition as occupied\n";
            current_time += 3;
            execution += std::to_string(current_time) + ", 6, updating PCB\n";
            current_time += 6;
            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;

            // Print EXEC PCB snapshot
            system_status += "time: " + std::to_string(current_time) + "; current trace: EXEC "
                          + program_name + ", " + std::to_string(duration_intr) + "\n";
            system_status += print_PCB(current, wait_queue);

            // Load program trace and recurse
            std::ifstream exec_trace_file(program_name + ".txt");
            if (exec_trace_file.is_open()) {
                std::vector<std::string> exec_traces;
                std::string line;
                while (std::getline(exec_trace_file, line))
                    exec_traces.push_back(line);
                exec_trace_file.close();

                PCB parent = current;
                auto [exec_exec, exec_status, exec_time] =
                    simulate_trace(exec_traces, current_time, vectors, delays, external_files, current, wait_queue);
                execution += exec_exec;
                system_status += exec_status;
                current_time = exec_time;
                current = parent;
            }
        }
    }

    return {execution, system_status, current_time};
}


int main(int argc, char** argv) {

    auto [vectors, delays, external_files] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);

    print_external_files(external_files);

    PCB current(0, -1, "init", 1, -1);
    if (!allocate_memory(&current)) {
        std::cerr << "ERROR! Memory allocation failed!\n";
    }

    std::vector<PCB> wait_queue;

    // Convert trace file to vector
    std::vector<std::string> trace_file;
    std::string trace;
    while (std::getline(input_file, trace)) {
        trace_file.push_back(trace);
    }

    auto [execution, system_status, _] = simulate_trace(trace_file, 0, vectors, delays, external_files, current, wait_queue);

    input_file.close();

    write_output(execution, "execution.txt");
    write_output(system_status, "system_status.txt");

    return 0;
}
