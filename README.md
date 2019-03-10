## Reader for ROSS Instrumentation binary files

Simple reader that just takes the binary output from
ROSS and converts it to a CSV.

### Running the reader

For either reader, `--help` can be used to see the available commands.

##### For Event Traces

Use `event-reader.py` to read files from event tracing.
The only option here for this reader is `--filename`.

The reader assumes that either there is no model data, or that
the only model data saved is a signed int for the value of event type.

##### All other Instrumentation modes

Use `instrumentation-reader.py` to read simulation engine and/or model data
from any of the 3 time sampling modes.

Some support is built in for reading from the CODES dragonfly, slimfly, and fat-tree models.
If model data is collected for these models use the `--network` and `--radix` options.
