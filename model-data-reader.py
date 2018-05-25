import sys
import struct
import argparse

ap = argparse.ArgumentParser()
ap.add_argument("-f", "--filename", required=True, help="binary file to convert")
ap.add_argument("-m", "--model", required=True, help="model type (dragonfly, slimfly, and fattree currently supported)")
args = vars(ap.parse_args())

filename = args["filename"]
stem = filename.split(".")[0]

DRAGONFLY = 1
SLIMFLY = 2
FATTREE = 3
PHOLD = 4
network_type = 0
if args["model"] == "dragonfly":
    network_type = DRAGONFLY
elif args["model"] == "slimfly":
    network_type = SLIMFLY
elif args["model"] == "fattree":
    network_type = FATTREE
elif args["model"] == "phold":
    network_type == PHOLD
else:
    sys.exit("INVALID TYPE for --model")

#terminal_out = open(stem + "-terminal-output.csv", "w")
#router_out = open(stem + "-router-output.csv", "w")
phold_out = open(stem + "-output.csv", "w")

#radix = int(args["radix"])

# write out headers
phold_out.write("LP,KP,PE,GVT,LPVT,RT,TYPE,test_int\n")
#if network_type == DRAGONFLY:
#    terminal_out.write("LP,KP,PE,terminal_id,fin_chunks,data_size,fin_hops,fin_chunks_time,busy_time,end_time,fwd_events,rev_events\n")
#    router_out.write("LP,KP,PE,router_id,end_time,fwd_events,rev_events")
#    for i in range(radix):
#        router_out.write(",busy_time_" + str(i))
#    for i in range(radix):
#        router_out.write(",link_traffic_" + str(i))
#    router_out.write("\n")
#elif network_type == SLIMFLY or network_type == FATTREE:
#    terminal_out.write("LP,KP,PE,terminal_id,end_time,vc_occupancy_sum\n")
#    router_out.write("LP,KP,PE,router_id,end_time")
#    for i in range(radix):
#        router_out.write(",vc_occupancy_sum_" + str(i))
#    router_out.write("\n")


metadata_sz = 24

flag = 0
sample_sz = 1
ts = 2
real_time = 3

peid = 0
kpid = 1
lpid = 2
gvt = 3
st_type = 4
model_sz = 5

with open(filename, "rb") as binary_file:
    binary_file.seek(0, 2)
    num_bytes = binary_file.tell()
    print("num_byes == " + str(num_bytes))

    pos = 0
    while (pos < num_bytes):
        binary_file.seek(pos)
        metadata = struct.unpack("@iidd", binary_file.read(metadata_sz))
        #print(metadata)
        pos += metadata_sz
        binary_file.seek(pos)
        struct_str = ""

        model_md = struct.unpack("@3IfiI", binary_file.read(metadata[sample_sz]))
        #print(model_md)
        pos += metadata[sample_sz]
        if metadata[flag] == 3: #  model data
            struct_str = "@i"
            #if network_type == DRAGONFLY:
            #    if (metadata[sample_sz] == 72): # terminal LP
            #        struct_str = "@Qllddddll"
            #    else:
            #        struct_str = "@Qdqdll" + str(radix) + "d" + str(radix) + "q"
            #elif network_type == SLIMFLY or network_type == FATTREE:
            #    if metadata[sample_sz] == 24: #accounting for end padding
            #        struct_str = "@Qdii"
            #    else:
            #        struct_str = "@Qqd" + str(radix) + "i"
        else:
            sys.exit("ERROR")

        #print(struct_str)
        #print(metadata[sample_sz])
        data = struct.unpack(struct_str, binary_file.read(model_md[model_sz]))
        pos += model_md[model_sz]
        #print(data)

        phold_out.write(str(model_md[lpid]) + "," + str(model_md[kpid]) + "," + str(model_md[lpid]) + "," + str(model_md[gvt]) + ",")
        phold_out.write(','.join(str(p) for p in metadata[ts:]))
        phold_out.write("," + str(model_md[st_type]))
        phold_out.write("," + str(data[0]) + "\n")
        #if network_type == DRAGONFLY:
        #    if (metadata[sample_sz] == 72):
        #        terminal_out.write(','.join(str(p) for p in metadata[lpid:peid+1]) + ",")
        #        terminal_out.write(','.join(str(p) for p in data))
        #        terminal_out.write("\n")
        #    else:
        #        new_list = []
        #        new_list.append(data[0]) # elements 1 and 2 are just mem addresses
        #        new_list.extend(data[3:])
        #        #print(new_list)
        #        router_out.write(','.join(str(p) for p in metadata[lpid:peid+1]) + ",")
        #        router_out.write(','.join(str(p) for p in new_list))
        #        router_out.write("\n")
        #elif network_type == SLIMFLY or network_type == FATTREE:
        #    if metadata[sample_sz] == 24:
        #        terminal_out.write(','.join(str(p) for p in metadata[lpid:peid+1]) + ",")
        #        terminal_out.write(','.join(str(p) for p in data[0:-1]))
        #        terminal_out.write("\n")
        #    else:
        #        new_list = []
        #        new_list.append(data[0]) # element 2 is just mem address
        #        new_list.extend(data[2:])
        #        #print(new_list)
        #        router_out.write(','.join(str(p) for p in metadata[lpid:peid+1]) + ",")
        #        router_out.write(','.join(str(p) for p in new_list))
        #        router_out.write("\n")

    print("position == " + str(pos))
