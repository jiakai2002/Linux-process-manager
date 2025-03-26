#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>


/******************************************************************************
 * Types
 ******************************************************************************/

typedef enum process_status {
    RUNNING = 0,
    READY = 1,
    STOPPED = 2,
    KILLED = 3
} process_status;

typedef struct process_record {
    pid_t pid;
    process_status status;
} process_record;

/******************************************************************************
 * Globals
 ******************************************************************************/
#define MAX_PROCESSES 99
#define MAX_RUNNING 3
#define BUFFER_SIZE 100
process_record process_records[MAX_PROCESSES];
int running_count = 0;
pid_t ready_queue[MAX_PROCESSES];
pid_t running_queue[MAX_RUNNING];
int front = 0, rear = -1, ready_count = 0;

/******************************************************************************
 * Functions
 ******************************************************************************/

void dispatch_next_ready_process(void) {
    if (ready_count > 0 && running_count < MAX_RUNNING) {
        pid_t next_pid = ready_queue[front];
        front = (front + 1) % MAX_PROCESSES;
        ready_count--;

        // Find the process record for the next ready process
        for (int i = 0; i < MAX_PROCESSES; i++) {
            process_record* const p = &process_records[i];
            if (p->pid == next_pid) {
                // Start the process by sending SIGCONT
                kill(next_pid, SIGCONT);
                p->status = RUNNING;

                // Add it to the running_queue
                running_queue[running_count] = next_pid;
                running_count++;

                return;
            }
        }
    }
}

void check_terminated_processes(void) {
    int status;
    pid_t result;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_record* const p = &process_records[i];
        if (p->pid > 0 && p->status == RUNNING) {
            result = waitpid(p->pid, &status, WNOHANG);
            if (result > 0) {  // Process has terminated
                p->status = KILLED;
                running_count--;

                // Remove the terminated process from the running_queue
                for (int j = 0; j < running_count; j++) {
                    if (running_queue[j] == p->pid) {
                        for (int k = j; k < running_count - 1; k++) {
                            running_queue[k] = running_queue[k + 1];
                        }
                        break;
                    }
                }

                dispatch_next_ready_process();
            }
        }
    }
}

void perform_stop(char* args[]) {
    const pid_t pid = atoi(args[0]);
    if (pid <= 0) {
        printf("The process ID must be a positive integer.\n");
        return;
    }

    for (int i = 0; i < MAX_PROCESSES; ++i) {
        process_record* const p = &process_records[i];
        if ((p->pid == pid) && (p->status == RUNNING)) {
            kill(p->pid, SIGSTOP);
            printf("stopping %d\n", p->pid);
            p->status = STOPPED;
            running_count--;

            dispatch_next_ready_process();
            return;
        }
    }
    printf("Process %d not found.\n", pid);
}

void perform_resume(char* args[]) {
    const pid_t pid = atoi(args[0]);
    if (pid <= 0) {
        printf("The process ID must be a positive integer.\n");
        return;
    }

    for (int i = 0; i < MAX_PROCESSES; ++i) {
        process_record* const p = &process_records[i];
        if (p->pid == pid) {
            if (p->status == STOPPED) {
                if (running_count < MAX_RUNNING) {
                    // Resume the stopped process
                    kill(pid, SIGCONT);
                    printf("resuming %d\n", p->pid);
                    p->status = RUNNING;

                    // Add it to the running_queue
                    running_queue[running_count] = pid;
                    running_count++;
                } else {
                    // Stop a running process to make room for the resumed process
                    pid_t last_pid = running_queue[MAX_RUNNING - 1];
                    kill(last_pid, SIGSTOP);
                    printf("Resuming %d\n", pid);

                    // Update the status of the stopped process
                    for (int j = 0; j < MAX_PROCESSES; ++j) {
                        if (process_records[j].pid == last_pid) {
                            process_records[j].status = READY;
                            break;
                        }
                    }

                    // Remove the last process from the running queue
                    for (int j = MAX_RUNNING - 1; j > 0; j--) {
                        running_queue[j] = running_queue[j - 1];
                    }
                    running_queue[0] = pid;

                    // Resume the stopped process
                    kill(pid, SIGCONT);
                    p->status = RUNNING;

                    // Update the running count
                    running_count++;
                }
            }
            return;
        }
    }
    printf("Process %d not found.\n", pid);
}

