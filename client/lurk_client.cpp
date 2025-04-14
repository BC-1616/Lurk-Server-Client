#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <vector>
#include <netdb.h> // For gethostbyname
#include <limits>
#include <iomanip> // For std::setw and std::setfill
#include <thread> // For threading
#include <mutex>  // For thread synchronization (mutex)

// Define color codes
#define RESET   "\033[0m"
#define GREEN   "\033[32m"
#define LIGHT_BLUE "\033[1;34m"

#define MESSAGE_TYPE 1
#define CHANGE_ROOM_TYPE 2
#define FIGHT_TYPE 3
#define PVP_FIGHT_TYPE 4
#define LOOT_TYPE 5
#define START_TYPE 6
#define ERROR_TYPE 7
#define ACCEPT_TYPE 8
#define ROOM_TYPE 9
#define CHARACTER_TYPE 10
#define GAME_TYPE 11
#define LEAVE_TYPE 12
#define CONNECTION_TYPE 13
#define VERSION_TYPE 14

struct Character {
    char name[32];         // Player name (31 characters + null terminator)
    uint8_t flags;         // Flags
    uint16_t attack;       // Attack value
    uint16_t defense;      // Defense value
    uint16_t regen;        // Regeneration value
    int16_t health;        // Health (signed)
    uint16_t gold;         // Gold
    uint16_t roomNumber;   // Current room number
    uint16_t descriptionLength;
    std::string description; // Player description
};

void handleReceiveMessages(int sockfd);

void handleServerMessage(int sockfd);
void handleGameMessage(int sockfd);
void handleRoomMessage(int sockfd);
void handleErrorMessage(int sockfd);
void handleAcceptMessage(int sockfd);
void handleCharacterMessage(int sockfd, Character &character);
void handleConnectionMessage(int sockfd);
void handleVersionMessage(int sockfd);

void usage(const char *progName);
void handleUserInput();
void inputCharacterData(Character& character);

void printServerMessage(const char* recipient_name, const char* sender_name, bool is_narration, const char* message_content);
void printAccept(uint8_t action_type);
void printCharacter(const std::vector<uint8_t>& message);
void printConnection(uint16_t room_number, const char* room_name, const char* room_description);

void handleSendMessage(int sockfd, uint8_t type);
void handleSendChangeRoom(int sockfd);
void handleSendPVPFight(int sockfd);
void handleSendLoot(int sockfd);


// Function to print usage
void usage(const char *progName) {
    std::cerr << "Usage: " << progName << " [hostname] [port]\n";
}

void printServerMessage(const char* recipient_name, const char* sender_name, bool is_narration, const char* message_content) {
    std::cout << "\n" LIGHT_BLUE << "Type: 1 (MESSAGE)" << RESET << std::endl;
    std::cout << "Recipient Name: " << recipient_name << std::endl;
    if(is_narration == true){
        std::cout << "Sender Name: " << sender_name << " (Narrator)" << std::endl;
    } else if(is_narration == false){
        std::cout << "Sender Name: " << sender_name << std::endl;
    }
    std::cout << "Message: " << message_content << std::endl;
}

void printAccept(uint8_t action_type){
    std::cout << "\n" LIGHT_BLUE << "Type: 8 (ACCEPT)" << RESET << std::endl;
    std::cout << "Accepted Message: " << static_cast<int>(action_type) << std::endl;
}

void printRoom(uint16_t room_number, const char* room_name, const char* room_description){
        std::cout << "\n" LIGHT_BLUE << "Type: 9 (ROOM)" << RESET << std::endl;
        std::cout << "Room Number: " << static_cast<int>(room_number) << std::endl;
        std::cout << "Room Name: " << room_name << std::endl;
        std::cout << "Room Description: " << room_description << std::endl;
}

