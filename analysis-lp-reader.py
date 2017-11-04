import sys
import struct

filename = sys.argv[1]

terminal_out = open("terminal-output.csv", "w")
router_out = open("router-output.csv", "w")
pe_out = open("pe-output.csv", "w")
kp_out = open("kp-output.csv", "w")
lp_out = open("lp-output.csv", "w")

radix = int(sys.argv[2])

# write out headers
pe_out.write("PE,VT,RT,last_gvt,num_gvt,net_read_CC,gvt_CC,fossil_collect_CC,event_abort_CC,event_process_CC,pq_CC,rollback_CC,cancel_q_CC,avl_CC\n")
kp_out.write("KP,PE,VT,RT,time_ahead_gvt,efficiency,rb_total,rb_primary,rb_secondary,fwd_ev,rev_ev,network_sends,network_recvs\n")
lp_out.write("LP,KP,PE,VT,RT,fwd_ev,rev_ev,network_sends,network_recvs\n")
terminal_out.write("LP,KP,PE,terminal_id,fin_chunks,data_size,fin_hops,fin_chunks_time,busy_time,end_time,fwd_events,rev_events\n")
router_out.write("LP,KP,PE,router_id,end_time,fwd_events,rev_events")
#busy_time,link_traffic\n")
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
            struct_str = "@dQ9d"
        elif metadata[flag] == 1: # KP data
            struct_str = "@ddQQQQQQ"
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

    print("position == " + str(pos))
