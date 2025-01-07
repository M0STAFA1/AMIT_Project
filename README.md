# Process Information Viewer

A simple GTK-based graphical user interface (GUI) application for monitoring system processes on Linux. It provides detailed information about running processes such as PID, user, command, memory usage, and CPU time, all displayed in an interactive tree view. The application also allows users to refresh data, expand/collapse the process tree, and kill processes.

## Features

- **Tree View**: Displays processes in a hierarchical structure with parent-child relationships.
- **Process Details**: View information about each process, including:
  - Process ID (PID)
  - User
  - Command
  - Memory Usage (VmRSS)
  - CPU Time (user + system time)
- **Expand/Collapse**: Expand or collapse all tree rows for better navigation.
- **Kill Process**: Select a process and kill it using the "SIGKILL" signal.
- **Segmentation Fault Handling**: Custom signal handler that prints a stack trace upon a segmentation fault.
- **Dark Mode**: Supports GTK dark theme for a modern and consistent user experience.

## Prerequisites

Before compiling the application, make sure your system has the following dependencies installed:

- **GTK 3**: The application uses GTK for the GUI.
  - On Ubuntu/Debian: `sudo apt-get install libgtk-3-dev`
- **C Compiler**: A C compiler like `gcc` is required to compile the source code.

## Installation

### 1. Clone the Repository

```
git clone https://github.com/M0STAFA1/AMIT_Project.git
cd AMIT_Project
```

### 2. Install Dependencies
On Ubuntu/Debian-based systems, you can install the necessary GTK development libraries:

```
sudo apt-get install libgtk-3-dev
```
### 3. Compile the Application
To compile the application, run the following command in the project directory:

```
make clean
make
sudo insmod proc_info.ko
gcc -o proc_info_reader proc_info_reader.c
gcc process_info_gui.c -o process_info_gui `pkg-config --cflags --libs gtk+-3.0`

```

```
./proc_info_reader
./process_info_gui
```

### Usage
Upon running the application, you'll see the main window with the following components:

* Process Tree View: Displays system processes in a tree structure. Processes are listed with information like PID, user, memory, and CPU time.
* Buttons:
  * Refresh: Reloads the process information.
  * Collapse All: Collapses all the rows in the tree view.
  * Expand All: Expands all the rows in the tree view.
  * Kill Process: Select a process and click this button to send a SIGKILL signal to terminate it.

### Development
This application is written in C and uses GTK 3 for the graphical interface. The code is structured as follows:

* process_info.c: The main source code containing logic for fetching process information and updating the GUI.
* Makefile: The build system for compiling the application.
Adding New Features
Feel free to fork the repository and submit pull requests. Some ideas for new features or improvements include:

Displaying additional process details, such as command-line arguments.
* Displaying CPU and memory usage in graphs.
* Allowing the user to filter processes by name, user, or other criteria.

### License
This project is licensed under the MIT License - see the LICENSE file for details.

