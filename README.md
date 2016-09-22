## Reader for ROSS data collection binary files

So far this is a pretty simple reader that just takes the binary output from 
ROSS and converts it to a CSV.

### Build

Just type 'make' when you're in the reader directory. 
If you're running on Linux on an x86 system, no flags needed.
If you're running this on a BG/Q, you need to add the -DBGQ flag to the Makefile.
If you're running this on a Mac, add -DMACOS, because Macs do not have the argp header needed for 
the runtime argument library used.

NOTE: Right now you can't read data created on a system with a different endianness
from the system you are running the reader on (i.e., don't read BG/Q data while
running the reader on an x86 system).


### Running the reader

You can type ./ross-reader --help to see an explanation for the available 
options, which are also explained here. 

`-f` or `--filename` is for inputting the filename of the binary file to be read.

`-t` or `--filetype` is for the type of file

File types:
0 GVT-based instrumentation
1 Real time sampling
2 event tracing

The above arguments are required, the following are optional:

`-c` or `--combine-events` is for use with the event trace data.  It will bin the event trace by
source, destination LP pairs  over an interval of virtual time.  It also gives the count of events
in that bin. This is useful if your event trace is really large and you'd like to have a smaller file
(at the cost of coarser grained data of course).

`-b` or `--bin-size` is to change the virtual time bin size.  Default is 10000.

### Output
Everything is output as a text file delimited by commas. The event tracing always outputs one file.
If the ROSS instrumentation was selected to run with output only at a
PE level, then only one file is output for the GVT-based data.  Two are output for the real time, because
the difference in virtual time and GVT metric is always reported at the KP level.  So in this case, there will be a file
for the PE metrics and one for the KP metrics.  If the ROSS instrumentation was set to do the KP/LP level metrics,
then both the GVT-based and real time output has one file each for PE, KP, and LP level metrics.  

The text files are saved to the same directory as the binary files.

### Other notes
The reader needs the README file that is output by the instrumentation, because it contains data about the simulation the
reader needs to correctly read the file.
The reader expects that this file has the same naming convention as the data file (which it will, unless you change
the file name manually).  
