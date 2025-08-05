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

    ./Simulator programVerySimple 6 program1 3 program2 0                                                    
This will:                                                                                                  
- Load programVerySimple.txt with arrival time 6                                                            
- Load program1.txt with arrival time 3                                                                     
- Load program2.txt with arrival time 0                                                                    

Or, if arrival times are omitted:
    ./Simulator programA programB programC
- programA.txt arrives at time 0
- programB.txt at time 1
- programC.txt at time 2
    
## Supported Instructions
- ADD x y:    Adds two integers and stores them into the Accumulator register
- SHIFT x:    Shifts the bits of the values stored in the Accumulator register depending on the value (+-)
- DIV x y:    Divides two integers and stores them into the Accumulator register
- TRAP x y :  Raises Interrupt with that code (number)
- NOP:        No operation
- JUMP x :    Jumps [x] lines (PC value + x)
- ZJUMP x:    Jumps [x] lines if the value in the Accumulator register equals Zero
- WRITE x y:  Writes value of the register specified by [y] in the memory address [x]. Writes the value of the Accumulator Register by default.
- READ x y:   Same functionality as WRITE, but reading values. [y] specifies in which register the data will be copied, [x] specifies the address.
- INC x y:    Increases the value stored in the [y] register by [x].
- HALT:       Stops the execution
- OS:         Raises Operating System Interrupt
- IRET:       Returns from executing an Interrupt
- MOV x y:    Copies the content of the [x] register into the [y] register
- RET:        Return from a subroutine
- CALL:       Jump to a subroutine
- MEMADD x y: Adds [x] and the contents of the address [y] into the Accumulator Register  

# Features
- Instruction parsing
- Basic scheduling
- Trap/Interrupt handling
- Arrival time and priority management

# Notes
This project is for educational purposes and not intended for production use.
Works best when run in a terminal or console.
Ensure all .txt program files are correctly formatted to avoid runtime errors.













