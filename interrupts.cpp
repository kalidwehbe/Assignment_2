/**
 *
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 *
 */

#include<interrupts.hpp>

std::tuple<std::string, std::string, int> simulate_trace(std::vector<std::string> trace_file, int time, std::vector<std::string> vectors, std::vector<int> delays, std::vector<external_file> external_files, PCB current, std::vector<PCB> wait_queue) {

    std::string trace;      //!< string to store single line of trace file
    std::string execution = "";  //!< string to accumulate the execution output
    std::string system_status = "";  //!< string to accumulate the system status output
    int current_time = time;

    //parse each line of the input trace file. 'for' loop to keep track of indices.
    for(size_t i = 0; i < trace_file.size(); i++) {
        auto trace = trace_file[i];

        auto [activity, duration_intr, program_name] = parse_trace(trace);

        if(activity == "CPU") { //As per Assignment 1
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", CPU Burst\n";
            current_time += duration_intr;
        } else if(activity == "SYSCALL") { //As per Assignment 1
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = time;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", SYSCALL ISR (ADD STEPS HERE)\n";
            current_time += delays[duration_intr];

            execution +=  std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "END_IO") {
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            current_time = time;
            execution += intr;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", ENDIO ISR(ADD STEPS HERE)\n";
            current_time += delays[duration_intr];

            execution +=  std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "FORK") {
            auto [intr, time] = intr_boilerplate(current_time, 2, 10, vectors);
            execution += intr;
            current_time = time;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //Add your FORK output here
            execution_log.push_back({current_time, 1, "switch to kernel mode"});
            execution_log.push_back({current_time + 1, 10, "context saved"});
            execution_log.push_back({current_time + 11, 1, "find vector 2 in memory position 0x0004"});
            execution_log.push_back({current_time + 12, 1, "load address 0X0695 into the PC"});
            execution_log.push_back({current_time + 13, trace.duration, "cloning the PCB"});
            current_time += 13 + trace.duration;

            // After cloning PCB
            write_system_status(system_status_log, current_time + 1, trace, parent, child); 
            execution_log.push_back({current_time + 1, 0, "scheduler called"});
            execution_log.push_back({current_time + 1, 1, "IRET"});
            current_time += 1;

            ///////////////////////////////////////////////////////////////////////////////////////////

            //The following loop helps you do 2 things:
            // * Collect the trace of the chile (and only the child, skip parent)
            // * Get the index of where the parent is supposed to start executing from
            std::vector<std::string> child_trace;
            bool skip = true;
            bool exec_flag = false;
            int parent_index = 0;

            for(size_t j = i; j < trace_file.size(); j++) {
                auto [_activity, _duration, _pn] = parse_trace(trace_file[j]);
                if(skip && _activity == "IF_CHILD") {
                    skip = false;
                    continue;
                } else if(_activity == "IF_PARENT"){
                    skip = true;
                    parent_index = j;
                    if(exec_flag) {
                        break;
                    }
                } else if(skip && _activity == "ENDIF") {
                    skip = false;
                    continue;
                } else if(!skip && _activity == "EXEC") {
                    skip = true;
                    child_trace.push_back(trace_file[j]);
                    exec_flag = true;
                }

                if(!skip) {
                    child_trace.push_back(trace_file[j]);
                }
            }
            i = parent_index;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //With the child's trace, run the child (HINT: think recursion)
            std::vector<TraceLine> child_trace;
            int i = current_index + 1;
            
            // Find IF_CHILD section
            while (i < trace_lines.size() && trace_lines[i].command != "IF_CHILD") i++;
            if (i < trace_lines.size() && trace_lines[i].command == "IF_CHILD") {
                i++;
                while (i < trace_lines.size() && trace_lines[i].command != "IF_PARENT" && trace_lines[i].command != "ENDIF") {
                    child_trace.push_back(trace_lines[i]);
                    i++;
                }
            }

            // Create child PCB (clone parent)
            PCB child = pcb;
            child.pid = next_pid++;
            child.state = "running";
            allocate_memory(child, child.size);
            
            // Run child trace recursively
            simulate_trace(child_trace, child, external_files, execution_log, system_status_log);
            
            // Free child’s partition after finish
            free_partition_of(child);




            ///////////////////////////////////////////////////////////////////////////////////////////


        } else if(activity == "EXEC") {
            auto [intr, time] = intr_boilerplate(current_time, 3, 10, vectors);
            current_time = time;
            execution += intr;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //Add your EXEC output here
            execution_log.push_back({current_time, 1, "switch to kernel mode"});
            execution_log.push_back({current_time + 1, 10, "context saved"});
            execution_log.push_back({current_time + 11, 1, "find vector 3 in memory position 0x0006"});
            execution_log.push_back({current_time + 12, 1, "load address 0X042B into the PC"});
            
            // Simulate loader time: 15 ms per MB
            int program_size = trace.duration; // e.g., 50 for 10 MB program
            int load_time = (program_size / 5) * 15; // Adjust to your scaling if needed
            execution_log.push_back({current_time + 13, program_size, "Program is " + std::to_string(program_size / 5) + " Mb large"});
            execution_log.push_back({current_time + 13 + program_size, load_time, "loading program into memory"});
            execution_log.push_back({current_time + 13 + program_size + load_time, 3, "marking partition as occupied"});
            execution_log.push_back({current_time + 13 + program_size + load_time + 3, 6, "updating PCB"});
            execution_log.push_back({current_time + 13 + program_size + load_time + 9, 0, "scheduler called"});
            execution_log.push_back({current_time + 13 + program_size + load_time + 9, 1, "IRET"});
            current_time += 13 + program_size + load_time + 10;
            
            // Write system snapshot
            write_system_status(system_status_log, current_time, trace, pcb, pcb);

            ///////////////////////////////////////////////////////////////////////////////////////////


            std::ifstream exec_trace_file(program_name + ".txt");

            std::vector<std::string> exec_traces;
            std::string exec_trace;
            while(std::getline(exec_trace_file, exec_trace)) {
                exec_traces.push_back(exec_trace);
            }

            ///////////////////////////////////////////////////////////////////////////////////////////
           //With the exec's trace (i.e. trace of external program), run the exec (HINT: think recursion)
            std::string exec_program = trace.arg1; // program1 or program2
            std::vector<TraceLine> exec_trace = parse_trace(exec_program + ".txt");
            
            // Run recursively using the same PCB (process image replaced)
            simulate_trace(exec_trace, pcb, external_files, execution_log, system_status_log);
            
            // After EXEC, terminate this trace — old code is gone
            return;

            ///////////////////////////////////////////////////////////////////////////////////////////

            break; //Why is this important? (answer in report)

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
