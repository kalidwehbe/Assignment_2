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
                   // Log interrupt steps (already done above)
            execution += std::to_string(current_time) + ", 1, switch to kernel mode\n";
            current_time += 1;
            execution += std::to_string(current_time) + ", 10, context saved\n";
            current_time += 10;
            execution += std::to_string(current_time) + ", 1, find vector 2 in memory position 0x0004\n";
            current_time += 1;
            execution += std::to_string(current_time) + ", 1, load address 0x0695 into the PC\n";
            current_time += 1;
            execution += std::to_string(current_time) + ", 13, cloning the PCB\n";
            current_time += 13;
            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
            
            // Create child process
            PCB child = current;
            child.pid += 1;
            child.program_name = current.program_name;
            child.partition_number = current.partition_number - 1; // just a mock change for variety
            child.state = "running";
            current.state = "waiting";
            
            // Append system status
            std::stringstream ss;
            ss << "time: " << current_time << "; current trace: " << trace << "\n";
            ss << "+------------------------------------------------------+\n";
            ss << "| PID | program name | partition number | size | state |\n";
            ss << "+------------------------------------------------------+\n";
            ss << "| " << child.pid << " | " << child.program_name << " | " << child.partition_number
               << " | " << child.size << " | running |\n";
            ss << "| " << current.pid << " | " << current.program_name << " | " << current.partition_number
               << " | " << current.size << " | waiting |\n";
            ss << "+------------------------------------------------------+\n";
            system_status += ss.str();
        



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

            auto [child_execution, child_system_status, child_time] = simulate_trace(
            child_trace, current_time, vectors, delays, external_files, current, wait_queue
            );
            execution += child_execution;
            system_status += child_system_status;
            current_time = child_time;


            ///////////////////////////////////////////////////////////////////////////////////////////


        } else if(activity == "EXEC") {
            auto [intr, time] = intr_boilerplate(current_time, 3, 10, vectors);
            current_time = time;
            execution += intr;

            ///////////////////////////////////////////////////////////////////////////////////////////
            execution += std::to_string(current_time) + ", 1, switch to kernel mode\n";
            current_time += 1;
            execution += std::to_string(current_time) + ", 10, context saved\n";
            current_time += 10;
            execution += std::to_string(current_time) + ", 1, find vector 3 in memory position 0x0006\n";
            current_time += 1;
            execution += std::to_string(current_time) + ", 1, load address 0x042B into the PC\n";
            current_time += 1;
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr * 3)
                        + ", Program is " + std::to_string(duration_intr) + " Mb large\n";
            current_time += duration_intr * 15;
            execution += std::to_string(current_time) + ", 3, marking partition as occupied\n";
            current_time += 3;
            execution += std::to_string(current_time) + ", 6, updating PCB\n";
            current_time += 6;
            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
            
            // Update PCB to reflect new program
            current.program_name = program_name;
            current.size = duration_intr;
            current.state = "running";
            
            // Update system status output
            std::stringstream ss;
            ss << "time: " << current_time << "; current trace: " << trace << "\n";
            ss << "+------------------------------------------------------+\n";
            ss << "| PID | program name | partition number | size | state |\n";
            ss << "+------------------------------------------------------+\n";
            ss << "| " << current.pid << " | " << current.program_name << " | "
               << current.partition_number << " | " << current.size << " | running |\n";
            ss << "+------------------------------------------------------+\n";
            system_status += ss.str();
            ///////////////////////////////////////////////////////////////////////////////////////////



            std::ifstream exec_trace_file(program_name + ".txt");

            std::vector<std::string> exec_traces;
            std::string exec_trace;
            while(std::getline(exec_trace_file, exec_trace)) {
                exec_traces.push_back(exec_trace);
            }

            ///////////////////////////////////////////////////////////////////////////////////////////
            auto [exec_execution, exec_system_status, exec_time] = simulate_trace(
                exec_traces, current_time, vectors, delays, external_files, current, wait_queue
            );
            execution += exec_execution;
            system_status += exec_system_status;
            current_time = exec_time;




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
