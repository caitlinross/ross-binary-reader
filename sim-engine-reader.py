import sys
import struct
import argparse

ap = argparse.ArgumentParser()
ap.add_argument("-f", "--filename", required=True, help="binary file to convert")
ap.add_argument("-m", "--machine_map", required=False, help="a csv of machine mapping data (csv: MPI rank,node id, socket id, core id)")
ap.add_argument("-r", "--radix", required=False, help="if file contains model data, the radix of the network model")
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
    pe_out.write("PE,VT,RT,event_proc,ev_abort,ev_rb,rb_total,rb_primary,rb_secondary,fc_attempts,pq_size,nsend_net,nrecv_net,num_gvt,ev_ties,allred_count,net_read_CC,net_other_CC,gvt_CC,fossil_collect_CC,event_abort_CC,event_process_CC,pq_CC,rollback_CC,cancel_q_CC,avl_CC,buddy_CC,lz4_CC\n")
    return pe_out

def setup_kp_output():
    kp_out = open(stem + "-kp-output.csv", "w")
    if mmap:
        kp_out.write("node,socket,core,")
    kp_out.write("PE,KP,VT,RT,event_proc,ev_abort,ev_rb,rb_total,rb_primary,rb_secondary,nsend_net,nrecv_net,virtual_time_diff\n")
    return kp_out

def setup_lp_output():
    lp_out = open(stem + "-lp-output.csv", "w")
    if mmap:
        lp_out.write("node,socket,core,")
    lp_out.write("PE,KP,LP,VT,RT,event_proc,ev_abort,ev_rb,nsend_net,nrecv_net\n")
    return lp_out

def setup_model_output():
    radix = int(args["radix"])
    router_out = open(stem + "-router-output.csv", "w")
    terminal_out = open(stem + "-terminal_output.csv", "w")
    terminal_out.write("PE,KP,LP,end_time,terminal_id,fin_chunks,data_size,fin_hops,fin_chunks_time,busy_time,end_time\n")
    router_out.write("PE,KP,LP,end_time,router_id")
    for i in range(radix):
        router_out.write(",busy_time_" + str(i))
    for i in range(radix):
        router_out.write(",link_traffic_" + str(i))
    router_out.write("\n")

    return router_out, terminal_out

metadata_sz = 48
last_gvt = 0
vts = 1
rts = 2
peid = 3
has_pe = 4
has_kp = 5
has_lp = 6
has_model = 7
num_model_lps = 8

m_md_sz = 12
m_kpid = 0
m_lpid = 1
m_num_vars = 2

mv_md_sz = 12
mv_id = 0
mv_type = 1
mv_num_elems = 2

pe_out = None
kp_out = None
lp_out = None
router_out = None
terminal_out = None

INT_TYPE = 0
LONG_TYPE = 1
FLOAT_TYPE = 2
DOUBLE_TYPE = 3

def model_var_type_size(type):
    if type == INT_TYPE:
        return 4
    if type == LONG_TYPE:
        return 8
    if type == FLOAT_TYPE:
        return 4
    if type == DOUBLE_TYPE:
        return 8

    return -1

def model_var_type_char(type):
    if type == INT_TYPE:
        return "i"
    if type == LONG_TYPE:
        return "q"
    if type == FLOAT_TYPE:
        return "f"
    if type == DOUBLE_TYPE:
        return "d"

    return ""

