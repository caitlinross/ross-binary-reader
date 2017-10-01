import sys
import struct
import copy

filename = sys.argv[1]

terminal_out = open("terminal-output.csv", "w")
router_out = open("router-output.csv", "w")
kp_out = open("kp-output.csv", "w")
lp_out = open("lp-output.csv", "w")

# write out headers
kp_out.write("KP,PE,VT,RT,time_ahead_gvt,rb_total,rb_primary,rb_secondary\n")
lp_out.write("LP,KP,PE,VT,RT,fwd_ev,rev_ev,network_sends,network_recvs\n")
router_out.write("router_id,end_time,fwd_events,rev_events,busy_time,link_traffic\n")
terminal_out.write("terminal_id,fin_chunks,data_size,fin_hops,fin_chunks,busy_time,end_time,fwd_events,rev_events\n")

metadata_sz = 48 

radix = 42
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

    pos = 0
    while (pos < num_bytes):
        binary_file.seek(pos)
        metadata = struct.unpack("@QLLddii", binary_file.read(metadata_sz))
        #print(metadata)
        pos += metadata_sz
        binary_file.seek(pos)
        struct_str = ""

        if metadata[flag] == 1: # KP data
            struct_str = "@dQQ"
        elif metadata[flag] == 2: # LP data
            struct_str = "@QQQQ"
        elif metadata[flag] == 3: #  model data
            if (metadata[sample_sz] == 72): # terminal LP
                struct_str = "@Qllddddll"
            else:
                struct_str = "@Qdqdll" + str(radix) + "d" + str(radix) + "q"

        data = struct.unpack(struct_str, binary_file.read(metadata[sample_sz]))
        pos += metadata[sample_sz]
        #print(data)
            
        if metadata[flag] == 1: # KP data
            kp_data = []
            kp_data.extend(metadata[kpid:real_time+1])
            kp_data.extend(data[0:2])
            kp_data.append(data[1] - data[2])
            kp_data.append(data[2])
            kp_out.write(','.join(str(p) for p in kp_data))
            kp_out.write("\n")
        elif metadata[flag] == 2: # LP data
            lp_data = []
            lp_data.extend(metadata[lpid:real_time+1])
            lp_data.extend(data)
            lp_out.write(','.join(str(p) for p in lp_data))
            lp_out.write("\n")
        elif metadata[flag] == 3: #  model data
            if (metadata[sample_sz] == 72):
                terminal_out.write(','.join(str(p) for p in data))
                terminal_out.write("\n")
            else:
                new_list = []
                new_list.append(data[0]) # elements 1 and 2 are just mem addresses
                new_list.extend(data[3:])
                #print(new_list)
                router_out.write(','.join(str(p) for p in new_list))
                router_out.write("\n")