void printCharacter(const std::vector<uint8_t>& message) {
    if (message.size() < 48) {
        std::cerr << "Invalid CHARACTER message, too short!" << std::endl;
        return;
    }

    size_t offset = 0;
    Character character;

    // Parse the player name (0-31 bytes)
    std::memcpy(character.name, &message[offset], 32);
    character.name[31] = '\0'; // Ensure null-termination
    offset += 32;

    // Parse flags (1 byte)
    character.flags = message[offset++];
    bool alive = (character.flags & 0x80) != 0;
    bool joinBattle = (character.flags & 0x40) != 0;
    bool monster = (character.flags & 0x20) != 0;
    bool started = (character.flags & 0x10) != 0;
    bool ready = (character.flags & 0x08) != 0;

    // Parse stats: attack, defense, regen, health, gold, room number (2 bytes each, total 12 bytes)
    character.attack = (message[offset] << 8) | message[offset + 1]; offset += 2;
    character.defense = (message[offset] << 8) | message[offset + 1]; offset += 2;
    character.regen = (message[offset] << 8) | message[offset + 1]; offset += 2;
    character.health = (message[offset] << 8) | message[offset + 1]; offset += 2;
    character.gold = (message[offset] << 8) | message[offset + 1]; offset += 2;
    character.roomNumber = (message[offset] << 8) | message[offset + 1]; offset += 2;

    // Parse description length (2 bytes)
    character.descriptionLength = (message[offset] << 8) | message[offset + 1]; offset += 2;

    // Parse description (variable length, based on the descriptionLength)
    if (character.descriptionLength > 0) {
        character.description = std::string(reinterpret_cast<const char*>(&message[offset]), character.descriptionLength);
    }

    // Print the parsed character data
    std::cout << "\n" LIGHT_BLUE << "Character Details: " << RESET << std::endl;
    std::cout << "Name: " << character.name << std::endl;
    std::cout << "Flags: " << std::endl;
    std::cout << "  Alive: " << (alive ? "Yes" : "No") << std::endl;
    std::cout << "  Join Battle: " << (joinBattle ? "Yes" : "No") << std::endl;
    std::cout << "  Monster: " << (monster ? "Yes" : "No") << std::endl;
    std::cout << "  Started: " << (started ? "Yes" : "No") << std::endl;
    std::cout << "  Ready: " << (ready ? "Yes" : "No") << std::endl;
    std::cout << "Attack: " << character.attack << std::endl;
    std::cout << "Defense: " << character.defense << std::endl;
    std::cout << "Regen: " << character.regen << std::endl;
    std::cout << "Health: " << character.health << std::endl;
    std::cout << "Gold: " << character.gold << std::endl;
    std::cout << "Room Number: " << character.roomNumber << std::endl;
    std::cout << "Description Length: " << character.descriptionLength << std::endl;
    std::cout << "Description: " << character.description << std::endl;
}

void printConnection(uint16_t room_number, const char* room_name, const char* room_description) {
    std::cout << "\n" LIGHT_BLUE << "Type: 13 (CONNECTION)" << RESET << std::endl;
    std::cout << "Room Number: " << static_cast<int>(room_number) << std::endl;
    std::cout << "Room Name: " << room_name << std::endl;
    std::cout << "Room Description: " << room_description << std::endl;
}

void handleReceiveMessages(int sockfd) {
    while (true) {
        //std::cout << "receive thread is starting" << std::endl;
        uint8_t firstByte;
        ssize_t bytesRead = recv(sockfd, &firstByte, sizeof(firstByte), MSG_WAITALL);

        //std::cout << "bytes were read" << std::endl;
        if (bytesRead < 0) {
            perror("recv");
        } else if (bytesRead == 0) {
            std::cerr << "Connection closed by the server." << std::endl;
            break;
        } else {
            // Handle VERSION message based on the first byte
            if (firstByte == MESSAGE_TYPE) {
                handleServerMessage(sockfd);
                //std::cout << "message type" << std::endl;
            } else if (firstByte == ERROR_TYPE){
                handleErrorMessage(sockfd);
            } else if (firstByte == ACCEPT_TYPE){
                handleAcceptMessage(sockfd);
            } else if (firstByte == ROOM_TYPE){
                handleRoomMessage(sockfd);
            } else if (firstByte == CHARACTER_TYPE){
                Character character;
                handleCharacterMessage(sockfd, character);
            } else if (firstByte == CONNECTION_TYPE) {
                handleConnectionMessage(sockfd);
            } else if (firstByte == VERSION_TYPE) {
                handleVersionMessage(sockfd);
            } else {
                std::cout << "Received unexpected message type: " << static_cast<int>(firstByte) << std::endl;
            }
        }
    }
}

// Function to read and parse the VERSION message
void handleVersionMessage(int sockfd) {
    uint8_t versionMessage[4]; // Buffer for the VERSION message (4 bytes)
    ssize_t bytesRead = recv(sockfd, versionMessage, sizeof(versionMessage), MSG_WAITALL);
    
    if (bytesRead > 0) {
        uint8_t major = versionMessage[0];
        uint8_t minor = versionMessage[1];
        uint16_t extensionSize = (versionMessage[2] << 8) | versionMessage[3];
        
        // Print formatted version information
        std::cout << "Version " << static_cast<int>(major) << "." 
                  << static_cast<int>(minor) << " with " 
                  << extensionSize << " bytes of extensions.\n" << std::endl;

        if (extensionSize > 0) {
            std::vector<char> extensions(extensionSize);
            bytesRead = recv(sockfd, extensions.data(), extensionSize, MSG_WAITALL);
            
            if (bytesRead > 0) {
                size_t offset = 0;

                while (offset < extensionSize) {
                    if (offset + 2 > extensionSize) break;
                    uint16_t extLength = (extensions[offset] << 8) | extensions[offset + 1];
                    offset += 2;

                    if (offset + extLength > extensionSize) {
                        std::cerr << "Error: Extension length exceeds available data." << std::endl;
                        break;
                    }

                    std::string extension(extensions.data() + offset, extLength);
                    std::cout << "Extension (" << extLength << " bytes): " << extension << std::endl;
                    offset += extLength;
                }
            } else {
                perror("recv extensions");
            }
        }

        // Now handle the GAME message immediately after the VERSION message
        handleGameMessage(sockfd);
    } else {
        perror("recv");
    }
}

