#pragma once

// Can include any other headers as needed

#include <stdint.h>
#include <time.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <stdbool.h>
#include <fcntl.h>
#include <string.h>

#define MAX_PROCS 100
#define MAX_QUEUE 100

typedef struct
{
    char *command;
    bool finished; 
    bool error;   
    uint64_t start_time;
    uint64_t completion_time;
    uint64_t turnaround_time;
    uint64_t waiting_time;
    uint64_t response_time;
    bool started;
    int process_id;

} Process;

void FCFS(Process p[], int n);
void RoundRobin(Process p[], int n, int quantum);
void MultiLevelFeedbackQueue(Process p[], int n, int quantum0, int quantum1, int quantum2, int boostTime);

uint64_t get_time_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000LL + tv.tv_usec / 1000);
}

char **parse_command(char *command)
{
    static char *argv[100];
    int i = 0;
    char *token = strtok(command, " \t\n");
    while (token != NULL && i < 99)
    {
        argv[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    argv[i] = NULL;
    return argv;
}

void FCFS(Process p[], int n)
{
    uint64_t scheduler_start = get_time_ms();
    FILE *csv = fopen("result_offline_FCFS_output.csv", "w");

    for (int i = 0; i < n; i++)
    {
        p[i].started = true;
        uint64_t start = get_time_ms() - scheduler_start;
        p[i].start_time = start;

        int pid = fork();
        if (pid == 0)
        {
            char cmd_copy[1000];
            strncpy(cmd_copy, p[i].command, sizeof(cmd_copy) - 1);
            cmd_copy[sizeof(cmd_copy) - 1] = '\0';
            char **argv = parse_command(cmd_copy);
            execvp(argv[0], argv);

            perror("execvp failed");
            _exit(EXIT_FAILURE);
        }
        else if (pid > 0)
        {
            int status;
            waitpid(pid, &status, 0);

            uint64_t end = get_time_ms() - scheduler_start;
            p[i].completion_time = end;

            p[i].finished = WIFEXITED(status);
            p[i].error = !p[i].finished || (WEXITSTATUS(status) != 0);

            p[i].turnaround_time = p[i].completion_time;
            p[i].waiting_time = p[i].start_time;
            p[i].response_time = p[i].start_time;

            printf("%s, %llu, %llu\n",
                   p[i].command,
                   (unsigned long long)p[i].start_time,
                   (unsigned long long)p[i].completion_time);

            fprintf(csv, "%s,%s,%s,%llu,%llu,%llu,%llu\n",
                    p[i].command,
                    p[i].finished ? "Yes" : "No",
                    p[i].error ? "Yes" : "No",
                    (unsigned long long)p[i].completion_time,
                    (unsigned long long)p[i].turnaround_time,
                    (unsigned long long)p[i].waiting_time,
                    (unsigned long long)p[i].response_time);
            fflush(csv);
        }
        else
        {
            perror("fork failed");
            continue;
        }
    }

    fclose(csv);
}

bool isEmpty(int front, int rear)
{
    return front == rear;
}

bool isFull(int front, int rear)
{
    return ((rear + 1) % MAX_QUEUE) == front;
}

void enque(int queue[], int *front, int *rear, int val)
{
    queue[*rear] = val;
    *rear = (*rear + 1) % MAX_QUEUE;
}

int deque(int queue[], int *front, int *rear)
{
    if (isEmpty(*front, *rear))
    {
        return -1;
    }
    int val = queue[*front];
    *front = (*front + 1) % MAX_QUEUE;
    return val;
}

void RoundRobin(Process p[], int n, int quantum)
{
    uint64_t scheduler_start = get_time_ms();

    FILE *csv = fopen("result_offline_RR_output.csv", "w");

    pid_t pids[MAX_PROCS] = {0};
    bool started[MAX_PROCS] = {0};
    bool finished[MAX_PROCS] = {0};
    int slices_used[MAX_PROCS] = {0};
    int remaining = n;

    int queue[MAX_QUEUE];

    int front = 0;
    int rear = 0;

    for (int i = 0; i < n; i++)
    {
        enque(queue, &front, &rear, i);
    }

    while (remaining > 0 && front < rear)
    {
        int i = deque(queue, &front, &rear);
        if (finished[i] || (i == -1))
        {
            continue;
        }

        uint64_t slice_start = get_time_ms() - scheduler_start;
        if (!started[i])
        {
            p[i].started = true;
            p[i].start_time = slice_start;
            started[i] = true;

            int pid = fork();

            if (pid == 0)
            {
                char cmd_copy[1000];
                strncpy(cmd_copy, p[i].command, sizeof(cmd_copy) - 1);
                cmd_copy[sizeof(cmd_copy) - 1] = '\0';
                char **argv = parse_command(cmd_copy);
                execvp(argv[0], argv);
                perror("execvp failed");
                _exit(EXIT_FAILURE);
            }
            else if (pid > 0)
            {
                p[i].process_id = pid;
                pids[i] = pid;
                kill(pid, SIGSTOP);
            }
            else
            {
                perror("fork failed");
                continue;
            }
        }

        if (remaining == 1)
        {
            kill(pids[i], SIGCONT);

            int status;
            waitpid(pids[i], &status, 0); 

            uint64_t slice_end = get_time_ms() - scheduler_start;

            printf("%s, %llu, %llu\n",
                   p[i].command,
                   (unsigned long long)slice_start,
                   (unsigned long long)slice_end);
            fflush(stdout);

            p[i].completion_time = slice_end;
            p[i].finished = WIFEXITED(status);
            p[i].error = !p[i].finished;
            finished[i] = true;
            remaining--;

            p[i].turnaround_time = p[i].completion_time;
            p[i].waiting_time = p[i].turnaround_time - (slices_used[i] * quantum);
            p[i].response_time = p[i].start_time;

            fprintf(csv, "%s,%s,%s,%llu,%llu,%llu,%llu\n",
                    p[i].command,
                    p[i].finished ? "Yes" : "No",
                    p[i].error ? "Yes" : "No",
                    (unsigned long long)p[i].completion_time,
                    (unsigned long long)p[i].turnaround_time,
                    (unsigned long long)p[i].waiting_time,
                    (unsigned long long)p[i].response_time);
            fflush(csv);
            break; 
        }

        kill(pids[i], SIGCONT);
        slices_used[i]++;

        struct timespec ts = {.tv_sec = quantum / 1000, .tv_nsec = (quantum % 1000) * 1000000};
        nanosleep(&ts, NULL);

        kill(pids[i], SIGSTOP);

        uint64_t slice_end = get_time_ms() - scheduler_start;
        printf("%s, %llu, %llu\n",
               p[i].command,
               (unsigned long long)slice_start,
               (unsigned long long)slice_end);
        fflush(stdout);

        int status;
        pid_t result = waitpid(pids[i], &status, WNOHANG);

        if (result == pids[i])
        {
            p[i].completion_time = slice_end;
            p[i].finished = WIFEXITED(status);
            p[i].error = !p[i].finished;
            finished[i] = true;
            remaining--;

            p[i].turnaround_time = p[i].completion_time;
            p[i].waiting_time = p[i].turnaround_time - (slices_used[i] * quantum);
            p[i].response_time = p[i].start_time;

            fprintf(csv, "%s,%s,%s,%llu,%llu,%llu,%llu\n",
                    p[i].command,
                    p[i].finished ? "Yes" : "No",
                    p[i].error ? "Yes" : "No",
                    (unsigned long long)p[i].completion_time,
                    (unsigned long long)p[i].turnaround_time,
                    (unsigned long long)p[i].waiting_time,
                    (unsigned long long)p[i].response_time);
            fflush(csv);
        }
        else
        {
            enque(queue, &front, &rear, i);
        }
    }

    fclose(csv);
}

// ############################################################
void MultiLevelFeedbackQueue(Process p[], int n, int quantum0, int quantum1, int quantum2, int boostTime)
{
    uint64_t scheduler_start = get_time_ms();
    FILE *csv = fopen("result_offline_MLFQ_output.csv", "w");

    pid_t pids[MAX_PROCS] = {0};
    bool started[MAX_PROCS] = {0};
    bool finished[MAX_PROCS] = {0};
    int remaining = n;
    uint64_t cpu_time_used_ms[MAX_PROCS] = {0};

    int q0[MAX_QUEUE], q1[MAX_QUEUE], q2[MAX_QUEUE];
    int f0 = 0, r0 = 0, f1 = 0, r1 = 0, f2 = 0, r2 = 0;


    for (int i = 0; i < n; i++)
    {
        enque(q0, &f0, &r0, i);
    }

    uint64_t next_boost_time = boostTime;


    while (remaining > 0 && (f0 != r0 || f1 != r1 || f2 != r2))
    {
        int idx = -1;
        int this_quantum = 0;

        if (f0 != r0)
        {
            idx = deque(q0, &f0, &r0);
            this_quantum = quantum0;
        }
        else if (f1 != r1)
        {
            idx = deque(q1, &f1, &r1);
            this_quantum = quantum1;
        }
        else if (f2 != r2)
        {
            idx = deque(q2, &f2, &r2);
            this_quantum = quantum2;
        }

        if (idx == -1 || finished[idx])
        {
            continue;
        }

        uint64_t slice_start = get_time_ms() - scheduler_start;


        if (!started[idx])
        {
            started[idx] = true;
            p[idx].started = true;
            p[idx].start_time = slice_start;

            pid_t pid = fork();
            if (pid == 0)
            {
                char cmd_copy[1000];
                strncpy(cmd_copy, p[idx].command, sizeof(cmd_copy) - 1);
                cmd_copy[sizeof(cmd_copy) - 1] = '\0';
                char **argv = parse_command(cmd_copy);
                execvp(argv[0], argv);
                perror("execvp failed");
                _exit(EXIT_FAILURE);
            }
            else if (pid > 0)
            {
                p[idx].process_id = pid;
                pids[idx] = pid;
                kill(pid, SIGSTOP);
            }
            else
            {
                perror("fork failed");
                finished[idx] = true;
                remaining--;
                continue;
            }
        }

        kill(pids[idx], SIGCONT);

        uint64_t run_start = get_time_ms();
        int status;
        pid_t res = 0;

        while (1)
        {
            res = waitpid(pids[idx], &status, WNOHANG);

            if (res == pids[idx])
            {
                uint64_t slice_end = get_time_ms() - scheduler_start;
                cpu_time_used_ms[idx] += (slice_end - slice_start);

                printf("%s, %llu, %llu\n",
                       p[idx].command,
                       (unsigned long long)slice_start,
                       (unsigned long long)slice_end);

                p[idx].completion_time = slice_end;
                p[idx].finished = WIFEXITED(status);
                p[idx].error = !p[idx].finished;
                finished[idx] = true;
                remaining--;

                p[idx].turnaround_time = p[idx].completion_time;
                if (p[idx].turnaround_time > cpu_time_used_ms[idx])
                {
                    p[idx].waiting_time = p[idx].turnaround_time - cpu_time_used_ms[idx];
                }
                else
                {
                    p[idx].waiting_time = 0;
                }
                p[idx].response_time = p[idx].start_time;

                fprintf(csv, "%s,%s,%s,%llu,%llu,%llu,%llu\n",
                        p[idx].command,
                        p[idx].finished ? "Yes" : "No",
                        p[idx].error ? "Yes" : "No",
                        (unsigned long long)p[idx].completion_time,
                        (unsigned long long)p[idx].turnaround_time,
                        (unsigned long long)p[idx].waiting_time,
                        (unsigned long long)p[idx].response_time);
                fflush(csv);

                break; 
            }

            uint64_t elapsed = get_time_ms() - run_start;
            if (elapsed >= (uint64_t)this_quantum)
            {
                kill(pids[idx], SIGSTOP);
                uint64_t slice_end = get_time_ms() - scheduler_start;
                cpu_time_used_ms[idx] += (slice_end - slice_start);

                printf("%s, %llu, %llu\n",
                       p[idx].command,
                       (unsigned long long)slice_start,
                       (unsigned long long)slice_end);

                if (this_quantum == quantum0)
                {
                    enque(q1, &f1, &r1, idx);
                }
                else
                {
                    enque(q2, &f2, &r2, idx);
                }

                break;
            }
        }

        uint64_t current_time = get_time_ms() - scheduler_start;
        while (boostTime > 0 && current_time >= next_boost_time)
        {
            int id;
            while (true)
            {
                id = deque(q1, &f1, &r1);
                if (id == -1)
                    break;

                enque(q0, &f0, &r0, id);
            }

            while (true)
            {
                id = deque(q2, &f2, &r2);
                if (id == -1)
                    break;

                enque(q0, &f0, &r0, id);
            }
            next_boost_time += boostTime;
        }
    }

    fclose(csv);
}