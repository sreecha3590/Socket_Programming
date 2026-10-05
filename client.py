import socket

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
server_addr = ("127.0.0.1", 3490)
s.sendto(b"Hello from Client!", server_addr)
data, addr = s.recvfrom(1024)
print(f"Server replied: {data.decode()}")
s.close()