// Function to read and parse the GAME message
void handleGameMessage(int sockfd) {
    uint8_t gameMessage[7]; // Buffer for the GAME message header (7 bytes)
    ssize_t bytesRead = recv(sockfd, gameMessage, sizeof(gameMessage), MSG_WAITALL);
    
    if (bytesRead > 0) {
        uint8_t type = gameMessage[0];
        uint16_t initialPoints = (gameMessage[1] | (gameMessage[2] << 8)); // Read initial points
        uint16_t statLimit = (gameMessage[3] | (gameMessage[4] << 8)); // Read stat limit
        uint16_t descriptionLength = (gameMessage[5] | (gameMessage[6] << 8)); // Read description length

        std::cout << "Received GAME message:" << std::endl;
        std::cout << "Type: " << static_cast<int>(type) << std::endl;
        std::cout << "Initial Points: " << initialPoints << std::endl;
        std::cout << "Stat Limit: " << statLimit << std::endl;
        std::cout << "Description Length: " << descriptionLength << " bytes" << std::endl;

        // Read game description if there is any
        if (descriptionLength > 0) {
            std::vector<char> description(descriptionLength);
            bytesRead = recv(sockfd, description.data(), descriptionLength, 0);
            if (bytesRead > 0) {
                std::cout << "Game Description: " << std::string(description.data(), descriptionLength) << std::endl;
            } else {
                perror("recv game description");
            }
        }
    } else {
        perror("recv game message");
    }
}

void handleCharacterMessage(int sockfd, Character &character) {
    uint8_t buffer[512];  // Buffer to store incoming message

    // Step 1: Receive the character message (at least 48 bytes for the basic fields)
    ssize_t bytes_received = recv(sockfd, buffer, 47, MSG_WAITALL);
    if (bytes_received <= 0) {
        std::cerr << "Failed to receive character message." << std::endl;
        return;
    }

    // Step 2: Parse the fields

    // Name (bytes 1-32, null-terminated string)
    std::memcpy(character.name, &buffer[0], 32);
    character.name[31] = '\0';  // Ensure null termination

    // Flags (byte 33, bitwise flags)
    character.flags = buffer[32];
    bool alive = character.flags & 0x80;     // Alive (highest bit)
    bool join_battle = character.flags & 0x40; // Join Battle (second highest bit)
    bool monster = character.flags & 0x20;    // Monster (third highest bit)
    bool started = character.flags & 0x10;    // Started (fourth highest bit)
    bool ready = character.flags & 0x08;      // Ready (fifth highest bit)

    // Attack (bytes 34-35, unsigned) - Little-endian byte order
    character.attack = (buffer[33]) | (buffer[34] << 8);

    // Defense (bytes 36-37, unsigned) - Little-endian byte order
    character.defense = (buffer[35]) | (buffer[36] << 8);

    // Regen (bytes 38-39, unsigned) - Little-endian byte order
    character.regen = (buffer[37]) | (buffer[38] << 8);

    // Health (bytes 40-41, signed short) - Little-endian byte order
    character.health = (buffer[39]) | (buffer[40] << 8);

    // Gold (bytes 42-43, unsigned) - Little-endian byte order
    character.gold = (buffer[41]) | (buffer[42] << 8);

    // Current Room (bytes 44-45, unsigned) - Little-endian byte order
    character.roomNumber = (buffer[43]) | (buffer[44] << 8);

    // Description Length (bytes 46-47, unsigned) - Little-endian byte order
    character.descriptionLength = (buffer[45]) | (buffer[46] << 8);

    // Step 3: Receive the character's description if present
    if (character.descriptionLength > 0) {
        bytes_received = recv(sockfd, buffer, character.descriptionLength, MSG_WAITALL);
        if (bytes_received <= 0) {
            std::cerr << "Failed to receive character description." << std::endl;
            return;
        }

        character.description.assign(reinterpret_cast<char*>(buffer), character.descriptionLength);
    } else {
        character.description.clear(); // Ensure it's cleared if there's no description
    }

    // Step 4: Output the character info (or update internal state)
    std::cout << "\n" LIGHT_BLUE << "Character Details: " << RESET << std::endl;
    std::cout << "Name: " << character.name << std::endl;
    std::cout << "Flags: " << std::endl;
    std::cout << "  Alive: " << (alive ? "Yes" : "No") << std::endl;
    std::cout << "  Join Battle: " << (join_battle ? "Yes" : "No") << std::endl;
    std::cout << "  Monster: " << (monster ? "Yes" : "No") << std::endl;
    std::cout << "  Started: " << (started ? "Yes" : "No") << std::endl;
    std::cout << "  Ready: " << (ready ? "Yes" : "No") << std::endl;
    std::cout << "Attack: " << character.attack << std::endl;
    std::cout << "Defense: " << character.defense << std::endl;
    std::cout << "Regen: " << character.regen << std::endl;
    std::cout << "Health: " << character.health << std::endl;
    std::cout << "Gold: " << character.gold << std::endl;
    std::cout << "Room Number: " << character.roomNumber << std::endl;
    std::cout << "Description Length: " << character.descriptionLength << std::endl;
    std::cout << "Description: " << character.description << std::endl;
}


