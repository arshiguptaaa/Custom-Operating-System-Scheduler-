# Custom-Operating-System-Scheduler-
A Linux-based CPU scheduling engine implementing offline and online schedulers using real process execution via POSIX system calls.
The project leverages fork(), execvp(), and signal-driven preemption (SIGSTOP, SIGCONT) to simulate kernel-like scheduling behavior entirely in user space.

🚀 Features
Offline Schedulers 

MTL458_Assignment_2

FCFS – Non-preemptive, sequential execution

Round Robin (RR) – Fixed quantum-based time slicing

MLFQ (3-level) – Priority-driven queues with dynamic demotion and periodic boosting

Online Schedulers 

MTL458_Assignment_2

Adaptive MLFQ

New tasks begin at medium priority

Priority updated using historical average burst times

Queue placement based on burst time vs. per-queue quantum

Predictive SJF

Default burst = 1s for first execution

Subsequent predictions = mean of last k valid bursts

Error-ending executions excluded from history

🛠 System Architecture

Process Control

fork() to spawn tasks

execvp() to run commands

Preemption and resume via SIGSTOP/SIGCONT

Context Switching Engine

Timestamp-based burst measurement

Non-blocking I/O to capture live process arrivals

Queue Structures

Circular queues for RR

Three-level MLFQ with configurable time slices and boost period

Metrics Tracking

Completion, turnaround, waiting, and response times

CSV export per scheduler type (e.g., result_offline_RR.csv)

📈 Output Specification 

MTL458_Assignment_2

After each context switch:

<Command>, <StartTime>, <EndTime>


For each finished task (logged in CSV):

Command

Finished (Yes/No)

Error (Yes/No)

Completion Time (ms)

Turnaround Time (ms)

Waiting Time (ms)

Response Time (ms)

📦 Project Structure
offline_schedulers.h      # FCFS, RR, MLFQ (offline)
online_schedulers.h       # Adaptive MLFQ & Online SJF
utils/                    # Timing, logging, queues
data/                     # Generated CSV logs
main.c                    # Scheduler entrypoint

▶️ How to Run
Compile
gcc main.c -o scheduler

Run Offline
./scheduler --mode offline --policy MLFQ input.txt

Run Online
./scheduler --mode online --policy SJF


Enter commands dynamically (non-blocking):

ls -l
sleep 1
echo Hello
