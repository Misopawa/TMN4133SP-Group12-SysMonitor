# TMN4133-Group12-SysMonitor-
Develop a command-line system monitoring tool using Linux system calls and /proc filesystem.

# SysMonitor++

A lightweight, robust Linux system resource monitoring tool written in C. It provides real-time information about CPU usage, memory usage, and top active processes using direct system calls and the `/proc` filesystem.

## Features

- **CPU Usage Monitoring**: Calculates real-time CPU utilization percentage.
- **Memory Usage**: Displays Total, Used, and Free memory statistics.
- **Top 5 Processes**: Lists the top 5 CPU-consuming processes.
- **Continuous Monitoring**: Live dashboard mode with auto-refresh.
- **Logging**: Automatically logs events and errors to `syslog.txt` with timestamps.
- **Robustness**:
    - Low-level system calls (`open`, `read`, `write`, `close`).
    - Async-signal-safe signal handling (Graceful exit on `Ctrl+C`).
    - Safe user input handling (no buffer overflows or infinite loops).
    - Error handling for missing files or permissions.

## Prerequisites

- **Operating System**: Linux (or WSL on Windows, macOS with limited functionality).
- **Compiler**: GCC (GNU Compiler Collection).

## Compilation

Open your terminal in the project directory and run:

```bash
gcc sysmonitor.c -o sysmonitor
```

This will create an executable file named `sysmonitor`.

## Usage

### 1. Interactive Menu Mode
Run the program without arguments to enter the interactive menu:

```bash
./sysmonitor
```

**Menu Options:**
1.  **CPU Usage**: Shows current CPU load (1-second sample).
2.  **Memory Usage**: Shows RAM statistics from `/proc/meminfo`.
3.  **Top 5 Processes (CPU)**: Lists processes consuming the most CPU time.
4.  **Continuous Monitoring**: Switch to live update mode.
5.  **Exit**: Terminate the program.

### 2. Continuous Monitoring Mode (Command Line)
You can start directly in continuous mode by specifying a refresh interval (in seconds):

```bash
./sysmonitor -c <interval>
```

**Example:**
To refresh every 2 seconds:
```bash
./sysmonitor -c 2
```

*Press `Ctrl+C` to stop continuous monitoring.*

## Logging
The tool automatically creates/appends to `syslog.txt` in the same directory.
Check the logs to see start/stop times and recorded metrics:

```bash
cat syslog.txt
```

## Running on Windows
This tool relies on the Linux `/proc` filesystem. To run it on Windows:
1.  Install **WSL** (Windows Subsystem for Linux).
2.  Open your WSL terminal (e.g., Ubuntu).
3.  Navigate to the folder and compile as shown above.

## Flowchart
A visual representation of the program's logic is available in [flowchart.svg](./flowchart.svg).