void handleAcceptMessage(int sockfd) {
    uint8_t action_type;
    recv(sockfd, &action_type, sizeof(action_type), MSG_WAITALL);
    printAccept(action_type);
}

void handleRoomMessage(int sockfd ){
uint8_t buffer[256];  // Buffer to store incoming message (size should be enough for room data)

    // Receive the room data message from the server
    ssize_t bytes_received = recv(sockfd, buffer, 2, 0);  // First, read the room number (2 bytes)
    
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            std::cerr << "Connection closed by the server." << std::endl;
        } else {
            perror("recv failed");
        }
        return;
    }

    // Room number: bytes 1-2
    uint16_t room_number = *(uint16_t*)&buffer[0];

    // Now, let's read the room name (32 bytes)
    bytes_received = recv(sockfd, &buffer[2], 32, 0);  // Read the room name (32 bytes)
    if (bytes_received <= 0) {
        std::cerr << "Failed to receive room name." << std::endl;
        return;

    }

    // Null-terminate the room name and print it
    buffer[34] = '\0';  // Make sure the string is null-terminated
    const char* room_name = reinterpret_cast<char*>(&buffer[2]);    

    // Room description length: bytes 35-36 (2 bytes)
    bytes_received = recv(sockfd, &buffer[34], 2, 0);  // Read description length (2 bytes)
    if (bytes_received <= 0) {
        std::cerr << "Failed to receive room description length." << std::endl;
        return;
    }

    uint16_t description_length = *(uint16_t*)&buffer[34];
    //std::cout << "Room description length: " << description_length << " bytes." << std::endl;

    // Now, let's read the room description
    if (description_length > 0) {
        bytes_received = recv(sockfd, &buffer[36], description_length, 0);  // Read the description
        if (bytes_received <= 0) {
            std::cerr << "Failed to receive room description." << std::endl;
            return;
        }

        // Null-terminate the room description and print it
        buffer[36 + description_length] = '\0';  // Ensure null-termination
        const char* room_description = reinterpret_cast<char*>(&buffer[36]);
        printRoom(room_number, room_name, room_description);
    } else {
        std::cout << "No room description available." << std::endl;
    }
}

void handleConnectionMessage(int sockfd) {
    uint8_t buffer[256];  // Buffer for receiving data (size should be enough for the connection data)

    // Step 1: Receive the room number (2 bytes)
    ssize_t bytes_received = recv(sockfd, buffer, 2, 0);  // First, read the room number (2 bytes)
    
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            std::cerr << "Connection closed by the server." << std::endl;
        } else {
            perror("recv failed");
        }
        return;
    }

    uint16_t room_number = *(uint16_t*)&buffer[0];  // Room number is stored in the first 2 bytes

    // Step 2: Receive the room name (32 bytes)
    bytes_received = recv(sockfd, &buffer[2], 32, MSG_WAITALL);  // Read the room name (32 bytes)
    if (bytes_received <= 0) {
        std::cerr << "Failed to receive room name." << std::endl;
        return;
    }

    // Null-terminate the room name to ensure it's a valid C-string
    buffer[34] = '\0';  // Ensure null-termination after 32 bytes
    const char* room_name = reinterpret_cast<char*>(&buffer[2]);

    // Step 3: Receive the room description length (2 bytes)
    bytes_received = recv(sockfd, &buffer[34], 2, 0);  // Read description length (2 bytes)
    if (bytes_received <= 0) {
        std::cerr << "Failed to receive room description length." << std::endl;
        return;
    }

    uint16_t description_length = *(uint16_t*)&buffer[34];  // Description length is stored in the next 2 bytes
    //std::cout << "Room description length: " << description_length << " bytes." << std::endl;

    // Step 4: Receive the room description (if any)
    const char* room_description = nullptr;
    if (description_length > 0) {
        bytes_received = recv(sockfd, &buffer[36], description_length, 0);  // Read the room description
        if (bytes_received <= 0) {
            std::cerr << "Failed to receive room description." << std::endl;
            return;
        }

        // Null-terminate the room description and print it
        buffer[36 + description_length] = '\0';  // Ensure null-termination
        room_description = reinterpret_cast<char*>(&buffer[36]);
    } else {
        std::cout << "No room description available." << std::endl;
    }

    // Optionally, call a function to print or process the connection message further
    printConnection(room_number, room_name, room_description);
}

