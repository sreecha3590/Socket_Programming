import socket

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(("127.0.0.1", 3490))
print("Server waiting")
data, addr = s.recvfrom(1024)
print(f"Client says: {data.decode()}")
s.sendto(b"Hello from Server!", addr)
s.close()