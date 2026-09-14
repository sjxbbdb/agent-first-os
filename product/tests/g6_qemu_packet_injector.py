import socket
import struct
import time

frame = bytes.fromhex("ffffffffffff52540012345608004500002e0001000040110000c0a80001c0a8000212340035001a0000473652582d5041434b4554")
# Socket backends carry complete Ethernet frames; pad the short fixture to
# the Ethernet minimum so QEMU does not reject it before virtio RX.
frame = frame.ljust(60, b"\0")
wire_packet = struct.pack("!I", len(frame)) + frame
with socket.socket() as server:
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("127.0.0.1", 19090))
    server.listen(1)
    server.settimeout(8)
    try:
        connection, _ = server.accept()
        with connection:
            # QEMU connects before the BIOS guest has necessarily armed RX.
            # Keep the bounded fixture alive long enough for the descriptor to
            # become available; each write is a complete Ethernet frame.
            for _ in range(20):
                connection.sendall(wire_packet)
                time.sleep(0.1)
    except socket.timeout:
        raise SystemExit("injector timeout waiting for QEMU socket")