void handleServerMessage(int sockfd) {
    uint8_t buffer[1024];  // Buffer to store the incoming server message (size should be enough for the data)

    // Step 1: Receive the message length (2 bytes)
    ssize_t bytes_received = recv(sockfd, buffer, 2, MSG_WAITALL);
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            std::cerr << "Connection closed by the server." << std::endl;
        } else {
            perror("recv failed");
        }
        return;
    }

    uint16_t message_length = *(uint16_t*)&buffer[0];  // Message length is stored in the first 2 bytes

    // Step 2: Receive the recipient name (32 bytes)
    bytes_received = recv(sockfd, &buffer[2], 32, MSG_WAITALL);  // Read recipient name (32 bytes)
    if (bytes_received <= 0) {
        std::cerr << "Failed to receive recipient name." << std::endl;
        return;
    }

    // Null-terminate the recipient name
    buffer[34] = '\0';
    const char* recipient_name = reinterpret_cast<char*>(&buffer[2]);

    // Step 3: Receive the sender name (30 bytes)
    bytes_received = recv(sockfd, &buffer[34], 30, MSG_WAITALL);  // Read sender name (30 bytes)
    if (bytes_received <= 0) {
        std::cerr << "Failed to receive sender name." << std::endl;
        return;
    }

    // Null-terminate the sender name
    buffer[64] = '\0';
    const char* sender_name = reinterpret_cast<char*>(&buffer[34]);

    // Step 4: Check for narration marker (bytes 65 and 66)
    bytes_received = recv(sockfd, &buffer[64], 2, MSG_WAITALL);  // Read sender name (30 bytes)
    if (bytes_received <= 0) {
        std::cerr << "Failed to receive sender name." << std::endl;
        return;
    }
    uint8_t check = 1;
    bool is_narration = (buffer[64] == 0 && buffer[65] == check);

    // Step 5: Receive the message (based on message length)
    //uint16_t message_content_length = message_length - (34 + 30 + 2);  // Subtract the header length
    if (message_length > 0) {
        bytes_received = recv(sockfd, &buffer[66], message_length, MSG_WAITALL);  // Read the message content
        if (bytes_received <= 0) {
            std::cerr << "Failed to receive message content." << std::endl;
            return;
        }

        // Null-terminate the message content and print it
        buffer[66 + message_length] = '\0';  // Ensure null-termination
        const char* message_content = reinterpret_cast<char*>(&buffer[66]);
        printServerMessage(recipient_name, sender_name, is_narration, message_content);
    } else {
        std::cout << "No message content." << std::endl;
    }

}

void handleErrorMessage(int sockfd) {
    uint8_t buffer[512];  // Buffer to store incoming message


    // Step 2: Receive the error code (1 byte)
    uint8_t error_code;
    ssize_t bytes_received = recv(sockfd, &error_code, 1, MSG_WAITALL);
    if (bytes_received <= 0) {
        std::cerr << "Failed to receive error code." << std::endl;
        return;
    }

    // Step 3: Receive the error message length (2 bytes, little-endian)
    uint16_t message_length;
    bytes_received = recv(sockfd, &buffer[0], 2, MSG_WAITALL);
    if (bytes_received <= 0) {
        std::cerr << "Failed to receive error message length." << std::endl;
        return;
    }
    message_length = buffer[0] | (buffer[1] << 8 );  // Little-endian to uint16_t

    // Step 4: Receive the actual error message (of the specified length)
    bytes_received = recv(sockfd, buffer, message_length, MSG_WAITALL);
    if (bytes_received <= 0) {
        std::cerr << "Failed to receive error message." << std::endl;
        return;
    }

    // Null-terminate the error message
    buffer[message_length] = '\0';

    // Step 5: Print the error based on the error code
    std::cout << "\n" LIGHT_BLUE << "Type: 7 (ERROR)" << RESET << std::endl;
    std::cout << "Error Code: " << static_cast<int>(error_code) << std::endl;
    std::cout << "Message Length: " << static_cast<int>(message_length) << std::endl;
    std::cout << "Message: " << reinterpret_cast<char*>(buffer) << std::endl;
}


