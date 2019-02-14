import sys
import struct
import argparse

ap = argparse.ArgumentParser()
ap.add_argument("-f", "--filename", required=True, help="binary file to convert")
ap.add_argument("-m", "--machine_map", required=False, help="a csv of machine mapping data (csv: MPI rank,node id, socket id, core id)")
args = vars(ap.parse_args())

filename = args["filename"]
stem = filename.split(".")[0]

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

def setup_pe_output():
    pe_out = open(stem + "-pe-output.csv", "w")
    if mmap:
        pe_out.write("node,socket,core,")
    pe_out.write("PE,last_GVT,RT,event_proc,ev_abort,ev_rb,rb_total,rb_primary,rb_secondary,fc_attempts,pq_size,nsend_net,nrecv_net,num_gvt,ev_ties,allred_count,net_read_CC,net_other_CC,gvt_CC,fossil_collect_CC,event_abort_CC,event_process_CC,pq_CC,rollback_CC,cancel_q_CC,avl_CC,buddy_CC,lz4_CC\n")
    return pe_out

def setup_kp_output():
    kp_out = open(stem + "-kp-output.csv", "w")
    if mmap:
        kp_out.write("node,socket,core,")
    kp_out.write("PE,KP,last_GVT,RT,event_proc,ev_abort,ev_rb,rb_total,rb_primary,rb_secondary,nsend_net,nrecv_net,virtual_time_diff\n")
    return kp_out

def setup_lp_output():
    lp_out = open(stem + "-lp-output.csv", "w")
    if mmap:
        lp_out.write("node,socket,core,")
    lp_out.write("PE,KP,LP,last_GVT,RT,event_proc,ev_abort,ev_rb,nsend_net,nrecv_net\n")
    return lp_out

metadata_sz = 40
last_gvt = 0
vts = 1
rts = 2
peid = 3
has_pe = 4
has_kp = 5
has_lp = 6
has_model = 7

pe_out = None
kp_out = None
lp_out = None

with open(filename, "rb") as binary_file:
    binary_file.seek(0, 2)
    num_bytes = binary_file.tell()
    print("num_byes == " + str(num_bytes))

    binary_file.seek(0)
    file_md = struct.unpack("@II", binary_file.read(8))
    npe = file_md[0];
    print(npe)
    nkp = file_md[1];
    print(nkp)
    nlp = struct.unpack("@" + str(npe) + "Q", binary_file.read(npe * 8))
    print(nlp)

    pos = 8 + (npe * 8)
    while (pos < num_bytes):
        binary_file.seek(pos)
        metadata = struct.unpack("@3dI4hI", binary_file.read(metadata_sz))
        #print(metadata)
        pos += metadata_sz
        binary_file.seek(pos)
        struct_str = ""

        if metadata[has_pe] == 1:
            if pe_out is None:
                pe_out = setup_pe_output()

            struct_str = "@14I12f"
            pe_size = 14*4 + 12*4
            data = struct.unpack(struct_str, binary_file.read(pe_size))
            pos += pe_size

            if mmap:
                pe_out.write(mmap[metadata[peid]]["node"] + ",")
                pe_out.write(mmap[metadata[peid]]["socket"] + ",")
                pe_out.write(mmap[metadata[peid]]["core"] + ",")
            pe_out.write(str(data[0]) + ",")
            pe_out.write(str(metadata[last_gvt]) + ",")
            pe_out.write(str(metadata[rts]) + ",")
            pe_out.write(','.join(str(p) for p in data[1:]))
            pe_out.write("\n")

        if metadata[has_kp] == 1:
            if kp_out is None:
                kp_out = setup_kp_output()

            struct_str = "@9If"
            kp_size = 40
            for i in range(nkp):
                data = struct.unpack(struct_str, binary_file.read(kp_size))
                pos += kp_size

                if mmap:
                    kp_out.write(mmap[metadata[peid]]["node"] + ",")
                    kp_out.write(mmap[metadata[peid]]["socket"] + ",")
                    kp_out.write(mmap[metadata[peid]]["core"] + ",")

                kp_out.write(str(metadata[peid]) + "," + str(data[0]) + ",")
                kp_out.write(str(metadata[last_gvt]) + ",")
                kp_out.write(str(metadata[rts]) + ",")
                kp_out.write(','.join(str(p) for p in data[1:]))
                kp_out.write("\n")

        if metadata[has_lp] == 1:
            if lp_out is None:
                lp_out = setup_lp_output()

            struct_str = "@7I"
            lp_size = 28

            for i in range(nlp[metadata[peid]]):
                data = struct.unpack(struct_str, binary_file.read(lp_size))
                pos += lp_size

                if mmap:
                    lp_out.write(mmap[metadata[peid]]["node"] + ",")
                    lp_out.write(mmap[metadata[peid]]["socket"] + ",")
                    lp_out.write(mmap[metadata[peid]]["core"] + ",")

                lp_out.write(str(metadata[peid]) + "," + str(data[0]) + "," + str(data[1]) + ",")
                lp_out.write(str(metadata[last_gvt]) + ",")
                lp_out.write(str(metadata[rts]) + ",")
                lp_out.write(','.join(str(p) for p in data[2:]))
                lp_out.write("\n")

    print("position == " + str(pos))
