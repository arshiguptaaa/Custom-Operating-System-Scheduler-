#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

// ------------------ CONSTANTS ------------------
#define MAX_HIST 50
#define READ_BUF 4096
#define MAX_PROCS 100
#define MAX_CMDS 50
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
    double est_burst;
    uint64_t arrival_time;

} Process;

int terminate_flag = 0;
char *cmd_history[MAX_CMDS] = {0};
double burst_hist[MAX_CMDS][MAX_HIST];
int total_cmds = 0;
double burst_sum[MAX_CMDS] = {0.0};
int burst_count[MAX_CMDS] = {0};

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

void handle_sigint(int sig)
{
    terminate_flag = 1;
}

void enque(int queue[], int *front, int *rear, int val)
{
    queue[(*rear)++] = val;
    if (*rear >= MAX_QUEUE)
    {
        *rear = 0;
    }
}

int deque(int queue[], int *front, int *rear)
{
    if (*front == *rear)
    {
        return -1;
    }
    int val = queue[(*front)++];
    if (*front >= MAX_QUEUE)
    {
        *front = 0;
    }
    return val;
}

int find_cmd_index(const char *cmd)
{
    for (int j = 0; j < total_cmds; j++)
    {
        if (strcmp(cmd_history[j], cmd) == 0)
        {
            return j;
        }
    }
    return -1;
}

void register_burst_global(int idx, double burst, bool error)
{
    if (idx < 0 || error)
    {
        return;
    }
    int pos = burst_count[idx] % MAX_HIST;
    burst_hist[idx][pos] = burst;
    burst_sum[idx] += burst;
    burst_count[idx]++;
}

double avg_burst(int idx)
{
    if (idx < 0)
    {
        return -1.0;
    }
    if (idx >= total_cmds || burst_count[idx] == 0)
    {
        return -1.0;
    }
    double avg = burst_sum[idx] / burst_count[idx];
    return avg;
}

double estimate_burst(int idx, int k)
{
    if (idx < 0)
    {
        return 1000.0;
    }
    if (idx >= total_cmds || burst_count[idx] == 0)
    {
        return 1000.0;
    }
    int total = burst_count[idx];
    int start = 0;
    if (total > k)
    {
        start = total - k;
    }
    double sum = 0.0;
    int used = 0;
    for (int i = start; i < total; i++)
    {
        sum += burst_hist[idx][i % MAX_HIST];
        used++;
    }
    if (used)
    {
        return sum / used;
    }
    else
    {
        return 1000.0;
    }
}

int read_new_arrivals(Process p[], int total_procs, uint64_t schedular_start)
{
    char *line = NULL;
    size_t linecap = 0;
    ssize_t nread;

    while ((nread = getline(&line, &linecap, stdin)) != -1)
    {
        if (nread > 0 && line[nread - 1] == '\n')
        {
            line[nread - 1] = '\0';
            nread--;
        }
        if (nread == 0)
        {
            continue;
        }

        p[total_procs].command = strdup(line);
        p[total_procs].process_id = -1;
        p[total_procs].waiting_time = 0;
        p[total_procs].response_time = 0;
        p[total_procs].finished = false;
        p[total_procs].error = false;
        p[total_procs].started = false;

        p[total_procs].arrival_time = get_time_ms() - schedular_start;

        int cmd_idx = find_cmd_index(line);

        if (cmd_idx == -1 && total_cmds < MAX_CMDS)
        {
            cmd_history[total_cmds] = strdup(line);
            cmd_idx = total_cmds;
            total_cmds++;
        }
        total_procs++;
    }

    free(line);

    return total_procs;
}

int read_all_commands(Process p[], int total_procs, uint64_t scheduler_start)
{
    FILE *fp = stdin;
    char line[READ_BUF];

    while (fgets(line, sizeof(line), fp))
    {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0')
        {
            continue;
        }
        if (total_procs >= MAX_PROCS)
        {
            break;
        }
        char *cmd = line;
        while (*cmd == ' ')
        {
            cmd++;
        }

        p[total_procs].command = strdup(cmd);
        p[total_procs].process_id = -1;
        p[total_procs].waiting_time = 0;
        p[total_procs].response_time = 0;
        p[total_procs].finished = false;
        p[total_procs].error = false;
        p[total_procs].started = false;

        p[total_procs].arrival_time = get_time_ms() - scheduler_start;

        int cmd_idx = find_cmd_index(cmd);
        if (cmd_idx == -1 && total_cmds < MAX_CMDS)
        {
            cmd_history[total_cmds] = strdup(cmd);
            cmd_idx = total_cmds;
            total_cmds++;
        }

        total_procs++;
    }

    return total_procs;
}

