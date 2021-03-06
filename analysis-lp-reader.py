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
SLIMFLY = 2
FATTREE = 3
network_type = 0
if args["network"] == "dragonfly":
    network_type = DRAGONFLY
elif args["network"] == "slimfly":
    network_type = SLIMFLY
elif args["network"] == "fattree":
    network_type = FATTREE
else:
    sys.exit("INVALID TYPE for --network")

terminal_out = open(stem + "-terminal-output.csv", "w")
router_out = open(stem + "-router-output.csv", "w")
pe_out = open(stem + "-pe-output.csv", "w")
kp_out = open(stem + "-kp-output.csv", "w")
lp_out = open(stem + "-lp-output.csv", "w")

radix = int(args["radix"])

# write out headers
pe_out.write("PE,VT,RT,last_gvt,num_gvt,rb_total,rb_secondary,fwd_ev,ev_abort,rev_ev,nsend_net,nrecv_net,fc_attempts,pq_size,ev_ties,net_read_CC,gvt_CC,fossil_collect_CC,event_abort_CC,event_process_CC,pq_CC,rollback_CC,cancel_q_CC,avl_CC,lookahead\n")
kp_out.write("KP,PE,VT,RT,time_ahead_gvt,efficiency,rb_total,rb_primary,rb_secondary,fwd_ev,rev_ev,network_sends,network_recvs\n")
lp_out.write("LP,KP,PE,VT,RT,fwd_ev,rev_ev,network_sends,network_recvs\n")

if network_type == DRAGONFLY:
    terminal_out.write("LP,KP,PE,terminal_id,fin_chunks,data_size,fin_hops,fin_chunks_time,busy_time,end_time,fwd_events,rev_events\n")
    router_out.write("LP,KP,PE,router_id,end_time,fwd_events,rev_events")
    for i in range(radix):
        router_out.write(",busy_time_" + str(i))
    for i in range(radix):
        router_out.write(",link_traffic_" + str(i))
    router_out.write("\n")
elif network_type == SLIMFLY or network_type == FATTREE:
    terminal_out.write("LP,KP,PE,terminal_id,end_time,vc_occupancy_sum\n")
    router_out.write("LP,KP,PE,router_id,end_time")
    for i in range(radix):
        router_out.write(",vc_occupancy_sum_" + str(i))
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
                else:
                    struct_str = "@Qdqdll" + str(radix) + "d" + str(radix) + "q"
            elif network_type == SLIMFLY or network_type == FATTREE:
                if metadata[sample_sz] == 24: #accounting for end padding
                    struct_str = "@Qdii"
                else:
                    struct_str = "@Qqd" + str(radix) + "i"

        #print(struct_str)
        #print(metadata[sample_sz])
        data = struct.unpack(struct_str, binary_file.read(metadata[sample_sz]))
        pos += metadata[sample_sz]
        #print(data)

        if metadata[flag] == 0: # PE data
            pe_data = []
            pe_data.extend(metadata[peid:real_time+1])
            pe_data.extend(data)
            pe_out.write(','.join(str(p) for p in pe_data))
            pe_out.write("\n")
        elif metadata[flag] == 1: # KP data
            kp_data = []
            kp_data.extend(metadata[kpid:real_time+1])
            kp_data.extend(data[0:3])
            kp_data.append(data[2] - data[3])
            kp_data.extend(data[3:])
            kp_out.write(','.join(str(p) for p in kp_data))
            kp_out.write("\n")
        elif metadata[flag] == 2: # LP data
            lp_data = []
            lp_data.extend(metadata[lpid:real_time+1])
            lp_data.extend(data)
            lp_out.write(','.join(str(p) for p in lp_data))
            lp_out.write("\n")
        elif metadata[flag] == 3: #  model data
            if network_type == DRAGONFLY:
                if (metadata[sample_sz] == 72):
                    terminal_out.write(','.join(str(p) for p in metadata[lpid:peid+1]) + ",")
                    terminal_out.write(','.join(str(p) for p in data))
                    terminal_out.write("\n")
                else:
                    new_list = []
                    new_list.append(data[0]) # elements 1 and 2 are just mem addresses
                    new_list.extend(data[3:])
                    #print(new_list)
                    router_out.write(','.join(str(p) for p in metadata[lpid:peid+1]) + ",")
                    router_out.write(','.join(str(p) for p in new_list))
                    router_out.write("\n")
            elif network_type == SLIMFLY or network_type == FATTREE:
                if metadata[sample_sz] == 24:
                    terminal_out.write(','.join(str(p) for p in metadata[lpid:peid+1]) + ",")
                    terminal_out.write(','.join(str(p) for p in data[0:-1]))
                    terminal_out.write("\n")
                else:
                    new_list = []
                    new_list.append(data[0]) # element 2 is just mem address
                    new_list.extend(data[2:])
                    #print(new_list)
                    router_out.write(','.join(str(p) for p in metadata[lpid:peid+1]) + ",")
                    router_out.write(','.join(str(p) for p in new_list))
                    router_out.write("\n")

    print("position == " + str(pos))
