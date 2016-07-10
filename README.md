## Reader for ROSS data collection binary files

So far this is a pretty simple reader that just takes the binary output from 
ROSS and converts it to a CSV.

### Build

Just type 'make' when you're in the reader directory.  As of right now, the only 
change that should need to be made to the Makefile is the -DBGQ option.
If the data you want to read was from a simulation ran on a BG/Q, leave this 
flag in.  Otherwise if the data is from a sim ran on an x86 system, remove -DBGQ. 

NOTE: Right now you can't read data created on a system with a different endianness
from the system you are running the reader on (i.e., don't read BG/Q data while
running the reader on an x86 system)


### Running the reader

You can type ./ross-reader --help to see an explanation for the available 
options, which are also explained here. 

-f or --filename is for inputting the filename of the binary file to be read.

-t --filetype is for the type of file, i.e. is the binary from the ROSS GVT data
collection (filetype=0) or real time sampling data collection (filetype=1).

