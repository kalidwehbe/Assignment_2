/**
 *
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 *
 */

#include "interrupts.hpp"

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
            // --------- FORK ISR and PCB clone ---------
            execution += std::to_string(current_time) + ", 1, switch to kernel mode //fork encountered, " 
                         + std::to_string(wait_queue.size() + 1) + " processes in PCB\n";
            
            execution += std::to_string(current_time + 1) + ", 10, context saved\n";
            current_time += 11; // switch + context
            
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
            // --------- Child recursive execution ---------
            std::vector<std::string> child_trace; // from trace file between IF_CHILD and ENDIF
            for (size_t j = i + 1; j < trace_file.size(); j++) {
                auto [act, dur, pn] = parse_trace(trace_file[j]);
                if (act == "IF_CHILD") continue;
                if (act == "ENDIF") break;
                child_trace.push_back(trace_file[j]);
            }
            
            // Run child
            auto [child_exec, child_status, child_time] = simulate_trace(child_trace, current_time, vectors, delays, external_files, child, wait_queue);
            execution += child_exec;
            system_status += child_status;
            current_time = child_time;



            ///////////////////////////////////////////////////////////////////////////////////////////


        } else if(activity == "EXEC") {
            auto [intr, time] = intr_boilerplate(current_time, 3, 10, vectors);
            current_time = time;
            execution += intr;

            ///////////////////////////////////////////////////////////////////////////////////////////
                        // --------- EXEC ISR ---------
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




            ///////////////////////////////////////////////////////////////////////////////////////////


            std::ifstream exec_trace_file(program_name + ".txt");

            std::vector<std::string> exec_traces;
            std::string exec_trace;
            while(std::getline(exec_trace_file, exec_trace)) {
                exec_traces.push_back(exec_trace);
            }

            ///////////////////////////////////////////////////////////////////////////////////////////
            // --------- Run external program trace ---------
            std::ifstream exec_trace_file(program_name + ".txt");
            std::vector<std::string> exec_traces;
            std::string line;
            while (std::getline(exec_trace_file, line)) exec_traces.push_back(line);
            
            // Recursively simulate the external program
            auto [prog_exec, prog_status, prog_time] = simulate_trace(exec_traces, current_time, vectors, delays, external_files, current, wait_queue);
            execution += prog_exec;
            system_status += prog_status;
            current_time = prog_time;




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
    // Tracks current CPU time for proper timestamping
    int current_time = 0;
    
    // Queue for waiting processes (forked but not currently running)
    std::vector<PCB> wait_queue;
    
    // Stores the lines of the trace file for processing
    std::vector<std::string> trace_file;
    
    // Optional: for debugging, can store each PCB snapshot separately
    // std::vector<std::string> pcb_snapshots;


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
