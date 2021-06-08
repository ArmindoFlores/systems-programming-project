# Systems Programming Assignement

## How to run
To compile the KVS library and its other binaries (localserver and authserver + demos), simply run the the command `make all`.
After that, all binaries will be under the `bin` directory, except for the library which will be under the `lib` directory (and is named `libKVS.so`).

### Running each program
Running a program with no command-line arguments will output the program's usage:

```
francisco@me:~$ bin/localserver
Usage: bin/localserver AUTH_SERVER_IP AUTH_SERVER_PORT
```

Arguments:

* localserver: AUTH_SERVER_IP AUTH_SERVER_PORT
* authserver: PORT_NUMBER
* client (demo1): GROUPID SECRET
* chat (demo2): USERNAME GROUPID SECRET

## Demos
To test the functionality of this project, we included two simple demos.

### Demo 1 (client)
The first demo simply connects to the localserver and waits for user commands.
The available commands are:
* put KEY VALUE
* get KEY
* delete KEY

The demo also automatically registers a callback associated to the key `name`, and sets it to `francisco` before prompting the user. Everytime this value is changed/deleted by any client in the group the client will output a message informing the user.

### Demo 2 (chat)
For the second demo we implemented a simple group chatting system. It is not meant to be a secure or even practical way of communicating, just a demonstration of the callback functionality of the KVS-lib.
When you run this program, everytime you type a message it will be sent to all connected clients by changing the value of a KV-pair that all clients automatically register a callback to. Thus the output should look something akin to:
```



>>> Hello
```
```
[Francisco] Hello


>>> 
```
```
[Francisco] Hello
[Miguel] Hello there

>>> 
```

__IMPORTANT NOTE:__ This demo requires ncurses and ncurses-dev for advanced command line usage.