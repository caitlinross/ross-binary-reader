import sys
import struct
import copy

filename = sys.argv[1]

terminal_out = open("terminal-output.csv", "w")
router_out = open("router-output.csv", "w")

metadata_field_sz = 8
metadata_sz = metadata_field_sz * 3

radix = 8
lpid = 0
ts = 1
sample_sz = 2

with open(filename, "rb") as binary_file:
    binary_file.seek(0, 2)
    num_bytes = binary_file.tell()

    pos = 0
    while (pos < num_bytes):
        binary_file.seek(pos)
        metadata = struct.unpack("Qdq", binary_file.read(metadata_sz))
        print(metadata)
        pos += metadata_sz

        binary_file.seek(pos)
        struct_str = ""
        if (metadata[sample_sz] == 72): # terminal LP
            struct_str = "Qllddddll"
        else:
            struct_str = "Qdqdll" + str(radix) + "d" + str(radix) + "q"

        lp_data = struct.unpack(struct_str, binary_file.read(metadata[sample_sz]))
        pos += metadata[sample_sz]
        print(lp_data)
        
        if (metadata[sample_sz] == 72):
            terminal_out.write(','.join(str(p) for p in lp_data))
            terminal_out.write("\n")
        else:
            new_list = []
            new_list.append(lp_data[0]) # elements 1 and 2 are just pointer crap
            new_list.extend(lp_data[3:])
            print(new_list)
            router_out.write(','.join(str(p) for p in new_list))
            router_out.write("\n")
