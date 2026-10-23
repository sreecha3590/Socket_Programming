import select
import socket
import sys
import headers
import time
import struct

on_ip = sys.argv[1]
my_ip = sys.argv[2]
my_port = int(sys.argv[3])

try:
    udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_socket.bind((my_ip, my_port))
except OSError as e:
    print(f"Error: Failed to bind UDP socket on {my_ip}:{my_port} - {e}", file=sys.stderr)
    sys.exit(1)

try:
    tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    tcp_socket.connect((on_ip, 5000))
except OSError as e:
    print(f"Error: Failed to connect to Oracle Node at {on_ip}:5000 - {e}", file=sys.stderr)
    udp_socket.close()
    sys.exit(1)

tcp_socket.sendall(headers.encode_connect(my_ip, my_port))
nodes = headers.receive_link_state(tcp_socket)
my_id = None
my_neighbors = {}
node_address = {}

for node_id, ip, port, cost in nodes:
    node_address[node_id] = (ip, port)
    if cost == 0:
        my_id = node_id
    else:
        my_neighbors[node_id] = cost
if my_id is None:
    raise ValueError("My node ID not found in the received nodes")

print("my_id:", my_id)
print("my_neighbors:", my_neighbors)

seq = 0
latest_seq = {my_id: seq}

graph = {my_id : my_neighbors.copy()}

############my own lsp

def my_own_lsp():
    global seq 
    seq += 1
    latest_seq[my_id] = seq
    graph[my_id] = my_neighbors.copy()
    message  = headers.encode_lsp(my_id, seq, list(my_neighbors.items()))
    for neighbor_id in my_neighbors:
        ip,port = node_address[neighbor_id]
        address = (ip,port)
        ############ for better reliability we aresending 2 packets of same info to each neighbor
        udp_socket.sendto(message, address)
        udp_socket.sendto(message, address)


my_own_lsp()

last_lsp_time = time.time() 
last_print_time = time.time()

############print graph currently known for every 15 seconds

def print_graph():
    print("Current graph:")
    for node_id in sorted(graph):
        print (node_id, ":", graph[node_id])
    print()

try:
    while True:
        read_sockets,_,_ = select.select([tcp_socket,udp_socket], [], [], 1)
        for sock in read_sockets:
            if sock == tcp_socket:
                new_nodes = headers.receive_link_state(tcp_socket)
                new_neighbors = {}
                new_node_address = {}
                for node_id,ip,port,cost in new_nodes:
                    new_node_address[node_id] = (ip, port)
                    if cost == 0:
                        my_id = node_id
                    else:
                        new_neighbors[node_id] = cost
                node_address.update(new_node_address)
                if new_neighbors != my_neighbors:
                    my_neighbors = new_neighbors
                    print("Neighbors updated:", my_neighbors)
                    my_own_lsp()
                    last_lsp_time = time.time()
            elif sock == udp_socket:
                data,addr = udp_socket.recvfrom(1024)
                try:
                    origin, seq_num, links = headers.decode_lsp(data)
                except (ValueError,struct.error):
                    continue
                if origin == my_id or latest_seq.get(origin,-1) >= seq_num:
                    continue
                latest_seq[origin] = seq_num
                graph[origin] = dict(links)
                for neighbor_id in my_neighbors:
                    ip,port = node_address[neighbor_id]
                    address = (ip,port)
                    if address != addr:
                        udp_socket.sendto(data, address)
        current_time = time.time()
        if current_time - last_lsp_time >= 10:
            my_own_lsp()
            last_lsp_time = current_time
        if current_time - last_print_time >= 15:
            print_graph()
            last_print_time = current_time
except KeyboardInterrupt:
    print("exiting virtual node....")

finally:
    tcp_socket.close()
    udp_socket.close()



    