void enque_queue_level(Process p[], int idx,
                           int q0[], int *f0, int *r0,
                           int q1[], int *f1, int *r1,
                           int q2[], int *f2, int *r2,
                           int quantum0, int quantum1)
{
    char *cmd = p[idx].command;
    int cmd_idx = find_cmd_index(cmd);
    double avg = -1.0;
    if (cmd_idx != -1)
    {
        avg = avg_burst(cmd_idx);
    }

    if (avg >= 0.0)
    {
        if (avg < (double)quantum0)
        {
            enque(q0, f0, r0, idx);
        }
        else if (avg < (double)quantum1)
        {
            enque(q1, f1, r1, idx);
        }
        else
        {
            enque(q2, f2, r2, idx);
        }
    }
    else
    {
        enque(q1, f1, r1, idx);
    }
}

void MultiLevelFeedbackQueue(int quantum0, int quantum1, int quantum2, int boostTime)
{
    uint64_t scheduler_start = get_time_ms();
    FILE *csv = fopen("result_online_MLFQ_output.csv", "w");
    signal(SIGINT, handle_sigint);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    Process procs[MAX_PROCS];
    pid_t pids[MAX_PROCS] = {0};
    bool started[MAX_PROCS] = {0};
    bool finished[MAX_PROCS] = {0};
    uint64_t cpu_time_used_ms[MAX_PROCS] = {0};

    int q0[MAX_QUEUE], q1[MAX_QUEUE], q2[MAX_QUEUE];
    int f0 = 0, r0 = 0, f1 = 0, r1 = 0, f2 = 0, r2 = 0;

    int total_procs = 0;
    uint64_t next_boost_time = boostTime;
    bool interactive = isatty(STDIN_FILENO);

    if (interactive)
    {
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    }
    else
    {
        total_procs = read_all_commands(procs, total_procs, scheduler_start);
    }

    while (!terminate_flag)
    {
        if (interactive)
        {
            total_procs = read_new_arrivals(procs, total_procs, scheduler_start);
        }

        for (int i = 0; i < total_procs; i++)
        {
            if (!started[i] && !finished[i])
            {
                enque_queue_level(procs, i, q0, &f0, &r0, q1, &f1, &r1, q2, &f2, &r2, quantum0, quantum1);
                started[i] = true;
            }
        }

        if (f0 == r0 && f1 == r1 && f2 == r2)
        {
            usleep(1000);
            continue;
        }

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

        if (pids[idx] == 0)
        {
            procs[idx].start_time = slice_start;
            pid_t pid = fork();

            if (pid == 0)
            {
                fclose(csv);
                char cmd_copy[1000];
                strncpy(cmd_copy, procs[idx].command, sizeof(cmd_copy) - 1);
                cmd_copy[sizeof(cmd_copy) - 1] = '\0';
                char **argv = parse_command(cmd_copy);
                execvp(argv[0], argv);
                perror("execvp failed");
                _exit(EXIT_FAILURE);
            }
            else if (pid > 0)
            {
                pids[idx] = pid;
                procs[idx].process_id = pid;
                kill(pid, SIGSTOP);
            }
            else
            {
                perror("fork failed");
                finished[idx] = true;
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
                       procs[idx].command,
                       (unsigned long long)slice_start,
                       (unsigned long long)slice_end);
                fflush(stdout);

                procs[idx].completion_time = slice_end;
                procs[idx].finished = WIFEXITED(status);
                procs[idx].error = !procs[idx].finished || WEXITSTATUS(status) != 0;
                finished[idx] = true;

                procs[idx].turnaround_time = procs[idx].completion_time - procs[idx].arrival_time;

                if (procs[idx].turnaround_time > cpu_time_used_ms[idx])
                {
                    procs[idx].waiting_time = procs[idx].turnaround_time - cpu_time_used_ms[idx];
                }
                else
                {
                    procs[idx].waiting_time = 0;
                }
                if (procs[idx].start_time >= procs[idx].arrival_time)
                {
                    procs[idx].response_time = procs[idx].start_time - procs[idx].arrival_time;
                }
                else
                {
                    procs[idx].response_time = 0;
                }

                fprintf(csv, "%s,%s,%s,%llu,%llu,%llu,%llu\n",
                        procs[idx].command,
                        procs[idx].finished ? "Yes" : "No",
                        procs[idx].error ? "Yes" : "No",
                        (unsigned long long)procs[idx].completion_time,
                        (unsigned long long)procs[idx].turnaround_time,
                        (unsigned long long)procs[idx].waiting_time,
                        (unsigned long long)procs[idx].response_time);

                fflush(csv);

                if (!procs[idx].error)
                {
                    int cmd_idx = find_cmd_index(procs[idx].command);
                    if (cmd_idx != -1)
                    {
                        double burst = (double)(slice_end - slice_start);
                        register_burst_global(cmd_idx, burst, false);
                    }
                }
                break;
            }

            uint64_t elapsed = get_time_ms() - run_start;

            if (elapsed >= (uint64_t)this_quantum)
            {
                kill(pids[idx], SIGSTOP);
                uint64_t slice_end = get_time_ms() - scheduler_start;
                cpu_time_used_ms[idx] += (slice_end - slice_start);

                printf("%s, %llu, %llu\n",
                       procs[idx].command,
                       (unsigned long long)slice_start,
                       (unsigned long long)slice_end);
                fflush(stdout);

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
            while ((id = deque(q1, &f1, &r1)) != -1)
            {
                enque(q0, &f0, &r0, id);
            }
            while ((id = deque(q2, &f2, &r2)) != -1)
            {
                enque(q0, &f0, &r0, id);
            }
            next_boost_time += boostTime;
        }
    }

    fclose(csv);
}


