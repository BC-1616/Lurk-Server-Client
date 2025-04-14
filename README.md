# 🖥️ Multi-User Client-Server System (C++)

A class project focused on developing a client-server architecture in C++ to support a retro, 80s-style text-based video game. The system allows multiple users to connect and interact with the game environment in real time, simulating classic command-line gameplay.

---

## 📚 Description

This project emphasized network programming, socket communication, and the fundamentals of multiplayer game architecture in a low-level programming environment. It's designed for learning purposes and demonstrates fundamental networking concepts such as:

- Socket communication (TCP/IP)
- Multi-threaded client handling
- Message broadcasting
- Basic connection lifecycle management

---

## 🗂️ Project Structure

- `client/`: Source code and build files for the client application.
- `server/`: Source code and build files for the server application.

---

## 💬 Features
- Multiple clients can connect and communicate concurrently
- Each client is handled in a separate thread on the server
- Message broadcasting between clients via the server
- Graceful shutdown and error handling
