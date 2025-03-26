# Linux Process Manager

## Overview
The **Linux Process Manager** is a lightweight process management system that allows users to monitor, manage, and control running processes on a Linux system. It provides essential functionalities such as process creation, termination, and status monitoring, helping users handle system resources efficiently.

## Features
- List all active processes
- Start and terminate processes
- Monitor CPU and memory usage of processes
- Handle inter-process communication (IPC)

## Technologies Used
- **Programming Language:** C/C++ or Python (Specify your language)
- **Operating System:** Linux/Unix
- **Concurrency & IPC:** POSIX Threads, Message Queues, or Shared Memory

## Installation
```bash
# Clone the repository
git clone git@github.com:your-username/linux_process_manager.git
cd linux_process_manager

# Build the project (if applicable)
make
```

## Usage
```bash
# Run the process manager
./process_manager
```

## Example Commands
```bash
cs205$ run prog x40 40
cs205$ run prog x50 50
cs205$ run prog x60 60
cs205$ run prog x70 70
cs205$ run prog x80 80
cs205$ list
1697479,0
1697480,0
1697483,0
1697486,1
1697489,1
cs205$ stop 1697479
stopping 1697479
```

## Contributing
Feel free to submit issues or pull requests if you'd like to contribute to the project.

## License
This project is licensed under the MIT License.

