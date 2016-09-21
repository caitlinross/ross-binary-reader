## Reader for ROSS data collection binary files

So far this is a pretty simple reader that just takes the binary output from 
ROSS and converts it to a CSV.

### Build

Just type 'make' when you're in the reader directory. 
If you're running on Linux on an x86 system, no flags needed.
If you're running this on a BG/Q, you need to add the -DBGQ flag to the Makefile.
If you're running this on a Mac, add -DMACOS. 

NOTE: Right now you can't read data created on a system with a different endianness
from the system you are running the reader on (i.e., don't read BG/Q data while
running the reader on an x86 system).


### Running the reader

You can type ./ross-reader --help to see an explanation for the available 
options, which are also explained here. 

`-f` or `--filename` is for inputting the filename of the binary file to be read.
*Note*: right now there is an error in dealing with the path name if you don't run the reader inside the directory
that contains the binary file.  I will be fixing this soon.  

`-t` or `--filetype` is for the type of file

File types:
0 GVT-based instrumentation
1 Real time sampling
2 event tracing

`-c` or `--combine-events` is for use with the event trace data.  It will bin the event trace by
source, destination LP pairs  over an interval of virtual time.  It also gives the count of events
in that bin. This is useful if your event trace is really large and you'd like to have a smaller file
(at the cost of coarser grained data of course).

`-b` or `--bin-size` is to change the virtual time bin size.  Default is 10000.


