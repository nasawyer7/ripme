import socket
import threading
import time

# CONFIG
VM_IP = '10.5.5.6'
VM_PORT = 5977
LISTEN_PORT = 5999

def handle_user(user_socket):
    vm_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        vm_socket.connect((VM_IP, VM_PORT))
    except Exception as e:
        print(f"Failed to connect to VM: {e}")
        user_socket.close()
        return

    # Using an Event to signal all threads to stop when one fails
    stop_event = threading.Event()
    input_allowed = [True] # Use a list to make it mutable in threads

    def cleanup():
        stop_event.set()
        user_socket.close()
        vm_socket.close()


    def vm_to_user():
        try:
            while not stop_event.is_set():
                data = vm_socket.recv(16384)
                if not data: break
                user_socket.sendall(data)
        except: pass
        finally: cleanup()

    
    def user_to_vm():
        try:
            while not stop_event.is_set():
                data = user_socket.recv(4096)
                if not data: break

                if input_allowed[0]:
                    # Allow handshake and first FramebufferUpdateRequest
                    vm_socket.sendall(data)
                    if b'\x03' in data[:1]:
                        print("!!! STREAM HANDSHAKE COMPLETE: LOBOTOMIZING INPUT !!!")
                        input_allowed[0] = False
                else:
                    # After lobotomy, we ignore mouse/key events (Types 4, 5, 6)
                    # But we MUST keep the connection alive
                    pass
        except: pass
        finally: cleanup()

   
    def ghost_heartbeat():
        # Standard VNC "Incremental Update Request" (10 bytes)
        update_req = b'\x03\x01\x00\x00\x00\x00\x07\xd0\x07\xd0'
        try:
            while not stop_event.is_set():
                if not input_allowed[0]:
                    vm_socket.sendall(update_req)
                time.sleep(0.05)
        except: pass
        finally: cleanup()

    threading.Thread(target=vm_to_user, daemon=True).start()
    threading.Thread(target=user_to_vm, daemon=True).start()
    threading.Thread(target=ghost_heartbeat, daemon=True).start()

def main():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        server.bind(('0.0.0.0', LISTEN_PORT))
        server.listen(10) # Increased backlog for more users
        print(f"LOBOTOMY PROXY ACTIVE on {LISTEN_PORT}")

        while True:
            conn, addr = server.accept()
            print(f"New connection from {addr}")
            threading.Thread(target=handle_user, args=(conn,), daemon=True).start()
    except KeyboardInterrupt:
        server.close()

if __name__ == "__main__":
    main()
