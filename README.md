# 🎮 Game Hub — Multiplayer TCP Game Server

A multiplayer game server written in **C**, built with raw **POSIX sockets** and **pthreads**. Clients connect over TCP, get matched with another waiting player, and play a real-time game of **Tic-Tac-Toe** or **Rock-Paper-Scissors (Best of 3)** — all through a simple terminal interface.

No external libraries, no frameworks — just sockets, threads, and a matchmaking queue.

---

## ✨ Features

- **Two game modes**: Tic-Tac-Toe and Rock-Paper-Scissors (best of 3)
- **Automatic matchmaking** — players are queued per game type and paired as soon as an opponent is available
- **Concurrent games** — each match runs on its own thread, so multiple games can run at the same time
- **Play again** support — winners/losers can queue up for a rematch without reconnecting
- **Graceful disconnect handling** — if a player drops mid-game, the opponent is notified instead of hanging
- **Simple TCP client** with a dedicated background thread for receiving server messages, so you can type and receive at the same time

---

## 🏗️ Architecture

```
┌──────────┐        TCP        ┌──────────────────┐
│  Client  │ <----------------> │   Game Hub Server │
└──────────┘                    └──────────────────┘
                                        │
                        ┌───────────────┼────────────────┐
                        │                                │
                 Matchmaking queue                 Matchmaking queue
                  (Tic-Tac-Toe)                  (Rock-Paper-Scissors)
                        │                                │
                        └───────────────┬────────────────┘
                                        │
                              Game Session Thread
                         (handles moves, turns, results)
```

- **`server_final.c`** — accepts incoming connections, spawns a thread per client, handles matchmaking queues (protected by a mutex), and runs each active game session on its own thread.
- **`client_final.c`** — connects to the server, spawns a background thread to continuously print incoming server messages, while the main thread reads and sends user input.

---

## 📋 Requirements

- A POSIX-compliant OS (Linux/macOS/WSL)
- `gcc` (or any C compiler with `pthread` support)

---

## 🚀 Getting Started

### 1. Clone the repository
```bash
git clone https://github.com/AdityaDRathod/mulitplayer-games-with-sockets.git
cd <your-repo>
```

### 2. Compile the server
```bash
gcc server_final.c -o server -lpthread
```

### 3. Compile the client
```bash
gcc client_final.c -o client -lpthread
```

### 4. Run the server
```bash
./server
```
The server starts listening on **port 9000** by default.

### 5. Run the client (in one or more separate terminals)
```bash
./client
```
By default the client connects to `127.0.0.1:9000`. You can override the IP and port:
```bash
./client <server_ip> <port>
```

You'll need **at least two client instances** running to start a match (one gets queued and waits until an opponent picks the same game type).

---

## 🕹️ How to Play

1. Launch the server, then connect two or more clients.
2. Each client is prompted to enter a name and choose a game:
   ```
   === GAME MENU ===
   1. Tic-Tac-Toe
   2. Rock-Paper-Scissors

   Enter choice (1 or 2):
   ```
3. Once two players pick the same game, they're matched automatically and the match begins.
4. **Tic-Tac-Toe**: players take turns entering a cell number (1–9) corresponding to the board position.
5. **Rock-Paper-Scissors**: both players type `rock`, `paper`, or `scissors` each round — best of 3 wins.
6. After the game ends, both players are asked **"Play again? (yes/no)"** — if both agree, a new round starts instantly.

---

## 📁 Project Structure

```
.
├── server_final.c   # Multithreaded game server (matchmaking + game logic)
├── client_final.c   # Terminal client (send/receive over TCP)
└── README.md
```

---

## ⚠️ Known Limitations

- Fixed-size queues/client arrays (`MAX_CLIENTS = 100`) — not dynamically resizable
- No authentication or encryption — intended for local/LAN use and learning purposes, not production deployment
- Player names aren't validated for uniqueness or length beyond truncation
- No persistent storage — scores and sessions are in-memory only and reset when the server restarts

---

## 🛠️ Possible Improvements

- [ ] Add more games (Connect Four, Hangman, etc.)
- [ ] Add spectator mode
- [ ] Persist match history / leaderboard to a file or database
- [ ] Add reconnect support after accidental disconnect
- [ ] Configurable port via command-line argument on the server side

---

## 📄 License

This project is open source and available under the [MIT License](LICENSE).

---

## 🙌 Acknowledgements

Built as a hands-on project to learn socket programming, multithreading, and concurrent server design in C.
