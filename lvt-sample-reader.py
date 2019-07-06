import sys
import struct
import argparse

ap = argparse.ArgumentParser()
ap.add_argument("-f", "--filename", required=True, help="binary file to convert")
args = vars(ap.parse_args())

filename = args["filename"]
stem = filename.split(".")[0]

def setup_kp_output():
    kp_out = open(stem + "-kp-output.csv", "w")
    kp_out.write("PE,KP,GVT,RT,LVT\n")
    return kp_out

### Struct info for ROSS sample_metadata struct
metadata_sz = 56
last_gvt = 0
vts = 1
rts = 2
peid = 3
kp_gid = 4
has_pe = 5
has_kp = 6
has_lp = 7
has_model = 8
num_model_lps = 9
md_struct_str = "@3dI7i"

### Info for ROSS st_kp_lvt_samp struct
kp_struct_sz = 16
kpid = 0
lvt = 1
kp_struct_str = "@Id"

kp_out = None

with open(filename, "rb") as binary_file:
    binary_file.seek(0, 2)
    num_bytes = binary_file.tell()
    #num_bytes=20000000
    print("num_bytes == " + str(num_bytes))

    binary_file.seek(0)
    file_md_sz = 12
    file_md = struct.unpack("@IIi", binary_file.read(file_md_sz))
    npe = file_md[0];
    print(npe)
    nkp = file_md[1];
    print(nkp)
    inst_mode = file_md[2]
    print(inst_mode)
    vt = last_gvt
    if inst_mode == 2: # VTS
        vt = vts
    #nlp_list = struct.unpack("@" + str(npe) + "Q", binary_file.read(npe * 8))
    #print(nlp_list)

    pos = file_md_sz
    while (pos < num_bytes):
        binary_file.seek(pos)
        metadata = struct.unpack(md_struct_str, binary_file.read(metadata_sz))
        #print(metadata)
        pos += metadata_sz
        binary_file.seek(pos)

        if metadata[has_kp] > 0:
            if kp_out is None:
                kp_out = setup_kp_output()

            nkp = metadata[has_kp]
            for i in range(nkp):
                data = struct.unpack(kp_struct_str, binary_file.read(kp_struct_sz))
                pos += kp_struct_sz
                binary_file.seek(pos)

                kp_out.write(str(metadata[peid]) + "," + str(data[kpid]) + ",")
                kp_out.write(str(metadata[vt]) + ",")
                kp_out.write(str(metadata[rts]) + ",")
                kp_out.write(str(data[lvt]))
                kp_out.write("\n")


    print("position == " + str(pos))