void handleSendMessage(int sockfd, uint8_t type) {
    uint16_t messageLength;
    char recName[32];
    char sendName[32];
    std::string message;

    std::cout << LIGHT_BLUE << "Message length (1-255): " << RESET;
    std::cin >> messageLength;

    // Input validation for messageLength
    if (messageLength < 1 || messageLength > 255) {
        std::cerr << "Invalid message length. Must be between 1 and 255." << std::endl;
        return; // Exit if invalid
    }

    std::cout << LIGHT_BLUE << "Receiver name (max 32 chars): " << RESET;
    std::cin.ignore(); // Clear input buffer before getline
    std::cin.getline(recName, sizeof(recName)); // Read receiver name

    std::cout << LIGHT_BLUE << "Sender name (max 32 chars): " << RESET;
    std::cin.getline(sendName, sizeof(sendName)); // Read sender name

    std::cout << LIGHT_BLUE << "Message: " << RESET;
    //std::cin.ignore(); // Clear input buffer before getline
    std::getline(std::cin, message);

    // Prepare to send
    size_t totalSize = sizeof(type) + sizeof(messageLength) + sizeof(recName) + sizeof(sendName) + messageLength; 
    std::vector<uint8_t> buffer(totalSize);
    size_t offset = 0;

    // Pack data into the buffer
    std::memcpy(buffer.data() + offset, &type, sizeof(type));
    offset += sizeof(type);
    std::memcpy(buffer.data() + offset, &messageLength, sizeof(messageLength));
    offset += sizeof(messageLength);

    // Copy receiver name (32 bytes)
    std::memcpy(buffer.data() + offset, recName, sizeof(recName)); // Will fill up to 32 bytes
    offset += sizeof(recName);

    // Copy sender name (32 bytes)
    std::memcpy(buffer.data() + offset, sendName, sizeof(sendName)); // Will fill up to 32 bytes
    offset += sizeof(sendName);

    // Copy message
    std::memcpy(buffer.data() + offset, message.data(), message.size());

    //std::cout << message << std::endl;

    // Send the entire message
    ssize_t bytesSent = send(sockfd, buffer.data(), buffer.size(), 0);
    if (bytesSent < 0) {
        perror("send");
    } else {
        std::cout << "Sent to server: " << static_cast<int>(type) << std::endl;
    }
}

void handleSendCharacter(int sockfd, const Character& character) {
    // Use the descriptionLength from the character struct
    uint16_t descriptionLength = character.descriptionLength; // Use the provided length
    size_t messageSize = 48 + descriptionLength; // 48 bytes for fixed fields + description length

    // Allocate a buffer for the message
    std::vector<uint8_t> message(messageSize);
    size_t offset = 0;

    // Pack the data into the buffer
    uint8_t type = 10; // Message type for CHARACTER
    std::memcpy(message.data() + offset, &type, sizeof(type));
    offset += sizeof(type);

    // Copy name
    std::memcpy(message.data() + offset, character.name, sizeof(character.name));
    offset += sizeof(character.name);

    // Copy flags
    message[offset++] = character.flags;

    // Copy attack, defense, regen, health, gold, roomNumber
    std::memcpy(message.data() + offset, &character.attack, sizeof(character.attack));
    offset += sizeof(character.attack);
    std::memcpy(message.data() + offset, &character.defense, sizeof(character.defense));
    offset += sizeof(character.defense);
    std::memcpy(message.data() + offset, &character.regen, sizeof(character.regen));
    offset += sizeof(character.regen);
    std::memcpy(message.data() + offset, &character.health, sizeof(character.health));
    offset += sizeof(character.health);
    std::memcpy(message.data() + offset, &character.gold, sizeof(character.gold));
    offset += sizeof(character.gold);
    std::memcpy(message.data() + offset, &character.roomNumber, sizeof(character.roomNumber));
    offset += sizeof(character.roomNumber);

    // Copy description length and description
    std::memcpy(message.data() + offset, &descriptionLength, sizeof(descriptionLength));
    offset += sizeof(descriptionLength);
    std::memcpy(message.data() + offset, character.description.data(), descriptionLength);

    // Send the message
    ssize_t bytesSent = send(sockfd, message.data(), message.size(), 0);
    if (bytesSent < 0) {
        perror("send CHARACTER");
    } else {
        std::cout << "Sent CHARACTER message with name: " << character.name << std::endl;
    }
}

