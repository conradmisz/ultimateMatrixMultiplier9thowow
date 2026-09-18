# FPGA Architecture Design Challenge

## Target Hardware 
Titanium Ti180 J484 Development Kit 
Refer to Documentation folder for relevant docs 

## Goal 
Perorm dot matrix multiplication where A and B are randomized vectors 

## Control  
Use an open source RISC-V IP Core for control 

## Architecture 
The below architecture is an example of something that is possible 
Soft core RISC-V IP Core for control with instruection memory. Scratchpad memory to hold vectors. Vector compute module that then stores outputs in a circular buffer / scratchpad memory. Bonus points for outputting the 

## Open Questions
1. Can a RISC-V Core fit on this FPGA?
2. Is there an easier way of doing this? 
   1. Probably, full RISC-V Core may be excessive? Investigate other options
3. Lets investigate vector compute architectures that are lightweight
4. What vector size to use? Doesn't need to be too complicated
5. How dodes the soft core communicate with scratchpad / vector compute? Bare wire? AXI-Lite?
6. How to program this FPGA?
7. How to simulate this?


## Language
Use SystemVerilog

## Testbench 
Test each individual component by creating a test bench for each module, as well as create a high level test bench for each module 