int select_shortest_job(Process p[], bool finished[], int total_procs)
{
    double min_burst = 1e18;
    int temp = -1;
    for (int i = 0; i < total_procs; i++)
    {
        if (!finished[i] && p[i].est_burst < min_burst)
        {
            min_burst = p[i].est_burst;
            temp = i;
        }
    }
    return temp;
}

void ShortestJobFirst(int k)
{
    uint64_t scheduler_start = get_time_ms();
    FILE *csv = fopen("result_online_SJF_output.csv", "w");
    signal(SIGINT, handle_sigint);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    Process procs[MAX_PROCS];
    bool finished[MAX_PROCS] = {0};
    pid_t pids[MAX_PROCS] = {0};
    uint64_t cpu_time_used_ms[MAX_PROCS] = {0};
    int total_procs = 0;

    bool interactive = isatty(STDIN_FILENO);
    if (interactive)
    {
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    }
    else
    {
        total_procs = read_all_commands(procs, total_procs, scheduler_start);
    }

    while (!terminate_flag)
    {
        if (interactive)
        {
            total_procs = read_new_arrivals(procs, total_procs, scheduler_start);
        }
        for (int i = 0; i < total_procs; i++)
        {
            int idx = find_cmd_index(procs[i].command);
            if (idx == -1 && total_cmds < MAX_CMDS)
            {
                cmd_history[total_cmds] = strdup(procs[i].command);
                idx = total_cmds;
                total_cmds++;
            }

            procs[i].est_burst = estimate_burst(idx, k);
        }
        int idx = select_shortest_job(procs, finished, total_procs);

        if (idx == -1)
        {
            continue;
        }

        uint64_t start = get_time_ms() - scheduler_start;
        procs[idx].start_time = start;
        procs[idx].started = true;

        pid_t pid = fork();
        if (pid == 0)
        {
            fclose(csv);
            char cmd_copy[1000];
            strncpy(cmd_copy, procs[idx].command, sizeof(cmd_copy) - 1);
            cmd_copy[sizeof(cmd_copy) - 1] = '\0';
            char **argv = parse_command(cmd_copy);
            execvp(argv[0], argv);
            perror("execvp failed");
            _exit(EXIT_FAILURE);
        }
        else if (pid > 0)
        {
            pids[idx] = pid;
            int status;
            waitpid(pid, &status, 0);

            uint64_t end = get_time_ms() - scheduler_start;
            procs[idx].completion_time = end;
            procs[idx].finished = WIFEXITED(status);
            procs[idx].error = !procs[idx].finished || WEXITSTATUS(status) != 0;
            finished[idx] = true;

            uint64_t burst_time = procs[idx].completion_time - procs[idx].start_time;
            procs[idx].turnaround_time = procs[idx].completion_time - procs[idx].arrival_time;
            procs[idx].response_time = procs[idx].start_time - procs[idx].arrival_time;

            if (procs[idx].turnaround_time > burst_time)
            {
                procs[idx].waiting_time = procs[idx].turnaround_time - burst_time;
            }
            else
            {
                procs[idx].waiting_time = 0;
            }

            cpu_time_used_ms[idx] = burst_time;

            printf("%s, %llu, %llu\n",
                   procs[idx].command,
                   (unsigned long long)procs[idx].start_time,
                   (unsigned long long)procs[idx].completion_time);

            fprintf(csv, "%s,%s,%s,%llu,%llu,%llu,%llu\n",
                    procs[idx].command,
                    procs[idx].finished ? "Yes" : "No",
                    procs[idx].error ? "Yes" : "No",
                    (unsigned long long)procs[idx].completion_time,
                    (unsigned long long)procs[idx].turnaround_time,
                    (unsigned long long)procs[idx].waiting_time,
                    (unsigned long long)procs[idx].response_time);
            fflush(csv);

            int cmd_idx = find_cmd_index(procs[idx].command);
            if (cmd_idx != -1)
            {
                register_burst_global(cmd_idx, (double)burst_time, procs[idx].error);
            }
        }
        else
        {
            perror("fork failed");
            continue;
        }
    }
    fclose(csv);
    printf("\nScheduler terminated by Ctrl+C.\n");
}