void handleSendChangeRoom(int sockfd) {
    uint16_t roomNumber;  // Room number to change to
    uint8_t type = 2;     // Protocol type for changing room is 2

    std::cout << LIGHT_BLUE << "Enter room number to change to (1-65535): " << RESET;
    std::cin >> roomNumber;

    // Input validation for room number
    if (roomNumber < 1 || roomNumber > 65535) {
        std::cerr << "Invalid room number. Must be between 1 and 65535." << std::endl;
        return; // Exit if invalid
    }

    // Prepare to send
    size_t totalSize = sizeof(type) + sizeof(roomNumber); // Type (1 byte) + Room number (2 bytes)
    std::vector<uint8_t> buffer(totalSize);
    size_t offset = 0;

    // Pack data into the buffer
    std::memcpy(buffer.data() + offset, &type, sizeof(type));  // Copy the type byte (2 for change room)
    offset += sizeof(type);
    std::memcpy(buffer.data() + offset, &roomNumber, sizeof(roomNumber));  // Copy the room number (2 bytes)
    
    // Send the entire message
    ssize_t bytesSent = send(sockfd, buffer.data(), buffer.size(), 0);
    if (bytesSent < 0) {
        perror("send");
    } else {
        std::cout << "Sent room change request to server. Room number: " << roomNumber << std::endl;
    }
}

void handleSendPVPFight(int sockfd) {
    char targetName[32] = {0}; // Initialize the target name array to 0 (32 bytes)

    uint8_t type = 4;    // Protocol type for fight initiation is 4

    std::cout << LIGHT_BLUE << "Enter target player's name (max 32 chars): " << RESET;
    
    // Read the target player's name (max 32 chars)
    std::cin.getline(targetName, sizeof(targetName)); // Read input into targetName

    // Ensure the name is always 32 bytes, padding with nulls if necessary
    // Since targetName is already initialized to 0, this step is technically not needed unless input exceeds 32 characters.
    if (std::cin.gcount() >= sizeof(targetName)) {
        targetName[31] = '\0';  // Ensure it does not overflow beyond 32 bytes
    }

    // Prepare to send the message
    size_t totalSize = sizeof(type) + sizeof(targetName); // Type (1 byte) + Target name (32 bytes)
    std::vector<uint8_t> buffer(totalSize);
    size_t offset = 0;

    // Pack data into the buffer
    std::memcpy(buffer.data() + offset, &type, sizeof(type));  // Copy the type byte (4 for fight initiation)
    offset += sizeof(type);
    std::memcpy(buffer.data() + offset, targetName, sizeof(targetName));  // Copy the target name (up to 32 bytes)

    // Send the entire message
    ssize_t bytesSent = send(sockfd, buffer.data(), buffer.size(), 0);
    if (bytesSent < 0) {
        perror("send");
    } else {
        std::cout << "Sent PVP fight initiation request to server. Target: " << targetName << std::endl;
    }
}

void handleSendLoot(int sockfd) {
    char targetName[32] = {0}; // Initialize the target name array to 0 (32 bytes)

    uint8_t type = 5;    // Protocol type for fight initiation is 4

    std::cout << LIGHT_BLUE << "Enter target player's name (max 32 chars): " << RESET;
    
    // Read the target player's name (max 32 chars)
    std::cin.getline(targetName, sizeof(targetName)); // Read input into targetName

    // Ensure the name is always 32 bytes, padding with nulls if necessary
    // Since targetName is already initialized to 0, this step is technically not needed unless input exceeds 32 characters.
    if (std::cin.gcount() >= sizeof(targetName)) {
        targetName[31] = '\0';  // Ensure it does not overflow beyond 32 bytes
    }

    // Prepare to send the message
    size_t totalSize = sizeof(type) + sizeof(targetName); // Type (1 byte) + Target name (32 bytes)
    std::vector<uint8_t> buffer(totalSize);
    size_t offset = 0;

    // Pack data into the buffer
    std::memcpy(buffer.data() + offset, &type, sizeof(type));  // Copy the type byte (4 for fight initiation)
    offset += sizeof(type);
    std::memcpy(buffer.data() + offset, targetName, sizeof(targetName));  // Copy the target name (up to 32 bytes)

    // Send the entire message
    ssize_t bytesSent = send(sockfd, buffer.data(), buffer.size(), 0);
    if (bytesSent < 0) {
        perror("send");
    } else {
        std::cout << "Sent loot request to server. Target: " << targetName << std::endl;
    }
}

