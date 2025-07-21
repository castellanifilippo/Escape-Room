# Escape Room Multiplayer - Computer Networks Project

## Overview

This is a university project. It's a multiplayer Escape Room game implemented in C using a client-server architecture. Players can register, log in, view available game servers, connect to a game, and collaborate to solve puzzles and escape the virtual room.

## Features

- **User Registration and Login:** Users can create an account or log in with existing credentials.
- **Server Discovery:** Clients can request a list of available game servers.
- **Game Server Connection:** After authentication, clients can connect to a specific game server by its ID.
- **Interactive Command-Line Interface:** The client guides users through the available commands at each stage.
- **Network Communication:** Uses TCP sockets for communication between client and server.
- **Structured Requests/Responses:** Communication is based on structured requests and responses, serialized as strings.

## File Structure

- `client.c`: Main client logic and user interface.
- `gameClient.c`, `gameClient.h`: Game logic and additional client-side features.
- `server.c`, `gameServer.c`, `gameServer.h`: Main server and game server logic.
- `files/users.txt`: Stores user credentials.
- `makefile`: Build script.
- `exec2024.sh`: Script to launch all components.
- `documentazione.pdf`: Project documentation for Uni.

## How the Client Works

### Startup

The client expects a port number as a command-line argument (used for the local client socket). It connects to the main server at `127.0.0.1:4242`.

### Authentication

Upon startup, the client displays:

```
1) login <username> <password> --> log in with your account
2) sign <username> <password>  --> create a new account
```

The client waits for a valid login or sign-up command. Credentials are sent to the server, and the response determines if the user is authenticated.

### Server Selection

Once logged in, the client displays:

```
1) list                --> list available game servers
2) connect <id>        --> connect to the chosen game server
```

- `list`: Requests and displays all available game servers.
- `connect <id>`: Requests connection details for the selected server. If successful, updates the server address and prepares to connect to the game server.

### Game Connection

After selecting a game server, the client closes the initial connection and establishes a new connection to the game server using the provided port. The game logic is then handled by `gameClient()`.

### Command Handling

The client parses user input, validates commands, and sends structured requests to the server. Responses are parsed and used to update the client state or display messages.

#### Example Commands

- `login <username> <password>`
- `sign <username> <password>`
- `list`
- `connect <id>`

Further commands for in-game actions are handled in `gameClient.c`.

## Build Instructions

Make sure you have GCC installed. Compile with:

```sh
make
```

## Running the Project

Start all components using the provided script:

```sh
sh exec2024.sh
```

Or manually:

- Main server: `./server 4242`
- Client: `./client 6000`
- Additional client: `./other 6100`

## Dependencies

- Linux system with POSIX support (sockets, semaphores, shared memory).
- GCC compiler.

## Notes

- User credentials are stored in `files/users.txt`.
- The main server port is fixed at 4242.
- For protocol details and game logic, see `documentazione.pdf`.

## Authors

University project for the Computer Networks