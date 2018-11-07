import sys
import struct
import argparse

ap = argparse.ArgumentParser()
ap.add_argument("-f", "--filename", required=True, help="binary file to convert")
ap.add_argument("-r", "--radix", required=True, help="network radix")
ap.add_argument("-n", "--network", required=True, help="network type (dragonfly, slimfly, and fattree currently supported)")
args = vars(ap.parse_args())

filename = args["filename"]
stem = filename.split(".")[0]

DRAGONFLY = 1
network_type = 0
if args["network"] == "dragonfly":
    network_type = DRAGONFLY
else:
    sys.exit("INVALID TYPE for --network")

terminal_out = open(stem + "-terminal-output.csv", "w")
router_out = open(stem + "-router-output.csv", "w")
mpi_out = open(stem + "-mpi-layer-output.csv", "w")

radix = int(args["radix"])

if network_type == DRAGONFLY:
    terminal_out.write("LP,KP,PE,terminal_id,virtual_time,fin_chunks,data_size,fin_hops,fin_chunks_time,busy_time\n")
    mpi_out.write("LP,KP,PE,nw_id,virtual_time,app_id,local_rank,num_sends,num_recvs,num_bytes_sent,num_bytes_recvd,send_time,recv_time,wait_time,compute_time,comm_time,max_time,avg_msg_time\n")
    router_out.write("LP,KP,PE,router_id,virtual_time")
    for i in range(radix):
        router_out.write(",busy_time_" + str(i))
    for i in range(radix):
        router_out.write(",link_traffic_" + str(i))
    router_out.write("\n")


metadata_sz = 48

lpid = 0
kpid = 1
peid = 2
ts = 3
real_time = 4
sample_sz = 5
flag = 6
with open(filename, "rb") as binary_file:
    binary_file.seek(0, 2)
    num_bytes = binary_file.tell()
    print("num_byes == " + str(num_bytes))

    pos = 0
    while (pos < num_bytes):
        binary_file.seek(pos)
        metadata = struct.unpack("@QLLddii", binary_file.read(metadata_sz))
        #print(metadata)
        pos += metadata_sz
        binary_file.seek(pos)
        struct_str = ""

        if metadata[flag] == 0: # PE data
            struct_str = "@d11Q9dQ"
        elif metadata[flag] == 1: # KP data
            struct_str = "@ddQQQQQQ"
        elif metadata[flag] == 2: # LP data
            struct_str = "@QQQQ"
        elif metadata[flag] == 3: #  model data
            if network_type == DRAGONFLY:
                if (metadata[sample_sz] == 72): # terminal LP
                    struct_str = "@Qllddddll"
                elif metadata[sample_sz] == 104: # nw LP
                    struct_str = "@Qii2L2Q7d"
                else:
                    struct_str = "@Qdqdll" + str(radix) + "d" + str(radix) + "q"

        #print(struct_str)
        #print(metadata[sample_sz])
        data = struct.unpack(struct_str, binary_file.read(metadata[sample_sz]))
        pos += metadata[sample_sz]
        #print(data)

        if metadata[flag] == 0: # PE data
            pass
        elif metadata[flag] == 1: # KP data
            pass
        elif metadata[flag] == 2: # LP data
            pass
        elif metadata[flag] == 3: #  model data
            if network_type == DRAGONFLY:
                if (metadata[sample_sz] == 72):
                    terminal_out.write(','.join(str(p) for p in metadata[lpid:peid+1]) + ",")
                    terminal_out.write(str(data[0]) + ",")
                    terminal_out.write(str(metadata[ts]) + ",")
                    terminal_out.write(','.join(str(p) for p in data[1:-3]))
                    terminal_out.write("\n")
                elif metadata[sample_sz] == 104:
                    mpi_out.write(','.join(str(p) for p in metadata[lpid:peid+1]) + ",")
                    mpi_out.write(str(data[0]) + ",")
                    mpi_out.write(str(metadata[ts]) + ",")
                    mpi_out.write(','.join(str(p) for p in data[1:]))
                    mpi_out.write("\n")
                else:
                    #new_list = []
                    #new_list.append(data[0]) # elements 1 and 2 are just mem addresses
                    #new_list.extend(data[3:])
                    #print(new_list)
                    router_out.write(','.join(str(p) for p in metadata[lpid:peid+1]) + ",")
                    router_out.write(str(data[0]) + ",")
                    router_out.write(str(metadata[ts]) + ",")
                    router_out.write(','.join(str(p) for p in data[6:]))
                    router_out.write("\n")

    print("position == " + str(pos))