with open(filename, "rb") as binary_file:
    binary_file.seek(0, 2)
    num_bytes = binary_file.tell()
    print("num_bytes == " + str(num_bytes))

    binary_file.seek(0)
    file_md_sz = 12
    file_md = struct.unpack("@IIi", binary_file.read(file_md_sz))
    npe = file_md[0];
    print(npe)
    nkp = file_md[1];
    print(nkp)
    inst_mode = file_md[2]
    vt = last_gvt
    if inst_mode == 2: # VTS
        vt = vts
    #nlp_list = struct.unpack("@" + str(npe) + "Q", binary_file.read(npe * 8))
    #print(nlp_list)

    pos = file_md_sz
    while (pos < num_bytes):
        binary_file.seek(pos)
        metadata = struct.unpack("@3dI5i", binary_file.read(metadata_sz))
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
            binary_file.seek(pos)

            if mmap:
                pe_out.write(mmap[metadata[peid]]["node"] + ",")
                pe_out.write(mmap[metadata[peid]]["socket"] + ",")
                pe_out.write(mmap[metadata[peid]]["core"] + ",")
            pe_out.write(str(data[0]) + ",")
            pe_out.write(str(metadata[vt]) + ",")
            pe_out.write(str(metadata[rts]) + ",")
            pe_out.write(','.join(str(p) for p in data[1:]))
            pe_out.write("\n")

        if metadata[has_kp] > 0:
            if kp_out is None:
                kp_out = setup_kp_output()

            struct_str = "@9If"
            kp_size = 40
            nkp = metadata[has_kp]
            for i in range(nkp):
                data = struct.unpack(struct_str, binary_file.read(kp_size))
                pos += kp_size
                binary_file.seek(pos)

                if mmap:
                    kp_out.write(mmap[metadata[peid]]["node"] + ",")
                    kp_out.write(mmap[metadata[peid]]["socket"] + ",")
                    kp_out.write(mmap[metadata[peid]]["core"] + ",")

                kp_out.write(str(metadata[peid]) + "," + str(data[0]) + ",")
                kp_out.write(str(metadata[vt]) + ",")
                kp_out.write(str(metadata[rts]) + ",")
                kp_out.write(','.join(str(p) for p in data[1:]))
                kp_out.write("\n")

        if metadata[has_lp] > 0:
            if lp_out is None:
                lp_out = setup_lp_output()

            struct_str = "@7I"
            lp_size = 28

            nlp = metadata[has_lp]
            for i in range(nlp):
                data = struct.unpack(struct_str, binary_file.read(lp_size))
                pos += lp_size
                binary_file.seek(pos)

                if mmap:
                    lp_out.write(mmap[metadata[peid]]["node"] + ",")
                    lp_out.write(mmap[metadata[peid]]["socket"] + ",")
                    lp_out.write(mmap[metadata[peid]]["core"] + ",")

                lp_out.write(str(metadata[peid]) + "," + str(data[0]) + "," + str(data[1]) + ",")
                lp_out.write(str(metadata[vt]) + ",")
                lp_out.write(str(metadata[rts]) + ",")
                lp_out.write(','.join(str(p) for p in data[2:]))
                lp_out.write("\n")

        if metadata[has_model] == 1:
            if router_out is None or terminal_out is None:
                router_out, terminal_out = setup_model_output()

            for mlp in range(metadata[num_model_lps]):
                # first get model metadata
                struct_str = "@IIi"
                model_md = struct.unpack(struct_str, binary_file.read(m_md_sz))
                pos += m_md_sz
                binary_file.seek(pos)
                #print("model_md: ")
                #print(model_md)

                struct_str = "@3i"
                if model_md[m_num_vars] == 3: # router
                    router_out.write(str(metadata[peid]) + "," + str(model_md[m_kpid]) + "," + str(model_md[m_lpid]) + ",")
                    router_out.write(str(metadata[vt]))
                    #router_out.write(str(metadata[rts]))
                    for i in range(model_md[m_num_vars]):
                        router_out.write(",")
                        var_md = struct.unpack(struct_str, binary_file.read(mv_md_sz))
                        pos += mv_md_sz
                        binary_file.seek(pos)
                        var_data_size = model_var_type_size(var_md[mv_type]) * var_md[mv_num_elems]
                        data_str = "@" + str(var_md[mv_num_elems]) + model_var_type_char(var_md[mv_type])
                        var_data = struct.unpack(data_str, binary_file.read(var_data_size))
                        pos += var_data_size
                        binary_file.seek(pos)
                        router_out.write(','.join(str(p) for p in var_data))
                    router_out.write("\n")

                elif model_md[m_num_vars] == 6: #terminal
                    terminal_out.write(str(metadata[peid]) + "," + str(model_md[m_kpid]) + "," + str(model_md[m_lpid]) + ",")
                    terminal_out.write(str(metadata[vt]))
                    #terminal_out.write(str(metadata[rts]))
                    for i in range(model_md[m_num_vars]):
                        terminal_out.write(",")
                        var_md = struct.unpack(struct_str, binary_file.read(mv_md_sz))
                        pos += mv_md_sz
                        binary_file.seek(pos)
                        var_data_size = model_var_type_size(var_md[mv_type]) * var_md[mv_num_elems]
                        data_str = "@" + str(var_md[mv_num_elems]) + model_var_type_char(var_md[mv_type])
                        var_data = struct.unpack(data_str, binary_file.read(var_data_size))
                        pos += var_data_size
                        binary_file.seek(pos)
                        terminal_out.write(','.join(str(p) for p in var_data))
                    terminal_out.write("\n")

                else:
                    sys.exit("Number of model variables incorrect!")
            

    print("position == " + str(pos))
