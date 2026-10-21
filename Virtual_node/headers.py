import socket
import struct

CONNECT = 1
ON_VN = 2
VN_VN = 3

CONNECT_FORMAT = "!B4sH"
LINK_HEADER_FORMAT = "!BB"
NODE_FORMAT = "!c4sHH"
LSP_HEADER_FORMAT = "!BcIB"
LSP_LINK_FORMAT = "!cH"

def recv_exact(sock,n):
    data = b""
    while len(data) < n:
        packet = sock.recv(n - len(data))
        if not packet:
            raise ConnectionError("Socket closed before receiving enough data")
        data += packet
    return data
############## encode_connect
def encode_connect(ip,port):
    return struct.pack(
        CONNECT_FORMAT,
        CONNECT,
        socket.inet_aton(ip),
        port
    )
#################decode_connect
def decode_connect(data):
    if len(data) != struct.calcsize(CONNECT_FORMAT):
        raise ValueError("Invalid CONNECT length")
    msg_type,ip,port = struct.unpack(CONNECT_FORMAT,data)
    if msg_type != CONNECT:
        raise ValueError("not connect message")
    return socket.inet_ntoa(ip),port 

###############read and decode a(one) Link_state message from TCP
def receive_link_state(sock):
    header = recv_exact(sock, struct.calcsize(LINK_HEADER_FORMAT))
    msg_type,count = struct.unpack(LINK_HEADER_FORMAT, header)
    if msg_type != ON_VN:
        raise ValueError("not ON_VN message")
    node_size = struct.calcsize(NODE_FORMAT)
    body = recv_exact(sock, count * node_size)
    nodes = []
    for i in range(count):
        start = i * node_size
        end = start + node_size
        node_id,ip,port,cost = struct.unpack(NODE_FORMAT, body[start:end])
        nodes.append((node_id.decode("ascii"), socket.inet_ntoa(ip), port, cost))
    return nodes
################encode a Link_state message to TCP
def encode_link_state(nodes):
    if len(nodes) > 26:
        raise ValueError("Too many nodes to encode")
    data = struct.pack(LINK_HEADER_FORMAT, ON_VN, len(nodes))
    for node_id, ip, port, cost in nodes:
        data += struct.pack(NODE_FORMAT, node_id.encode("ascii"), socket.inet_aton(ip), port, cost)
    return data
##################encode a LSP message based on UDP,links is a list of tuple with format (neighbor_id, cost)

def encode_lsp(origin,seq,links):
    if len(links) > 26:
        raise ValueError("Too many links to encode")
    data = struct.pack(LSP_HEADER_FORMAT, VN_VN, origin.encode("ascii"), seq,len(links))
    for neighbor_id,cost in links:
        data += struct.pack(LSP_LINK_FORMAT,neighbor_id.encode("ascii"),cost)
    return data
##################decode a LSP message based on UDP return a tuple (origin,seq,links)
def decode_lsp(data):
    header_size = struct.calcsize(LSP_HEADER_FORMAT)
    if (len(data) < header_size):
        raise ValueError("Data is too short to contain a valid LSP header")
    msg_type,origin,seq,count = struct.unpack(LSP_HEADER_FORMAT,data[:header_size])
    if msg_type != VN_VN:
        raise ValueError("not VN_VN message")
    links = []
    link_size = struct.calcsize(LSP_LINK_FORMAT)
    expected_size = header_size + count*link_size
    if len(data) != expected_size:
        raise ValueError("Data is not the expected size")
    for i in range(count):
        start = header_size + i*link_size
        end = start + link_size
        neighbor_id,cost = struct.unpack(LSP_LINK_FORMAT,data[start:end])
        links.append((neighbor_id.decode("ascii"),cost))
    return origin.decode("ascii"),seq,links

    





