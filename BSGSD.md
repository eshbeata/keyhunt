# BSGSD

`BSGS` method but as a local `server`; the final `D` stands for daemon.

### Compilation

```
make bsgsd
```

The `bsgsd` target uses the same optimized flags as `keyhunt` (AVX2, `-flto`, `-O3 -ffast-math`). On x86-64 this automatically enables 8-wide SIMD hashing and the improved bloom filter (XXH3_128 + prefetch).

### Parameters

 - `-6`         Skip file checksum verification
 - `-t number`  Number of threads
 - `-k factor`  K factor (same meaning as keyhunt; controls baby-step table size)
 - `-n number`  Range length to scan per cycle (decimal or `0x` hex); must have an exact integer square root
 - `-i ip`      Listening IP address (default: `127.0.0.1`)
 - `-p port`    Listening port (default: `8080`)

bsgsd uses the same keyhunt files `.blm` and `.tbl`.

### Server

This is a minimal TCP server with no standard protocol. By default it listens only on `localhost:8080`.

The main advantage is that BSGS bloom filters and the bP table are loaded into RAM once at startup and reused across all client requests — no reload overhead per query. Clients send a single line and wait for a single-line reply.

Format of the client request:
```
<publickey> <range from>:<range to>
```
example puzzle 63

```
0365ec2994b8cc0a20d40dd69edfe55ca32a54bcbbaa6b0ddcff36049301a54579 4000000000000000:8000000000000000
```
The search is done Sequentialy Client need to knows more o less the time expect time to solve.

The server only reply one single line. Client must read that line and proceed according its content, possible replies:

 - `404 Not Found` if the key wasn't in the given range
 - `400 Bad Request`if there is some error on client request
 - `value` hexadecimal value with the Private KEY in case of be found 

The server will close the Conection inmediatly after send that line, also in case some other error the server will close the Conection without send any error message. Client need to hadle the Conection status by his own.

### Example

Run the server in one terminal:
```
./bsgsd -k 4096 -t 8 -6
[+] Version 0.2.230519 Satoshi Quest, developed by AlbertoBSD
[+] K factor 4096
[+] Threads : 8
[W] Skipping checksums on files
[+] Mode BSGS sequential
[+] N = 0x100000000000
[+] Bloom filter for 17179869184 elements : 58890.60 MB
[+] Bloom filter for 536870912 elements : 1840.33 MB
[+] Bloom filter for 16777216 elements : 57.51 MB
[+] Allocating 256.00 MB for 16777216 bP Points
[+] Reading bloom filter from file keyhunt_bsgs_4_17179869184.blm .... Done!
[+] Reading bloom filter from file keyhunt_bsgs_6_536870912.blm .... Done!
[+] Reading bP Table from file keyhunt_bsgs_2_16777216.tbl .... Done!
[+] Reading bloom filter from file keyhunt_bsgs_7_16777216.blm .... Done!
[+] Listening in 127.0.0.1:8080
```
Once that you see `[+] Listening in 127.0.0.1:8080` the server is ready to process client requests

Now we can connect it in annother terminal with `netcat` as client, this server is `64 GB` ram, expected time for puzzle 63 `~8` Seconds

command:
```
time echo "0365ec2994b8cc0a20d40dd69edfe55ca32a54bcbbaa6b0ddcff36049301a54579 4000000000000000:8000000000000000" | nc -v localhost 8080
```
```
time echo "0365ec2994b8cc0a20d40dd69edfe55ca32a54bcbbaa6b0ddcff36049301a54579 4000000000000000:8000000000000000" | nc -v localhost 8080
localhost.localdomain [127.0.0.1] 8080 (http-alt) open
7cce5efdaccf6808
real    0m7.551s
user    0m0.002s
sys     0m0.001s
```
If you notice the answer from the server is `7cce5efdaccf6808`

**Other example `404 Not Found`:**

```
time echo "0233709eb11e0d4439a729f21c2c443dedb727528229713f0065721ba8fa46f00e 4000000000000000:8000000000000000" | nc -v localhost 8080
localhost.localdomain [127.0.0.1] 8080 (http-alt) open
404 Not Found
real    0m7.948s
user    0m0.003s
sys     0m0.000s
```

### One client at the time
To maximize the Speed of BSGS this server only attends one client at the time.
I know what are you thinking, but if you are doing 10 ranges of 63 bits, you can send only one range and the time and the program only will take 80 seconds (Based on the speed of the previous example).

But if i do the program multi-client, and you send the 10 ranges at the same time in 10 different connections, the whole process will also take 80 seconds... so is only question of how your client send the data and manage the ranges..

### Client

Here is a small python example to implent by your self as client.

```
import socket
import time

def send_and_receive_line(host, port, message):
    # Create a TCP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        # Connect to the server
        sock.connect((host, port))

        # Send the message
        start_time = time.time()
        sock.sendall(message.encode())

        # Receive the reply
        reply = sock.recv(1024).decode()
        end_time = time.time()

        # Calculate the elapsed time
        elapsed_time = end_time - start_time
        sock.close()
        return reply, elapsed_time

    except ConnectionResetError:
        print("Server closed the connection without replying.")
        return None, None

    except ConnectionRefusedError:
        print("Connection refused. Make sure the server is running and the host/port are correct.")
        return None, None

    except AttributeError:
        pass
        return None, None

		
# TCP connection details
host = 'localhost'  # Change this to the server's hostname or IP address
port = 8080       # Change this to the server's port number

# Message to send
message = '0365ec2994b8cc0a20d40dd69edfe55ca32a54bcbbaa6b0ddcff36049301a54579 4000000000000000:8000000000000000'

# Number of iterations in the loop
num_iterations = 5

# Loop for sending and receiving messages
for i in range(num_iterations):
    reply, elapsed_time = send_and_receive_line(host, port, message)
    if reply is not None:
        print(f'Received reply: {reply}')
        print(f'Elapsed time: {elapsed_time} seconds')
```

The previous client example only repeat 5 times the same target, change it according to your needs.