void perform_run(char* args[]) {
    char exec_path[BUFFER_SIZE];  // Buffer to store "./" + args[0]
    
    // Prepend "./" to args[0] unless it's already an absolute or relative path
    if (args[0][0] != '/' && args[0][0] != '.') {
        snprintf(exec_path, sizeof(exec_path), "./%s", args[0]);
    } else {
        strncpy(exec_path, args[0], sizeof(exec_path) - 1);
        exec_path[sizeof(exec_path) - 1] = '\0';
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return;
    }

    if (pid == 0) {  // Child process executes the command with execvp
        execvp(exec_path, args);
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else {  // Parent process adds the new process to the process table
        int index = -1;
        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (process_records[i].status == KILLED || process_records[i].pid == 0) {
                index = i;
                break;
            }
        }

        if (index == -1) {
            printf("Process table full, cannot track more processes.\n");
            return;
        }

        process_records[index].pid = pid;
        process_records[index].status = RUNNING;

        // Handle process scheduling based on current running processes
        if (running_count < MAX_RUNNING) {
            running_queue[running_count++] = pid;
        } else {
            process_records[index].status = READY;
            ready_queue[(rear + 1) % MAX_PROCESSES] = pid;
            ready_count++;
        }
    }
}

void perform_kill(char* args[]) {
	const pid_t pid = atoi(args[0]);
    if (pid <= 0) {
        printf("The process ID must be a positive integer.\n");
        return;
    }

    for (int i = 0; i < MAX_PROCESSES; ++i) {
        process_record* const p = &process_records[i];
        if ((p->pid == pid) && (p->status == RUNNING)) {
            kill(p->pid, SIGTERM);
            printf("%d killed\n", p->pid);
            p->status = KILLED;
            running_count--;
            dispatch_next_ready_process();
            return;
        }
    }
    printf("Process %d not found.\n", pid);
}

void perform_list(void) {
    bool anything = false;
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        process_record* const p = &process_records[i];
        
        if (p->pid > 0) {
            printf("%d,", p->pid);
            switch (p->status) {
                case RUNNING:  printf("%d\n", 0); break;
                case READY:    printf("%d\n", 1); break;
                case STOPPED:  printf("%d\n", 2); break;
                case KILLED:   printf("%d\n", 3); break;
                default:       break;
            }
            anything = true;
        }
    }

    if (!anything) {
        printf("No active processes.\n");
    }
}

void perform_exit(void) {
    for (int i = 0; i < 20; i++) {
        if (process_records[i].status == RUNNING || process_records[i].status == READY) {
            kill(process_records[i].pid, SIGTERM);
            process_records[i].status = KILLED;
        }
    }
    printf("bye!\n");
    exit(0);
}

char * get_input(char * buffer, char * args[], int args_count_max) {
	for (char* c = buffer; *c != '\0'; ++c) {
		if ((*c == '\r') || (*c == '\n')) {
			*c = '\0';
			break;
		}
	}
	strcat(buffer, " ");
	// tokenize command's arguments
	char * p = strtok(buffer, " ");
	int arg_cnt = 0;
	while (p != NULL) {
		args[arg_cnt++] = p;
		if (arg_cnt == args_count_max - 1) {
			break;
		}
		p = strtok(NULL, " ");
	}
	args[arg_cnt] = NULL;
	return args[0];
}

void handle_command(char *buffer) {

    char * args[10];
    const int args_count = sizeof(args) / sizeof(*args);
    char* cmd = get_input(buffer, args, args_count);

    if (strcmp(cmd, "kill") == 0) {
        perform_kill(&args[1]);
    } else if (strcmp(cmd, "stop") == 0) {
        perform_stop(&args[1]);
    } else if (strcmp(cmd, "run") == 0) {
        perform_run(&args[1]);
    } else if (strcmp(cmd, "resume") == 0) {
        perform_resume(&args[1]);
    } else if (strcmp(cmd, "list") == 0) {
        perform_list();
    } else if (strcmp(cmd, "exit") == 0) {
        perform_exit();
    } else {
        printf("invalid command\n");
    }
}

/******************************************************************************
 * Entry point
 ******************************************************************************/

int main (void) {
    int p[2];

    if(pipe(p)) {
        return EXIT_FAILURE;
    }

    int reading_pipe = p[0];
    int writing_pipe = p[1];

    const pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "fork_failed\n");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) { //parent handles command input
        close(reading_pipe);
        char buffer[BUFFER_SIZE];

        while(true){
            printf("\x1B[34m" "cs205" "\x1B[0m" "$ "); // cursor
            fgets(buffer, BUFFER_SIZE-1, stdin); 

            if (write(writing_pipe, buffer, BUFFER_SIZE) <= 0) {
                fprintf(stderr, "unable to write\n");
                break;
            }

            usleep(20000); // makes sure output is returned before waiting for input again
        }
        close(writing_pipe);
    } 
    
    if (pid == 0) { // child handles command execution
        close(writing_pipe);
        char buffer[BUFFER_SIZE];

        fcntl(reading_pipe, F_SETFL, O_NONBLOCK); // set non-blocking reads

        while (true) {
            if (read(reading_pipe, buffer, BUFFER_SIZE) > 0) {
                handle_command(buffer);
            }
            check_terminated_processes(); 
            usleep(10000); //prevent CPU hogging
        }
        close(reading_pipe);
    }
    
    return EXIT_SUCCESS;
}