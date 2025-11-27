# Custom OS-Level CPU Scheduler

A Linux-based CPU scheduling engine implementing both **offline** and **online** schedulers using real process execution via POSIX system calls.  
This scheduler uses `fork()`, `execvp()`, and signal-based preemption (`SIGSTOP`, `SIGCONT`) to emulate OS-level context switching entirely in user space.

---

##  Features

### **Offline Scheduling Algorithms**
- **First-Come First-Serve (FCFS)**  
  Non-preemptive, sequential execution of tasks.
- **Round Robin (RR)**  
  Quantum-based preemptive scheduling using circular queues.
- **Multi-Level Feedback Queue (MLFQ)**  
  Three-level queue structure with dynamic demotion and periodic priority boosting.

### **Online Scheduling Algorithms**
- **Adaptive MLFQ**
  - New tasks start at Medium priority.
  - Priority recalculated using historical average burst time.
  - Queue selected based on burst estimate vs. queue quantum.
- **Predictive Shortest Job First (SJF)**
  - Default burst = 1s for first run.
  - Subsequent burst predictions = average of last *k* valid bursts.
  - Error-ending bursts excluded from history.

---

##  System Architecture Overview

### **Process Management**
- `fork()` used to spawn new child processes.
- `execvp()` executes the actual command.
- `SIGSTOP` / `SIGCONT` simulate preemption and resumption.

### **Context Switching Engine**
- Uses timestamps to measure accurate process burst times.
- Non-blocking STDIN polling to accept real-time tasks.
- Scheduler loop handles preemption, queue updates, and logging.

### **Queue & Scheduling Structures**
- Circular queue for **Round Robin**.
- Three-level **MLFQ** with configurable time slices.
- Automatic **priority boosting** after a fixed interval.

### **Performance Metrics**
Tracked for every process:
- Completion Time  
- Turnaround Time  
- Waiting Time  
- Response Time  

All metrics are exported to CSV files (e.g., `result_offline_RR.csv`).

---

## Output Format

### **After every context switch:**
<Command>, <StartTime>, <EndTime>

### **For each completed process (CSV rows):**
- Command  
- Finished (Yes/No)  
- Error (Yes/No)  
- Completion Time (ms)  
- Turnaround Time (ms)  
- Waiting Time (ms)  
- Response Time (ms)

---

## Project Structure
offline_schedulers.h # FCFS, RR, MLFQ (offline)
online_schedulers.h # Adaptive MLFQ & Online SJF
utils/ # Timing, logging, data structures
data/ # Auto-generated CSV outputs
main.c # Scheduler entrypoint

---

##  Running the Scheduler

### **Compile in bash**
gcc main.c -o scheduler

Run Offline Scheduler
./scheduler --mode offline --policy MLFQ input.txt

Run Online Scheduler
./scheduler --mode online --policy SJF




