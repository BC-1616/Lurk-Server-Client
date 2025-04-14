# 🖥️ Multi-User Client-Server System (C++)

A fully functional multi-user client-server application written in C++. This system enables real-time communication between multiple clients through a centralized server, using socket programming and multi-threading.

---

## 📚 Description

This project simulates a networked environment where multiple clients can connect to a server, send and receive messages, and interact concurrently. It's designed for learning purposes and demonstrates fundamental networking concepts such as:

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
