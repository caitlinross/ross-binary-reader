import sys
import struct
import argparse

ap = argparse.ArgumentParser()
ap.add_argument("-f", "--filename", required=True, help="binary file to convert")
ap.add_argument("-m", "--machine_map", required=False, help="a csv of machine mapping data (csv: MPI rank,node id, socket id, core id)")
args = vars(ap.parse_args())

filename = args["filename"]
stem = filename.split(".")[0]

pe_out = open(stem + "-pe-output.csv", "w")
kp_out = open(stem + "-kp-output.csv", "w")
lp_out = open(stem + "-lp-output.csv", "w")

mmap = {}

print(args["machine_map"])
#if "machine_map" in args:
if args["machine_map"] is not None:
    machine_data = open(args["machine_map"], "r")
    for line in machine_data:
        if not line.startswith("#"):
            tokens=line.strip().split(",")
            mmap[int(tokens[0])] = {}
            mmap[int(tokens[0])]["node"] = tokens[1]
            mmap[int(tokens[0])]["socket"] = tokens[2]
            mmap[int(tokens[0])]["core"] = tokens[3]

    machine_data.close()

print (mmap)
# write out headers
if mmap:
    pe_out.write("node,socket,core,")
    kp_out.write("node,socket,core,")
    lp_out.write("node,socket,core,")
pe_out.write("PE,VT,RT,event_proc,ev_abort,ev_rb,rb_total,rb_secondary,fc_attempts,pq_size,nsend_net,nrecv_net,num_gvt,ev_ties,allred_count,efficiency,net_read_CC,net_other_CC,gvt_CC,fossil_collect_CC,event_abort_CC,event_process_CC,pq_CC,rollback_CC,cancel_q_CC,avl_CC,buddy_CC,lz4_CC\n")
kp_out.write("KP,PE,VT,RT,event_proc,ev_abort,ev_rb,rb_total,rb_secondary,nsend_net,nrecv_net,virtual_time_diff,efficiency\n")
lp_out.write("LP,KP,PE,VT,RT,event_proc,ev_abort,ev_rb,nsend_net,nrecv_net,efficiency\n")

metadata_sz = 24

flag = 0
sample_sz = 1
ts = 2
real_time = 3

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

        if metadata[flag] == 0: # PE data
            struct_str = "@13I13f"
        elif metadata[flag] == 1: # KP data
            struct_str = "@9I2f"
        elif metadata[flag] == 2: # LP data
            struct_str = "@8If"
        elif metadata[flag] == 3: #  model data
            pass

        #print(struct_str)
        #print(metadata[sample_sz])
        data = struct.unpack(struct_str, binary_file.read(metadata[sample_sz]))
        pos += metadata[sample_sz]
        #print(data)

        if metadata[flag] == 0: # PE data
            pe_data = []
            if mmap:
                pe_data.append(mmap[data[0]]["node"])
                pe_data.append(mmap[data[0]]["socket"])
                pe_data.append(mmap[data[0]]["core"])
            pe_data.append(data[0])
            pe_data.extend(metadata[ts:])
            pe_data.extend(data[1:])
            pe_out.write(','.join(str(p) for p in pe_data))
            pe_out.write("\n")
        elif metadata[flag] == 1: # KP data
            kp_data = []
            if mmap:
                kp_data.append(mmap[data[0]]["node"])
                kp_data.append(mmap[data[0]]["socket"])
                kp_data.append(mmap[data[0]]["core"])
            kp_data.extend(data[0:2])
            kp_data.extend(metadata[ts:])
            kp_data.extend(data[2:])
            kp_out.write(','.join(str(p) for p in kp_data))
            kp_out.write("\n")
        elif metadata[flag] == 2: # LP data
            lp_data = []
            if mmap:
                lp_data.append(mmap[data[0]]["node"])
                lp_data.append(mmap[data[0]]["socket"])
                lp_data.append(mmap[data[0]]["core"])
            lp_data.extend(data[0:3])
            lp_data.extend(metadata[ts:])
            lp_data.extend(data[3:])
            lp_out.write(','.join(str(p) for p in lp_data))
            lp_out.write("\n")
        elif metadata[flag] == 3: #  model data
            pass

    print("position == " + str(pos))
