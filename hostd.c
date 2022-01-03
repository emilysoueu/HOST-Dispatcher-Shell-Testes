
#include "hostd.h"

int main(int argc, char *argv[])
{
    div_t output, output1;
    char *input_file;
    FILE *input_list_stream;

    if (argc == 2)
    {
        input_file = argv[1]; // Parse the command from running the program to get input file
    }
    else
    {
        print_usage(); // If the user did not specify a file, print the correct usage of the program, then exit
        exit(0);
    }

    if (!(input_list_stream = fopen(input_file, "r"))) // Open the file
    {
        printf("Error opening file"); // If there was an error opening the file, print error and exit program
        exit(0);
    }

    fill_input_queue(input_file, input_list_stream); // Fill the input queue with new processes
    initialize_system();                             // Initialize the system

    while (!complete()) // Loop until all processes have finished
    {
        check_input_queue(); // Remove processes from input queue and put in appropriate queue

        check_user_job_queue(); // Remove processes from user job queue and put in appropriate queue

        if (current_process) // se existe processo rodando
        {
            handle_current_process();
        }

        if ((real_time_queue || priority_one_queue) && (!current_process)) // Processos que ainda não foram completados
        {
            assign_current_process();
        }

        sleep(1);   // Sleep 1 second to simulate 1 second of work
        timer += 1; // Add 1 second to the current time
    }

    printf("All processes have finished.\n");

    return 0;
}

void print_usage()
{
    printf("\nPlease supply a file specifying a sequence of processes as an argument when running this file.\n\n");
}

void fill_input_queue(char *input_file, FILE *input_list_stream)
{
    struct pcb *process; // Pointer to new process to create
    char line[50];       // Line buffer

    while (fgets(line, 50, input_list_stream)) // Loop through the file line by line
    {
        if (line[0] == '\n') // Exit loop when file ends
        {
            break;
        }

        process = create_null_pcb(); // Create null process

        char *s = strtok(line, ","); // Parse the line for eight parameters of process
        int nums[8];
        int i = 0;

        while (s)
        {
            int n = atoi(s);
            s = strtok(NULL, ",");
            nums[i] = n;
            ++i;
        }

        process->arrival_time = nums[0]; // Assign process attributes accordingly
        process->priority = nums[1];
        process->remaining_cpu_time = nums[2];
        process->processed_cpu_time = nums[2];
        process->mbytes = nums[3];
        process->num_printers = nums[4];
        process->num_scanners = nums[5];
        process->num_modems = nums[6];
        process->num_drives = nums[7];

        input_queue = enqueue_pcb(input_queue, process); // Add process to input queue

        process = NULL;
    }

    fclose(input_list_stream); // Close the file
}

void initialize_system()
{
    memory = create_null_mab(); // Create main memory block
    memory->size = TOTAL_MEMORY - RESERVED_MEMORY;
    reserved_memory = create_null_mab(); // Create reserved memory block
    reserved_memory->size = RESERVED_MEMORY;
    rsrcs = create_null_resources(); // Create resources
    rsrcs->remaining_printers = TOTAL_PRINTERS;
    rsrcs->remaining_scanners = TOTAL_SCANNERS;
    rsrcs->remaining_modems = TOTAL_MODEMS;
    rsrcs->remaining_drives = TOTAL_DRIVES;
}

bool complete()
{
    div_t output, output1;

    if (!input_queue && !real_time_queue && !user_job_queue && !priority_one_queue && !current_process)
    {
        output = div(total, qtd);

        printf("%d - %d\n", qtd, output.quot);
    }
    return (!input_queue && !real_time_queue && !user_job_queue && !priority_one_queue && !current_process);
}

void check_input_queue()
{
    while (input_queue && input_queue->arrival_time <= timer) // Check for processess arriving at current time
    {
        struct pcb *p = input_queue;
        input_queue = dequeue_pcb(input_queue); // Remove from input queue
        if (p->priority == 0)
        {
            real_time_queue = enqueue_pcb(real_time_queue, p); // Place in real-time queue
        }
        else
        {
            user_job_queue = enqueue_pcb(user_job_queue, p); // Place in user job queue
        }
    }
}

void check_user_job_queue()
{
    while (user_job_queue && fit_memory(memory, user_job_queue->mbytes) && // check user job queue and make sure resources are sufficient
           check_resources(user_job_queue, rsrcs))
    {
        struct pcb *p = user_job_queue;
        user_job_queue = dequeue_pcb(user_job_queue); // Remove from user job queue
        allocate_resources(p, rsrcs);                 // Allocate resources to the process
        priority_one_queue = enqueue_pcb(priority_one_queue, p);
    }
}

void handle_current_process()
{
    // O ultimo instante do processo na cpu será atualizado.
    // O motivo de time + 1: time só é atualizado no final do while que está na main.
    current_process->instanteDeCPU = timer + 1;

    if (--current_process->remaining_cpu_time == 0) // Check if the process has finished
    {
        total = total + (timer - current_process->arrival_time) - current_process->processed_cpu_time;
        qtd = qtd + 1;
        terminate_pcb(current_process);          // Terminate process
        free_memory(current_process->mem_block); // Free process memory
        free_resources(current_process, rsrcs);  // Free process resources
        free(current_process);
        current_process = NULL;
    }
    else // The process hasn't finished
    {
        if ((real_time_queue || user_job_queue || priority_one_queue) &&
            (current_process->priority != 0)) // There are still jobs in the queue(s)
        {
            struct pcb *p = suspend_pcb(current_process); // Suspend current process

            //----------------- PRECISO ALTERAR O PROCESSO DE ESCALONAMENTO AQUI----------------------

            priority_one_queue = enqueue_pcb(priority_one_queue, p);

            current_process = NULL; // Reset pointer to NULL since process has been suspended

            struct pcb *pcbIncognita = NULL;
        }
    }
}

void assign_current_process()
{
    if (real_time_queue) // Check real-time queue first
    {
        current_process = real_time_queue;
        real_time_queue = dequeue_pcb(real_time_queue);
    }
    else if (priority_one_queue) // Then check priority one queue
    {
        current_process = priority_one_queue;
        priority_one_queue = dequeue_pcb(priority_one_queue);
    }

    if (current_process->pid != 0) // Check if the process has already been started before
    {
        restart_pcb(current_process); // If so, just restart it
    }
    else // Otherwise, the process is being started for the first time, so allocate memory and start it
    {
        start_pcb(current_process);
        if (current_process->priority == 0)
        {
            current_process->mem_block = allocate_memory(reserved_memory, current_process->mbytes);
        }
        else
        {
            current_process->mem_block = allocate_memory(memory, current_process->mbytes);
        }
    }
}