void inputCharacterData(Character& character) {
    std::cout << LIGHT_BLUE << "Enter Player Name (max 32 characters): " << RESET;
    std::cin.getline(character.name, sizeof(character.name));

    // Input Flags
    int flagsInput;
    std::cout << LIGHT_BLUE << "Enter Flags (0-31): " << RESET;
    std::cin >> flagsInput;
    character.flags = static_cast<uint8_t>(flagsInput); // Cast to uint8_t

    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Clear the input buffer

    // Input Attack
    std::cout << LIGHT_BLUE << "Enter Attack: " << RESET;
    std::cin >> character.attack;

    // Input Defense
    std::cout << LIGHT_BLUE << "Enter Defense: " << RESET;
    std::cin >> character.defense;

    // Input Regen
    std::cout << LIGHT_BLUE << "Enter Regen: " << RESET;
    std::cin >> character.regen;

    // Input Health
    std::cout << LIGHT_BLUE << "Enter Health: " << RESET;
    std::cin >> character.health;

    // Input Gold
    std::cout << LIGHT_BLUE << "Enter Gold: " << RESET;
    std::cin >> character.gold;

    // Input Room Number
    std::cout << LIGHT_BLUE << "Enter Current Room Number: " << RESET;
    std::cin >> character.roomNumber;

    // Input Description Length
    std::cout << LIGHT_BLUE << "Enter Description Length: " << RESET;
    std::cin >> character.descriptionLength;

    // Input Description
    std::cout << LIGHT_BLUE << "Enter Player Description: " << RESET;
    std::cin.ignore(); // Clear the input buffer before reading description
    std::getline(std::cin, character.description);

    // Make sure inputs are valid (if needed)
    // Example: Validate character health and gold are non-negative
    if (character.health < 0) character.health = 0;
    if (character.gold < 0) character.gold = 0;

    std::cout << "Character data input complete." << std::endl;
}

// Function for handling the user input
void handleUserInput(int sockfd) {
    while (true) {
        sleep(1);
        int type;  // Read as int first
        std::cout << LIGHT_BLUE << "\nPlease enter a type: " << RESET;

        std::cin >> type;

        // Check for input errors
        if (std::cin.fail()) {
            std::cerr << "Invalid input. Please enter a valid integer." << std::endl;
            std::cin.clear(); // Clear the error flags
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Ignore the rest of the line
            continue; // Skip to the next iteration
        }

        // Clear the newline character from the input buffer
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        // Convert to uint8_t
        uint8_t typeByte = static_cast<uint8_t>(type);

        // Send the integer to the server
        if (typeByte == MESSAGE_TYPE) {
            handleSendMessage(sockfd, typeByte);
        } else if (typeByte == CHANGE_ROOM_TYPE){
            handleSendChangeRoom(sockfd);
        } else if (typeByte == FIGHT_TYPE) {
            uint8_t valueToSend = FIGHT_TYPE;
            ssize_t bytesSent = send(sockfd, &valueToSend, sizeof(valueToSend), 0);
            
            if (bytesSent < 0) {
                perror("send");
            } else {
                std::cout << "Sent value: " << static_cast<int>(valueToSend) << std::endl;
            }
        } else if(typeByte == PVP_FIGHT_TYPE){
            handleSendPVPFight(sockfd);
        } else if (typeByte == LOOT_TYPE){
            handleSendLoot(sockfd);
        } else if (typeByte == START_TYPE) {
            uint8_t valueToSend = START_TYPE;
            ssize_t bytesSent = send(sockfd, &valueToSend, sizeof(valueToSend), 0);
            
            if (bytesSent < 0) {
                perror("send");
            } else {
                std::cout << "Sent value: " << static_cast<int>(valueToSend) << std::endl;
            }
        } else if (typeByte == ERROR_TYPE) {
            std::cerr << "Invalid choice, try again!" << std::endl;
        } else if (typeByte == ACCEPT_TYPE) {
            std::cerr << "Invalid choice, try again!" << std::endl;
        } else if (typeByte == ROOM_TYPE) {
            std::cerr << "Invalid choice, try again!" << std::endl;
        } else if (typeByte == CHARACTER_TYPE) {
            Character character;
            inputCharacterData(character);
            handleSendCharacter(sockfd, character);
        } else if (typeByte == GAME_TYPE) {
            std::cerr << "Invalid choice, try again!" << std::endl;
        } else if (typeByte == LEAVE_TYPE) {
            uint8_t valueToSend = LEAVE_TYPE;
            ssize_t bytesSent = send(sockfd, &valueToSend, sizeof(valueToSend), 0);
            
            if (bytesSent < 0) {
                perror("send");
            } else {
                std::cout << "Sent value: " << static_cast<int>(valueToSend) << std::endl;
            }
            break;
        } else {
            std::cerr << "Unknown type." << std::endl;
        }
    }
}


int main(int argc, char* argv[]) {
    if (argc != 3) {
        usage(argv[0]);
        return 1;
    }

    const char* host = argv[1];
    const char* port = argv[2];

    // Get server address and port
    struct addrinfo hints{}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int status = getaddrinfo(host, port, &hints, &res);
    if (status != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(status) << std::endl;
        return 1;
    }

    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        perror("socket");
        return 1;
    }

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        perror("connect");
        return 1;
    }

    freeaddrinfo(res);

    // Start a thread to handle receiving messages
    std::thread receiverThread(handleReceiveMessages, sockfd);

    // Handle user input in the main thread
    handleUserInput(sockfd);

    receiverThread.join(); // Wait for the receiver thread to finish

    close(sockfd);
    return 0;
}