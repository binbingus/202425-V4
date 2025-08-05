# OS Simulator

A simple **Operating System Simulator** developed for a university project (Escuela de Infeniería Informática, UniOvi)
It supports the loading and execution of basic user-defined programs written in a custom text-based format.

## What It Does
This simulator mimics the behavior of a very basic operating system. It handles:

- **Program loading**
- **Basic scheduling with priorities and arrival times**
- **Execution of simple instructions (e.g., `ADD`, `JUMP`, `TRAP`, etc.)**
- **Interrupt and trap management**

Programs are written in `.txt` files and interpreted line-by-line by the simulator.

## File Structure

- All files (including the simulator executable and `.txt` programs) must be placed in the same directory.
- The simulator reads the programs passed via the command line and loads them into the system for execution.

## Program Format
Each program file follows this structure:

- Line 1:  <program size>     # Number of instructions
- Line 2:  <priority>         # Integer priority value
- Line 3+: <instruction list> # One instruction per line

### Example (`program1.txt`)
    5
    2
    ADD 3
    JUMP 1
    TRAP
    NOP
    END

## How to Use
Run the simulator from the command line, specifying the program names and optionally their arrival times.

### Syntax
./Simulator <programName1> [arrivalTime1] <programName2> [arrivalTime2] ...

- <programName> refers to the .txt file with the program (do not include the .txt extension).
- [arrivalTime] is optional. If omitted, programs are loaded in the order they appear, with increasing arrival times.

./Simulator programVerySimple 6 program1 3 program2 0                                                    Or, if arrival times are omitted:
This will:                                                                                                  ./Simulator programA programB programC
- Load programVerySimple.txt with arrival time 6                                                            - programA.txt arrives at time 0
- Load program1.txt with arrival time 3                                                                     - programB.txt at time 1
- Load program2.txt with arrival time 0                                                                     - programC.txt at time 2

## Supported Instructions


# Notes
This project is for educational purposes and not intended for production use.
Works best when run in a terminal or console.
Ensure all .txt program files are correctly formatted to avoid runtime errors.













