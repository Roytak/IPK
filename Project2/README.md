# IPKCPD - A server for remote calculator

IPKCPD is a remote calculator server, which uses thes *IPK Calculator* protocol[1] for communication. It is non-blocking supports multiple (up to 128) clients. The server was implemented by Roman Janota.

## Command-line arguments
The server can be run in the UDP or TCP mode. The listening address and port both must be specified.

```
    --help (-H)
        prints a help message
    --host (-h) <host>
        sets the listening address
    --port (-p) <port>
        sets the listening port
    --mode (-m) <mode>
        sets the internet protocol, can be either TCP or UDP
    --tcptest (-t)
        runs a TCP server with default parameters, that is host = 127.0.0.1, port = 9999, mode = TCP
    --udptest (-u)
        runs a UDP server with default parameters, that is host = 127.0.0.1, port = 9999, mode = UDP    
```

## A closer look at a TCP server

First of all, what even is TCP? TCP (or Transmission Control Protocol)[2] is a highly dependent connection based internet protocol. This means that each packet is guaranteed to reach it's destination under the right circumstances. A host to host connection has to be set up first. That is the first thing that the server's TCP implementation does. A function, which initializes the server is called. This function does the basic setup of the server. One of them is to call the standard library function socket(), which creates an endpoint for communication and returns a file descriptor that refers to that endpoint. After a socket is created, it is set to a non-blocking mode. The term non-blocking refers to a behaviour of the socket such that it doesn't wait for I/O operations to be ready. Instead it returns immediately and if the I/O operation wasn't ready the information is stored somewhere and the operation can be tried again later.

Next comes a call to the standard function bind(), which assigns an actual address to the file descriptor. The clients can then connect to this address. Finally a call to listen() is made. This function sets the socket as a *passive* socket, meaning that it will not make any connections itself, but rather it will listen for connections on the given address.

After a successful initialization of the server's socket, a main loop commences. This loop runs until the interrupt signal is sent to the server and does four things: accept a new connection, create new context for the new client, create a new thread and pass it the created context and sleep.

### Accepting a new connection

The key thing I would like to point out in this section is that even though the server's socket is non-blocking, the server still calls the function select()[3]. Select is a relatively old and rather discouraging function to use. It allows a program to monitor multiple file descriptors, waiting until one or more of the file descriptors become ready for some class of I/O operation. I am aware that it might seem meaningless in my implementation, because the server's socket is already non-blocking, so there is no point to wait for it to be ready. However I use select, because by default it reacts to the interrupt signal[4] and that is very handy when dealing with a multithreaded program.

So the call the select blocks until a certain timeout is reached and if the server's socket is ready for reading, it calls the accept() function, else it just tries again after sleeping. This function is used for accepting new connections from clients. Normally, accept would block the socket as well (and wouldn't react to the interrupt signal in my case), but since select already found out that there is a connection pending on the server's socket, it returns instantly. The call to accept returns a client's socket and I immediately set this socket to non-blocking mode and return from this function.

### Handling multiple clients

Now that a socket of a new client is ready, a context for this client is created. It is used to store some information specific to each client, for example his read and write buffer. A new thread is then created. The starting point of a thread is a main controller of a TCP session. The thread is also given the new client's context. The main disadvantage of this approach is that even though the maximum number of the clients is 128, it could be way less and heavily depends on the machine the server is run on. Every thread requires quite a lot of memory and with a lot of connections the machine can quickly run out of free memory.

### A TCP session

Once a thread is created, it behaves in accordance to a finite state automata with 4 states: init, read, write and term. The initial state is init. In this state a hello message from the client is expected to arrive. If not, the next state is set to term. The term state sends the client a bye message and cancels the thread.

Reading and writing messages to or from a client are both done very similarly. A call to select() is made followed by a call to recv() or send() for receiving or sending a message, respectively. Both of these are blocking, but as mentioned above, this is what select solves and same goes for the interrupt signal.

### Parsing a message and calculating a result

When some bytes are received, the server checks, if the message contains a new line character. If not, then the server keeps waiting for new messages and appends them to the previous ones. This process ends when a message contains a new line character. The format of an IPK Protocol TCP messages is defined like so:
```
operator = "+" / "-" / "*" / "/"
expr = "(" operator 2*(SP expr) ")" / 1*DIGIT
query = "(" operator 2*(SP expr) ")"
hello = "HELLO" LF
solve = "SOLVE" SP query LF
result = "RESULT" SP 1*DIGIT LF
bye = "BYE" LF
```

When a solve request is received, it is validated using the rules of the grammar by a *recursive descent top-down* parser[5]. This parser simulates a derivation tree and recursively checks the query.

If the validation is successful a calculation comes. This is done by actually creating a binary tree. This tree stores the expressions in the prefix order and only stores the digits and the operators. The leafs are always digits. The tree is then traversed in post-order and the calculations are made recursively.

## The UDP mode

