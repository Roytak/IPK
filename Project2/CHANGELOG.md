### Implemented functionality

- TCP communication
- UDP communication
- Multiple clients support


### Known limitations

- Only \*nix like systems are supported
- a lot of clients in the TCP mode might break the server's reaction to C-c
- negative resulsts are forbidden
- a maximum buffer length is capped at 2048 bytes, the rest is truncated and the behaviour is undefined
- a maximum digit length is set to 10 (which is the length of maximum integer)
