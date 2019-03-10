import struct
import argparse

ap = argparse.ArgumentParser()
ap.add_argument("-f", "--filename", required=True, help="binary file to convert")
args = vars(ap.parse_args())

filename = args["filename"]
stem = filename.split(".")[0]

event_data_sz = 24
src_lp = 0
dest_lp = 1
send_vts = 2
recv_vts = 3
real_ts = 4
model_data = 5
ev_struct_str = "@IIfffI"

event_out = open(stem + ".csv", "w")
event_out.write("src_lp,dest_lp,send_ts,recv_ts,real_ts,event_id\n")
event_count = 0

with open(filename, "rb") as binary_file:
    binary_file.seek(0, 2)
    num_bytes = binary_file.tell()
    print("num_bytes == " + str(num_bytes))

    binary_file.seek(0)
    file_md_sz = 12
    file_md = struct.unpack("@IIi", binary_file.read(file_md_sz))
    npe = file_md[0];
    #print(npe)
    nkp = file_md[1];
    #print(nkp)
    inst_mode = file_md[2]
    #print(inst_mode)

    pos = file_md_sz
    while pos < num_bytes:
        event_data = struct.unpack(ev_struct_str, binary_file.read(event_data_sz))
        pos += event_data_sz
        event_out.write(','.join(str(p) for p in event_data[:-1]))
        if event_data[model_data] > 0:
            msg_type = struct.unpack("@i", binary_file.read(event_data[model_data]))
            pos += event_data[model_data]
            event_out.write("," + str(msg_type[0]))
        else:
            event_out.write(",-1")
        event_out.write("\n")
        event_count += 1

    print("position: " + str(pos))
    print("event count: " + str(event_count))