UDP (or User Datagram Protocol)[6] is an internet protocol just like TCP. The key differences are that UDP is connection-less and that the package delivery is not guaranteed. Whenever the server is ran in the UDP mode, the first things it does is the initialization of the server's socket. It is somewhat similar to the TCP variant, however there is no need to make the socket non-blocking, because the protocol itself is non-blocking by default. Next difference is that calling the functions listen() and accept() is redundant, because there will be no connection to clients' sockets.

### The UDP cycle

When the server's socket has been successfully initialized, the server enters the infinite main loop. I will not go into details here, because it is analogous to the TCP variant. The loop always starts with a call to select(). Again it is unnecessary, but it is a useful for it's ability to break out of it's blocking state.

Whenever there is an activity on the server's socket, the datagrams that were received are read. The message is parsed and a response is created. The answer is then sent back to the same client. Because there is no connection between the server and the client, the server needs to remember the address from which the datagrams were received and send the answer there. The datagrams are read by the standard function recvfrom(), which behaves similarly to the recv() call used in TCP, but has the extra address' parameters. Answer is sent back using the sendto() function, where the address has to be specified.

### Parsing an UDP request

The definition of the IPK Protocol UDP request follows:

```
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +---------------+---------------+-------------------------------+
 |     Opcode    |Payload Length |          Payload Data         |
 |      (8)      |      (8)      |                               |
 +---------------+---------------+ - - - - - - - - - - - - - - - +
 :                     Payload Data continued ...                :
 + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
 |                     Payload Data continued ...                |
 +---------------------------------------------------------------+
```

If the value of the opcode byte is 0, the datagram is a request. The payload data with the length of payload length is then parsed the same way as in TCP. Any data beyond the specified length is not looked at.

The IPK Protocol UDP response is defined like this:

```
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +---------------+---------------+---------------+---------------+
 |     Opcode    |  Status Code  |Payload Length | Payload Data  |
 |      (8)      |      (8)      |      (8)      |               |
 +---------------+---------------+---------------+ - - - - - - - +
 :                     Payload Data continued ...                :
 + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
 |                     Payload Data continued ...                |
 +---------------------------------------------------------------+
```

The server then computes the answer just like in TCP, however this time if something goes wrong, it is able to send error messages back to the client. An example of an error message might be a division by zero attempted.

## Testing

The server was tested manually. A couple of test results are listed now. In each TCP test the server was run like so : `./ipkcpd -t` and the client using the networking utility netcat[7] : `netcat localhost 9999`. As for the UDP tests the server was run using : `./ipkcpd -u` and it's client : `echo -n -e 'input' | netcat -u localhost 9999`.

|              |   Client Input   | Client Output |
|:------------:|:---------:|:------:|
| Basic TCP | HELLO\nSOLVE (+ 1 1)\nBYE\n | HELLO\nRESULT 2\nBYE\n | HELLO\nRESULT 2\nBYE\n |
|   TCP invalid hello message   | hi\n |    BYE\n    |
|       TCP invalid query       |    HELLO\nSOLVE (/ 1 8(\n       |     HELLO\nBYE\n   |
|       TCP nested       |    HELLO\nSOLVE (+ (+ 1 1) (+ 1 1))\nBYE\n       |     HELLO\nRESULT 4\nBYE\n   |
|      Basic UDP       |    \x00\x07\x28\x2B\x20\x31\x20\x31\x29       |     \x01\x00\x01\x32   |
|      Invalid UDP request      |    \x00\x03\x28\x2B\x29       |     \x01\x01\0x11Invalid request.\n   |
|      UDP nested       |    \x00\x13\x28\x2B\x20\x28\x2B\x20\x31\x20\x31\x29\x20\x28\x2B\x20\x31\x20\x31\x29\x29       |     \x01\x00\x01\x34   |

## Known limitations

- negative resulsts are forbidden
- a maximum buffer length is capped at 2048 bytes, the rest is truncated and the behaviour is undefined
- a maximum digit length is set to 10 (which is the length of maximum integer)
 

## References

- [1] [The IPK Calculator Protocol](https://git.fit.vutbr.cz/NESFIT/IPK-Projekty/src/branch/master/Project%201/Protocol.md)
- [2] [Transmission Control Protocol](https://www.ietf.org/rfc/rfc793.txt)
- [3] [Select() manual page](https://man7.org/linux/man-pages/man2/select.2.html)
- [4] [Stevens, W. Richard. “Handling Interrupted System Calls.” Unix Network Programming, 3rd ed., vol. 1, Prentice-Hall of India Private Ltd., New Delhi, 1999.](https://doc.lagout.org/programmation/unix/Unix%20Network%20Programming%20Volume%201.pdf)
- [5] [Recursive descent parsing](https://en.wikipedia.org/wiki/Recursive_descent_parser)
- [6] [User Datagram Protocol](https://www.rfc-editor.org/rfc/rfc768)
- [7] [netcat](https://en.wikipedia.org/wiki/Netcat)
