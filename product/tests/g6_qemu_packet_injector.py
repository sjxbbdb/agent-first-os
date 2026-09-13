import socket
import time

frame = bytes.fromhex("ffffffffffff52540012345608004500002e0001000040110000c0a80001c0a8000212340035001a0000473652582d5041434b4554")
with socket.socket() as server:
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("127.0.0.1", 19090))
    server.listen(1)
    server.settimeout(8)
    try:
        connection, _ = server.accept()
        with connection:
            connection.sendall(frame)
            time.sleep(1)
    except socket.timeout:
        raise SystemExit("injector timeout waiting for QEMU socket")
