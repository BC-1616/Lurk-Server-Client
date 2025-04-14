#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>

#define FLAG_ALIVE       0x01 // 00000001
#define FLAG_JOIN_BATTLE 0x02 // 00000010
#define FLAG_MONSTER     0x04 // 00000100
#define FLAG_STARTED (1 << 4)
#define FLAG_READY       0x10 // 00010000

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

#define DESCRIPTION "In the heart of the kingdom of Eldoria lies the Whispering Woods, a vast forest shrouded in mist and mystery. Legends say that the trees here can speak, sharing secrets with those brave enough to listen. However, the forest has grown dark in recent years, and strange creatures roam its paths, turning what was once a sanctuary into a land of peril."
#define START_MESSAGE "You are an adventurer drawn to the Whispering Woods by tales of forgotten treasures and ancient magic. As you step beneath the canopy, the air grows thick with anticipation. The trees rustle as if whispering warnings, and shadows dance at the corners of your vision. You carry your trusty sword and shield, ready to face whatever challenges await."
#define BOSS_MESSAGE "All three bosses have been defeated, the Heart of the Woods has opened up!"


#define MAX_CONNECTIONS 100
#define INITIAL_POINTS_MAX 1000
#define STAT_LIMIT_MAX 1000
#define NAME_LENGTH 32
#define MAX_DESCRIPTION_LENGTH 1024
#define MAX_PLAYERS 100
#define MAX_MONSTERS 100
#define MAX_ROOMS 14
#define MAX_NAME_LENGTH 32
#define MAX_SENDER_NAME_LENGTH 30


typedef struct {
    int client_fd;
    bool final_message_sent = false;
    bool end_message_sent = false;
} ThreadArgs;

ThreadArgs *thread_args[MAX_CONNECTIONS];
int thread_count = 0;
pthread_mutex_t thread_count_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    int alive;      // 1 for alive, 0 for not
    int join_battle; // 1 for join battles, 0 for do not
    int monster;    // 1 for monster, 0 for player
    int started;    // 1 for started, 0 for not
    int ready;      // 1 for ready, 0 for not
    uint8_t type;
    char name[NAME_LENGTH + 1];
    uint8_t flags;
    uint16_t attack;
    uint16_t defense;
    uint16_t regen;
    int16_t health;
    uint16_t gold;
    uint16_t room_number;
    uint16_t description_length;
    char *description;
    size_t description_received;
    size_t total_received;
    int client_fd;
} CharacterState;

CharacterState global_players[MAX_PLAYERS]; // Global array to hold character states
size_t player_count = 0; // Number of active players
pthread_mutex_t player_mutex = PTHREAD_MUTEX_INITIALIZER;
CharacterState monsters[MAX_MONSTERS];
size_t num_monsters = 0;

typedef struct Room {
    uint16_t room_number;
    char *name;
    char *description;
    struct Room *connections[4]; // Array to hold pointers to connected rooms
    size_t num_connections;

    // Arrays to hold pointers to players and monsters in the room
    CharacterState *players[MAX_PLAYERS];
    size_t num_players;

    CharacterState *monsters[MAX_MONSTERS];
    size_t num_monsters;
} Room;

Room rooms[MAX_ROOMS];


// Function prototypes
char* allocate_string(const char* str);
int is_alive(CharacterState *character);
int is_started(CharacterState *character);
int add_player_to_room(Room *room, CharacterState *player);
void calculate_damage(CharacterState *attacker, CharacterState *defender);
void handle_character(ThreadArgs *args);
void handle_change_room(ThreadArgs *args, uint16_t requested_room_number);
void handle_client_disconnection(int client_fd);
void handle_fight(CharacterState *player, CharacterState *monster);
void handle_loot(ThreadArgs *args);
void initialize_final_room();
void initialize_rooms();
void notify_players_in_room(CharacterState *player, CharacterState *monster);
void populate_final_room();
void populate_rooms_with_monsters();
void receive_message(int sender_fd, char *buffer, size_t buffer_size);
void send_accept_message(int client_fd, uint8_t action_type);
void send_character_message(int client_fd, CharacterState *character_state, const char *player_name, const char *description);
void send_connection_message(int client_fd, Room *room);
void send_error_message(int client_fd, uint8_t error_code, const char *error_message);
void send_game_message(int client_fd);
void send_message(const char *recipient_name, const char *sender_name, const char *message, size_t message_length, uint8_t narration_marker);
void send_room_message(int client_fd, Room *room, CharacterState *character_state);
void send_version_message(int client_fd);
CharacterState find_player_by_fd(int client_fd);
CharacterState* find_pp_by_fd(int client_fd);
CharacterState *get_monster_in_room(int room_number, int monster_index);
Room* find_room_by_character(CharacterState *character);

const char* start_message = "You are an adventurer drawn to the Whispering Woods by tales of forgotten treasures and ancient magic. As you step beneath the canopy, the air grows thick with anticipation. The trees rustle as if whispering warnings, and shadows dance at the corners of your vision. You carry your trusty sword and shield, ready to face whatever challenges await.";
const char* boss_message = "All three bosses have been defeated, the Heart of the Woods has opened up!";



// Function to handle each client connection in a separate thread
void *client_handler(void *args) {
    printf("Starting handler\n");
    ThreadArgs *thread_args = (ThreadArgs *)args;
    char buffer[1024];

    uint8_t player_choice;
    int try_again = 0;
    while (try_again < 5) {
        player_choice = 0;
        ssize_t recv_result = recv(thread_args->client_fd, &player_choice, sizeof(player_choice), 0);
        printf("%d, %d\n", thread_args->client_fd, player_choice);
        if (recv_result <= 0) {
            perror("recv failed");
            handle_client_disconnection(thread_args->client_fd);
            break; // Exit loop on error or disconnection
        }

        // Process player choice
        if (player_choice == CHARACTER_TYPE) {
            // Call handle_character and then break
            printf("handle_character is being run\n");
            handle_character(thread_args);
            break;
        } else if (player_choice == MESSAGE_TYPE){
            // Send error because client has not created player yet
            char randbuffer[1024];
            const char* message = "Not ready to perform actions yet";
            recv(thread_args->client_fd, &randbuffer, 66, MSG_WAITALL);
            recv(thread_args->client_fd, &randbuffer, sizeof(randbuffer), 0);
            send_error_message(thread_args->client_fd, 5, message);
            try_again++;
        } else if (player_choice == CHANGE_ROOM_TYPE){
            // Send error because client has not created player yet
            char randbuffer[1024];
            const char* message = "Not ready to perform actions yet";
            recv(thread_args->client_fd, &randbuffer, 2, MSG_WAITALL);
            send_error_message(thread_args->client_fd, 5, message);
            try_again++;
        } else if (player_choice == FIGHT_TYPE){
            // Send error because client has not created player yet
            const char* message = "Not ready to perform actions yet";
            send_error_message(thread_args->client_fd, 5, message);
            try_again++;
        } else if (player_choice == PVP_FIGHT_TYPE){
            // Send error because client has not created player yet
            char randbuffer[1024];
            const char* message = "Not ready to perform actions yet";
            recv(thread_args->client_fd, &randbuffer, 32, MSG_WAITALL);
            send_error_message(thread_args->client_fd, 5, message);
            try_again++;
        } else if (player_choice == LOOT_TYPE){
            // Send error because client has not created player yet
            char randbuffer[1024];
            const char* message = "Not ready to perform actions yet";
            recv(thread_args->client_fd, &randbuffer, 32, MSG_WAITALL);
            send_error_message(thread_args->client_fd, 5, message);
            try_again++;
        } else if (player_choice == START_TYPE){
            // Send error because client has not created player yet
            const char* message = "Not ready to perform actions yet";
            send_error_message(thread_args->client_fd, 5, message);
            try_again++;
        } else if (player_choice == ERROR_TYPE){
            // Booting connection because client not speaking proper LURK
            const char* message = "Booting connection due to invalid choice.";
            send_error_message(thread_args->client_fd, 0, message);
            break;
        } else if (player_choice == ACCEPT_TYPE){
            // Booting connection because client not speaking proper LURK
            const char* message = "Booting connection due to invalid choice.";
            send_error_message(thread_args->client_fd, 0, message);
            break;
        } else if (player_choice == ROOM_TYPE){
            // Booting connection because client not speaking proper LURK
            const char* message = "Booting connection due to invalid choice.";
            send_error_message(thread_args->client_fd, 0, message);
            break;
        } else if (player_choice == GAME_TYPE){
            // Booting connection because client not speaking proper LURK
            const char* message = "Booting connection due to invalid choice.";
            send_error_message(thread_args->client_fd, 0, message);
            break;
        } else if (player_choice == LEAVE_TYPE){
            // Handle client disconnection then break loop
            handle_client_disconnection(thread_args->client_fd);
            break;
        } else if (player_choice == CONNECTION_TYPE){
            // Booting connection because client not speaking proper LURK
            const char* message = "Booting connection due to invalid choice.";
            send_error_message(thread_args->client_fd, 0, message);
            break;
        } else if (player_choice > 13) {
            // Booting connection because client not speaking proper LURK
            const char* message = "Booting connection due to invalid choice.";
            handle_client_disconnection(thread_args->client_fd);
            break;
        } else if (player_choice == 0) {
            // Read must not have properly worked, send back to top of loop
            continue;
        }
    }
    if(try_again >= 5){
        // Client failed to send a character type in 5 attempts, send error message before boot on next if statement
        const char* message = "Booting connection due to repeated invalid choice.";
        send_error_message(thread_args->client_fd, 0, message);
    }
    // Check the state for the client before proceeding
    CharacterState* state = find_pp_by_fd(thread_args->client_fd);
    if (state == NULL) {
        fprintf(stderr, "Client disconnected before accessing state for fd: %d\n", thread_args->client_fd);
        handle_client_disconnection(thread_args->client_fd);
        return NULL;
    }

    // Set variables for later use of name and description
    const char* player_name = state->name;
    const char* player_description = state->description;

    // Send inital room messages after player is successfully made
    send_room_message(thread_args->client_fd, &rooms[state->room_number - 1], NULL);
    Room *current_room = &rooms[state->room_number - 1];
    for (size_t i = 0; i < current_room->num_connections; i++) {
            send_connection_message(thread_args->client_fd, current_room->connections[i]);
    }
    printf("handle_character is done\n");

    // Retrieve initial boss pointers
    CharacterState *boss1 = get_monster_in_room(8, 0);
    CharacterState *boss2 = get_monster_in_room(9, 0);
    CharacterState *boss3 = get_monster_in_room(10, 0);
    CharacterState *boss4;


    int try_again2 = 0;

    while (try_again2 < 10) {
        // Reset player choice on start of loop
        player_choice = 0;
        
        // Check to see if all bosses are dead and send final message if they are
        if(!is_alive(boss1) && !is_alive(boss2) && !is_alive(boss3) && thread_args->final_message_sent == false){
            const char* message = "The heart of the woods has opened up! Return to the Adventure's Camp to enter it!";
            initialize_final_room();
            populate_final_room();
            boss4 = get_monster_in_room(14, 0);
            send_message(player_name, "Server", message, strlen(message), 1);
            thread_args->final_message_sent = true;
        }
        // If final message sent check for if the last boss is alive
        if(thread_args->final_message_sent == true && thread_args->end_message_sent == false){
            if(!is_alive(boss4)){
                const char* message = "Congratulations! You have defeated the Forest Guardian and restored the Whispering Woods to their former glory!";
                send_message(player_name, "Server", message, strlen(message), 1);
                thread_args->end_message_sent = true;
            }
        }
        printf("waiting for input again\n");
        printf("%d, %d\n", thread_args->client_fd, player_choice);
        ssize_t recv_result = recv(thread_args->client_fd, &player_choice, sizeof(player_choice), 0);

        // Check if recv failed or client disconnected
        if (recv_result <= 0) {
            fprintf(stderr, "Client disconnected or recv error for fd: %d\n", thread_args->client_fd);
            handle_client_disconnection(thread_args->client_fd);
            break;
        }

        // Handle player choice if player has not started yet
        if (player_choice != START_TYPE && !is_started(state)) {
            if (player_choice == CHARACTER_TYPE) {
                const char* message = "You haven't started the game yet!";
                send_error_message(thread_args->client_fd, 5, message);
                try_again2++;
                continue;
            } else if (player_choice == MESSAGE_TYPE){
                char randbuffer[1024];
                const char* message = "You haven't started the game yet!";
                recv(thread_args->client_fd, &randbuffer, 66, MSG_WAITALL);
                recv(thread_args->client_fd, &randbuffer, sizeof(randbuffer), 0);
                send_error_message(thread_args->client_fd, 5, message);
                try_again2++;
                continue;
            } else if (player_choice == CHANGE_ROOM_TYPE){
                char randbuffer[1024];
                const char* message = "You haven't started the game yet!";
                recv(thread_args->client_fd, &randbuffer, 2, MSG_WAITALL);
                send_error_message(thread_args->client_fd, 5, message);
                try_again2++;
                continue;
            } else if (player_choice == FIGHT_TYPE){
                const char* message = "You haven't started the game yet!";
                send_error_message(thread_args->client_fd, 5, message);
                try_again2++;
                continue;
            } else if (player_choice == PVP_FIGHT_TYPE){
                char randbuffer[1024];
                const char* message = "You haven't started the game yet!";
                recv(thread_args->client_fd, &randbuffer, 32, MSG_WAITALL);
                send_error_message(thread_args->client_fd, 5, message);
                try_again2++;
                continue;
            } else if (player_choice == LOOT_TYPE){
                char randbuffer[1024];
                const char* message = "You haven't started the game yet!";
                recv(thread_args->client_fd, &randbuffer, 32, MSG_WAITALL);
                send_error_message(thread_args->client_fd, 5, message);
                try_again2++;
                continue;
            } else if (player_choice == ERROR_TYPE){
                const char* message = "Booting connection due to invalid choice.";
                send_error_message(thread_args->client_fd, 0, message);
                handle_client_disconnection(thread_args->client_fd);
                break;
            } else if (player_choice == ACCEPT_TYPE){
                const char* message = "Booting connection due to invalid choice.";
                send_error_message(thread_args->client_fd, 0, message);
                handle_client_disconnection(thread_args->client_fd);
                break;
            } else if (player_choice == ROOM_TYPE){
                const char* message = "Booting connection due to invalid choice.";
                send_error_message(thread_args->client_fd, 0, message);
                handle_client_disconnection(thread_args->client_fd);
                break;
            } else if (player_choice == GAME_TYPE){
                const char* message = "Booting connection due to invalid choice.";
                send_error_message(thread_args->client_fd, 0, message);
                handle_client_disconnection(thread_args->client_fd);
                break;
            } else if (player_choice == LEAVE_TYPE){
                handle_client_disconnection(thread_args->client_fd);
                break;
            } else if (player_choice == CONNECTION_TYPE){
                const char* message = "Booting connection due to invalid choice.";
                send_error_message(thread_args->client_fd, 0, message);
                handle_client_disconnection(thread_args->client_fd);
                break;
            } else if (player_choice > 13) {
                printf("choice was greater than 13\n");
                continue;
                //This breaks the lurk_test because it sends values greater than 13 to start
              /*const char* message = "Booting connection due to invalid choice.";
                send_error_message(thread_args->client_fd, 0, message);
                handle_client_disconnection(thread_args->client_fd);
                break;*/
            } else if (player_choice == 0) {
                continue;
            }
        }

        //Handle player choice if player has started or player choice is start
        if (player_choice == CHANGE_ROOM_TYPE) {
            printf("Received type 2\n");
            int room_choice;
            recv(thread_args->client_fd, &room_choice, sizeof(room_choice), 0);
            handle_change_room(thread_args, room_choice);
            continue;
        } else if (player_choice == START_TYPE) {
            send_accept_message(thread_args->client_fd, player_choice);
            state->flags |= (1 << 4); // Started
            send_message(player_name, "Server", start_message, strlen(start_message), 1);
            send_character_message(thread_args->client_fd, state, player_name, player_description);
            send_room_message(thread_args->client_fd, &rooms[state->room_number - 1], NULL);
            for (size_t i = 0; i < current_room->num_connections; i++) {
                    send_connection_message(thread_args->client_fd, current_room->connections[i]);
            }
            add_player_to_room(&rooms[0], state);
            Room *room_1 = &rooms[0];
            // Send CHARACTER messages for the new player to all players in the room
            for (size_t i = 0; i < room_1->num_players; i++) {
                if (room_1->players[i] != NULL && room_1->players[i] != state) {
                    send_character_message(room_1->players[i]->client_fd, state, state->name, state->description);
                }
            }
            // Send CHARACTER messages for all players in the new room
            for (size_t i = 0; i < room_1->num_players; i++) {
                if (room_1->players[i] != NULL) {
                    send_character_message(thread_args->client_fd, room_1->players[i], room_1->players[i]->name, room_1->players[i]->description);
                }
            }
            continue;
        } else if (player_choice == MESSAGE_TYPE) {
            printf("Received type 1\n");
            receive_message(thread_args->client_fd, buffer, sizeof(buffer));
            send_accept_message(thread_args->client_fd, player_choice);
            continue;
        } else if (player_choice == LEAVE_TYPE) {
            printf("Received type 12\n");
            handle_client_disconnection(thread_args->client_fd);
            break;
        } else if (player_choice == FIGHT_TYPE) {
            printf("Recieved type 3\n");
            CharacterState *monster = get_monster_in_room(state->room_number, 0);
            if (monster != NULL) {
                printf("Monster found: %s\n", monster->name);
            } else {
                const char* message = "There are no monsters to fight!";
                printf("Monster not found.\n");
                send_error_message(thread_args->client_fd, 7, message);
                continue;
            }
            if (!is_alive(monster)) {
                const char* message = "There are no monsters to fight!";
                printf("%s cannot fight because they are not alive.\n", monster->name);
                send_error_message(thread_args->client_fd, 7, message);
                continue;
            }
            send_accept_message(thread_args->client_fd, player_choice);
            handle_fight(state, monster);
            send_character_message(thread_args->client_fd, state, player_name, player_description);
            send_character_message(thread_args->client_fd, monster, monster->name, monster->description);
            continue;

        } else if (player_choice == PVP_FIGHT_TYPE) {
            char buff[32];
            recv(thread_args->client_fd, buff, 32, MSG_WAITALL);
            const char* message = "Server does not support PVP combat";
            send_error_message(thread_args->client_fd, 8, message);
            continue;
        } else if (player_choice == LOOT_TYPE) {
            printf("received type 5\n");
            send_accept_message(thread_args->client_fd, player_choice);
            handle_loot(thread_args);
            continue;
        }else if (player_choice == 0) {
            printf("Received type 0\n");
            const char* message = "Booting connection due to invalid choice.";
            send_error_message(thread_args->client_fd, 0, message);
            handle_client_disconnection(thread_args->client_fd);
            break;
        } else {
            printf("received type: %d from :%d\n", player_choice, thread_args->client_fd);
            continue;
        }
    }

    if(try_again2 >= 10){
        const char* message = "Booting connection due to repeated invalid choice.";
        send_error_message(thread_args->client_fd, 0, message);
    }
    return NULL;
}

// Function for checking if a CHARACTER is alive
int is_alive(CharacterState *character) {
    return (character->flags & 0x80) != 0; // Check if the highest bit (Bit 7) is set
}

// Function for checking if a CHARACTER has started
int is_started(CharacterState *character) {
    return (character->flags & FLAG_STARTED) != 0;
}

// Function for handling client disconnection when given a client file descriptor
void handle_client_disconnection(int client_fd) {
    // Find the player associated with the client_fd
    CharacterState *player = find_pp_by_fd(client_fd);
    if (player != NULL) {
        player->client_fd = -1; // Indicate disconnection
    } else {
        printf("No player found associated with fd: %d\n", client_fd);
    }

    // Close the client socket
    if (client_fd >= 0) {
        if (close(client_fd) == -1) {
            perror("Error closing client socket");
        } else {
            printf("Client with fd %d has left the game.\n", client_fd);
        }
    } else {
        printf("Invalid client file descriptor: %d\n", client_fd);
    }

    // Adjust thread count and remove the thread args entry
    pthread_mutex_lock(&thread_count_mutex);
    for (int i = 0; i < thread_count; i++) {
        if (thread_args[i]->client_fd == client_fd) {
            free(thread_args[i]); // Free the allocated memory
            // Shift remaining threads down
            for (int j = i; j < thread_count - 1; j++) {
                thread_args[j] = thread_args[j + 1];
            }
            thread_args[thread_count - 1] = NULL; // Optional: Nullify the last entry
            thread_count--; // Decrement thread count
            break; // Exit loop after handling
        }
    }
    pthread_mutex_unlock(&thread_count_mutex);
}

// Function for finding a copy of a player based on a client file descriptor
CharacterState find_player_by_fd(int client_fd) {
    for (int i = 0; i < player_count; i++) {
        if (global_players[i].client_fd == client_fd) {
            return global_players[i];  // Return a copy of the character state
        }
    }
    // Return a default state if not found
    return (CharacterState){0};
}

// Function for finding a pointer to a player when given a client file descriptor
CharacterState* find_pp_by_fd(int client_fd) {
    // Assume players is an array of CharacterState
    for (int i = 0; i < player_count; i++) {
        if (global_players[i].client_fd == client_fd) {
            return &global_players[i]; // Return a pointer to the found player
        }
    }
    return nullptr; // Return nullptr if not found
}

// Function for finding a room based on a CHARACTER
Room* find_room_by_character(CharacterState *character) {
    if (character == NULL) {
        printf("Invalid character state provided.\n");
        return NULL;
    }

    uint16_t room_index = character->room_number - 1;

    // Check if the room index is valid
    if (room_index < 0 || room_index >= MAX_ROOMS) {
        printf("Room number %d is out of bounds.\n", character->room_number);
        return NULL;
    }

    return &rooms[room_index]; // Return the corresponding room
}

// Function for adding a CHARACTER to a specific room's list
int add_player_to_room(Room *room, CharacterState *player) {
    if (room->num_players < MAX_PLAYERS) {
        room->players[room->num_players++] = player;
        return 0; // Success
    }
    return -1; // Room is full
}

// Function for removing a CHARACTER from a specific room's list
int remove_player_from_room(Room *room, CharacterState *player) {
    for (size_t i = 0; i < room->num_players; ++i) {
        if (room->players[i] == player) {
            // Shift remaining players down
            for (size_t j = i; j < room->num_players - 1; ++j) {
                room->players[j] = room->players[j + 1];
            }
            room->num_players--;
            return 0; // Success
        }
    }
    return -1; // Player not found
}

// Function for sending VERSION message to client based on a client file descriptor
void send_version_message(int client_fd) {
    uint8_t version_message[5];

    version_message[0] = VERSION_TYPE;   
    version_message[1] = 2;              
    version_message[2] = 3;              
    uint16_t extensions_size = 0; 
    version_message[3] = extensions_size & 0xFF;  
    version_message[4] = (extensions_size >> 8) & 0xFF;  

    ssize_t bytes_sent = send(client_fd, version_message, sizeof(version_message), 0);
    if (bytes_sent == -1) {
        perror("send VERSION");
    } else {
        printf("Sent VERSION message\n");
    }
}

// Function for sending GAME message to client based on a client file descriptor
void send_game_message(int client_fd) {
    uint16_t initial_points = 1000;
    uint16_t stat_limit = 1000;
    const char *description = DESCRIPTION;
    uint16_t description_length = strlen(description);

    size_t message_size = 7 + description_length;
    uint8_t *message = (uint8_t *)malloc(message_size);

    if (message == NULL) {
        perror("malloc");
        close(client_fd);
        return;
    }

    message[0] = GAME_TYPE;
    message[1] = initial_points & 0xFF;
    message[2] = (initial_points >> 8) & 0xFF;
    message[3] = stat_limit & 0xFF;
    message[4] = (stat_limit >> 8) & 0xFF;
    message[5] = description_length & 0xFF;
    message[6] = (description_length >> 8) & 0xFF;
    memcpy(message + 7, description, description_length);

    ssize_t bytes_sent = send(client_fd, message, message_size, 0);
    if (bytes_sent == -1) {
        perror("send GAME");
    } else {
        printf("Sent GAME message\n");
    }

    free(message);
}

// Function for sending ERROR message to client based on a client file descriptor
void send_error_message(int client_fd, uint8_t error_code, const char *error_message) {
    uint16_t message_length = strlen(error_message);  
    uint16_t total_length = 4 + message_length; 

    uint8_t *message = (uint8_t *)malloc(total_length);
    if (message == NULL) {
        perror("malloc");
        return; 
    }

    message[0] = ERROR_TYPE; 
    message[1] = error_code;
    message[2] = message_length & 0xFF; 
    message[3] = (message_length >> 8) & 0xFF; 
    memcpy(message + 4, error_message, message_length);

    ssize_t bytes_sent = send(client_fd, message, total_length, 0);
    if (bytes_sent == -1) {
        perror("send ERROR");
    } else {
        printf("Sent ERROR message: %s\n", error_message);
    }

    free(message);
}

// Function for sending ACCEPT message to client based on a client file descriptor
void send_accept_message(int client_fd, uint8_t action_type) {
    uint8_t accept_message[2];
    accept_message[0] = ACCEPT_TYPE;  
    accept_message[1] = action_type;  

    ssize_t bytes_sent = send(client_fd, accept_message, sizeof(accept_message), 0);
    if (bytes_sent == -1) {
        perror("send ACCEPT");
    } else {
        printf("Sent ACCEPT message for action type %d\n", action_type);
    }
    CharacterState* state = find_pp_by_fd(client_fd);
    const char* player_name = state->name;
    const char* player_description = state->description;
}

// Function to send CHARACTER message to client based on client file descriptor
void send_character_message(int client_fd, CharacterState *character_state, const char *player_name, const char *description) {
    // Define message size and create a buffer
    uint8_t message[512]; // Adjust size as needed

    // Initialize message with CHARACTER type
    message[0] = CHARACTER_TYPE; // CHARACTER message type

    // Copy the player name (1-32)
    strncpy((char*)&message[1], player_name, 32);

    // Ensure lower 3 bits are zero
    message[33] = character_state->flags;

    // Set Attack (34-35)
    message[34] = character_state->attack & 0xFF;
    message[35] = (character_state->attack >> 8) & 0xFF;

    // Set Defense (36-37)
    message[36] = character_state->defense & 0xFF;
    message[37] = (character_state->defense >> 8) & 0xFF;

    // Set Regen (38-39)
    message[38] = character_state->regen & 0xFF;
    message[39] = (character_state->regen >> 8) & 0xFF;

    // Set Health (40-41) - signed
    message[40] = character_state->health & 0xFF;
    message[41] = (character_state->health >> 8) & 0xFF;

    // Set Gold (42-43)
    message[42] = character_state->gold & 0xFF;
    message[43] = (character_state->gold >> 8) & 0xFF;

    // Set Current Room Number (44-45)
    message[44] = character_state->room_number & 0xFF;
    message[45] = (character_state->room_number >> 8) & 0xFF;

    // Set Description Length (46-47)
    size_t description_length = strlen(description);
    message[46] = description_length & 0xFF;
    message[47] = (description_length >> 8) & 0xFF;

    // Copy Player Description (48+)
    strncpy((char*)&message[48], description, description_length);
    // Null-terminate if there's space
    if (description_length < sizeof(message) - 48) {
        message[48 + description_length] = '\0';
    }

    // Send the message
    ssize_t bytes_sent = send(client_fd, message, 48 + description_length, 0);
    if (bytes_sent == -1) {
        perror("send CHARACTER");
    } else {
        printf("Sent CHARACTER message for player %s in room %d to fd %d\n", player_name, character_state->room_number, client_fd);
    }
}

// Function to send ROOM message to client based on client file descriptor
void send_room_message(int client_fd, Room *room, CharacterState *character_state) {
    uint16_t description_length = strlen(room->description);
    size_t message_size = 37 + description_length;
    uint8_t *message = (uint8_t *)malloc(message_size);

    if (message == NULL) {
        perror("malloc");
        return;
    }

    message[0] = ROOM_TYPE;
    message[1] = room->room_number & 0xFF;
    message[2] = (room->room_number >> 8) & 0xFF;

    strncpy((char *)message + 3, room->name, 32);
    memset((char *)message + 3 + strlen(room->name), 0, 32 - strlen(room->name));

    message[35] = description_length & 0xFF;
    message[36] = (description_length >> 8) & 0xFF;
    memcpy(message + 37, room->description, description_length);

    ssize_t bytes_sent = send(client_fd, message, message_size, 0);
    if (bytes_sent == -1) {
        perror("send ROOM");
    } else {
        printf("Sent ROOM message for room %d\n", room->room_number);
    }

    free(message);
}

// Function to send CONNECTION message to client based on client file descriptor
void send_connection_message(int client_fd, Room *room) {
    uint16_t description_length = strlen(room->description);
    size_t message_size = 37 + description_length;
    uint8_t *message = (uint8_t *)malloc(message_size);

    if (message == NULL) {
        perror("malloc");
        return;
    }

    message[0] = CONNECTION_TYPE;
    message[1] = room->room_number & 0xFF;
    message[2] = (room->room_number >> 8) & 0xFF;

    strncpy((char *)message + 3, room->name, 32);
    memset((char *)message + 3 + strlen(room->name), 0, 32 - strlen(room->name));

    message[35] = description_length & 0xFF;
    message[36] = (description_length >> 8) & 0xFF;
    memcpy(message + 37, room->description, description_length);

    ssize_t bytes_sent = send(client_fd, message, message_size, 0);
    if (bytes_sent == -1) {
        perror("send CONNECTION");
    } else {
        printf("Sent CONNECTION message for room %d\n", room->room_number);
    }

    free(message);
    printf("send_connection_message is ending\n");
}

// Function to receive MESSAGE type from specified client file descriptor
void receive_message(int sender_fd, char *buffer, size_t buffer_size) {
    // Read the message from the sender
    ssize_t bytes_received = recv(sender_fd, buffer, 66, MSG_WAITALL);
    char msg_buffer[65535];
    printf("Read message from %d\n", sender_fd);
    
    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            printf("Client disconnected\n");
        } else {
            perror("recv");
        }
        return;
    }

    // Check if the message is valid and has enough bytes
    if (bytes_received < 66) {
        printf("Received message too short: %ld bytes\n", bytes_received);
        return;
    }

    // Extract message length
    uint16_t message_length;
    memcpy(&message_length, buffer, sizeof(message_length)); // Copy the first two bytes
    printf("Message length: %d\n", message_length);

    if (message_length > 65535) {
        printf("Message length exceeds maximum size.\n");
        return;
    }

    // Extract recipient name
    char recipient_name[33]; // 32 bytes + null terminator
    memcpy(recipient_name, buffer + 2, 32); // Start from buffer[2]
    recipient_name[32] = '\0'; // Null-terminate
    printf("Recipient Name: %s\n", recipient_name);

    // Extract sender name
    char sender_name[33]; // 32 bytes + null terminator
    memcpy(sender_name, buffer + 34, 32); // Start from buffer[34]
    sender_name[32] = '\0'; // Null-terminate
    printf("Sender Name: %s\n", sender_name);

    // Handle narration marker
    uint8_t narration_marker = buffer[64]; // At buffer[64]
    printf("Narration marker: %d\n", narration_marker);

    // Extract the actual message
    ssize_t msg_bytes_received = recv(sender_fd, msg_buffer, message_length, MSG_WAITALL);
    if (msg_bytes_received <= 0) {
        if (msg_bytes_received == 0) {
            printf("Client disconnected during message body read.\n");
        } else {
            perror("recv");
        }
        return;
    }

    if (msg_bytes_received < message_length) {
        printf("Received only %ld bytes of the message body.\n", msg_bytes_received);
        return;
    }

    // Null-terminate the message body
    msg_buffer[msg_bytes_received] = '\0'; // Ensure null-terminated string
    printf("Message: %s\n", msg_buffer);

    // Prepare to send this message to the correct client
    send_message(recipient_name, sender_name, msg_buffer, msg_bytes_received, narration_marker);
}

// Function to send MESSAGE to specified client based on file descriptor
void send_message(const char *recipient_name, const char *sender_name, const char *message, size_t message_length, uint8_t narration_marker) {
    // Allocate buffer for the entire message
    size_t total_length = 67 + message_length; // 67 bytes for header + message length
    char *buffer = (char *)malloc(total_length);
    if (!buffer) {
        perror("malloc");
        return;
    }

    // Set the message type (1)
    buffer[0] = 1;

    // Set the message length (little-endian)
    buffer[1] = (message_length & 0xFF);           // Lower byte
    buffer[2] = ((message_length >> 8) & 0xFF);    // Upper byte

    // Copy recipient name
    strncpy(buffer + 3, recipient_name, 32);
    buffer[35] = '\0';  // Ensure null-termination

    // Copy sender name
    strncpy(buffer + 35, sender_name, 30);
    buffer[65] = 0; // Ensure the last byte is 0 for the sender name
    buffer[66] = narration_marker; // Set narration marker

    // Copy the actual message body
    memcpy(buffer + 67, message, message_length);

    // Find the recipient using thread_args
    for (int i = 0; i < thread_count; i++) {
        ThreadArgs *args = thread_args[i];
        CharacterState player = find_player_by_fd(args->client_fd);

        if (strcmp(player.name, recipient_name) == 0) {
            ssize_t bytes_sent = send(args->client_fd, buffer, total_length, 0);
            if (bytes_sent == -1) {
                perror("send");
            }
            printf("Message sent to %s\n", player.name);
            free(buffer); // Free the allocated buffer
            return; // Exit after sending the message
        }
    }

    printf("Recipient not found: %s\n", recipient_name);
    free(buffer); // Free buffer if recipient not found
}

// Function to allocate memory for string
char* allocate_string(const char* str) {
    // Allocate memory for the string
    char* new_string = (char*)malloc(strlen(str) + 1); // +1 for null terminator
    if (new_string != NULL) {
        strcpy(new_string, str); // Copy the string
    }
    return new_string; // Return the allocated string (or NULL if allocation failed)
}

// Function to initialize rooms and their connections
void initialize_rooms() {
    // Initialize room 1
    rooms[0].room_number = 1;
    rooms[0].name = allocate_string("The Adventure's Camp");
    rooms[0].description = allocate_string("A cozy campsite at the edge of the Whispering Woods. Adventurers gather here to share stories and prepare for their journeys.");
    rooms[0].num_connections = 3;
    rooms[0].connections[0] = &rooms[1]; // Room Two
    rooms[0].connections[1] = &rooms[2]; // Room Three
    rooms[0].connections[2] = &rooms[3]; // Room Four
    rooms[0].num_players = 0;
    rooms[0].num_monsters = 0;

    // Initialize room 2
    rooms[1].room_number = 2;
    rooms[1].name = allocate_string("Whispering Meadow");
    rooms[1].description = allocate_string("A peaceful meadow filled with wildflowers. The soft whispers of nature surround you.");
    rooms[1].num_connections = 2;
    rooms[1].connections[0] = &rooms[0]; // Room One
    rooms[1].connections[1] = &rooms[4]; // Room Five
    rooms[1].num_players = 0;
    rooms[1].num_monsters = 0;

    // Initialize room 3
    rooms[2].room_number = 3;
    rooms[2].name = allocate_string("Flickering Torches");
    rooms[2].description = allocate_string("A dimly lit area where the flicker of torches reveals twisted shadows.");
    rooms[2].num_connections = 2;
    rooms[2].connections[0] = &rooms[0]; // Room One
    rooms[2].connections[1] = &rooms[5]; // Room Six
    rooms[2].num_players = 0;
    rooms[2].num_monsters = 0;

    // Initialize room 4
    rooms[3].room_number = 4;
    rooms[3].name = allocate_string("Ancient Overlook");
    rooms[3].description = allocate_string("A high vantage point overlooking the forest, with ruins visible in the distance.");
    rooms[3].num_connections = 2;
    rooms[3].connections[0] = &rooms[0]; // Room One
    rooms[3].connections[1] = &rooms[6]; // Room Seven
    rooms[3].num_players = 0;
    rooms[3].num_monsters = 0;

    // Initialize room 5
    rooms[4].room_number = 5;
    rooms[4].name = allocate_string("Sunlit Glen");
    rooms[4].description = allocate_string("Sunbeams filter through the trees, illuminating the vibrant flora.");
    rooms[4].num_connections = 2;
    rooms[4].connections[0] = &rooms[1]; // Room Two
    rooms[4].connections[1] = &rooms[7]; // Room Eight
    rooms[4].num_players = 0;
    rooms[4].num_monsters = 0;

    // Initialize room 6
    rooms[5].room_number = 6;
    rooms[5].name = allocate_string("Eerie Clearing");
    rooms[5].description = allocate_string("An open area filled with twisted trees and an unsettling silence.");
    rooms[5].num_connections = 2;
    rooms[5].connections[0] = &rooms[2]; // Room Three
    rooms[5].connections[1] = &rooms[8]; // Room Nine
    rooms[5].num_players = 0;
    rooms[5].num_monsters = 0;

    // Initialize room 7
    rooms[6].room_number = 7;
    rooms[6].name = allocate_string("Forgotten Hall");
    rooms[6].description = allocate_string("A long hallway filled with crumbling statues and dusty artifacts.");
    rooms[6].num_connections = 2;
    rooms[6].connections[0] = &rooms[3]; // Room Four
    rooms[6].connections[1] = &rooms[9]; // Room Ten
    rooms[6].num_players = 0;
    rooms[6].num_monsters = 0;

    // Initialize room 8
    rooms[7].room_number = 8;
    rooms[7].name = allocate_string("Pixie Grove");
    rooms[7].description = allocate_string("A vibrant area filled with playful pixies");
    rooms[7].num_connections = 2;
    rooms[7].connections[0] = &rooms[4]; // Room Five
    rooms[7].connections[1] = &rooms[10]; //Room Eleven
    rooms[7].num_players = 0;
    rooms[7].num_monsters = 0;

    // Initialize room 9
    rooms[8].room_number = 9;
    rooms[8].name = allocate_string("Shadowed Path");
    rooms[8].description = allocate_string("A dark trail with an oppressive atmosphere.");
    rooms[8].num_connections = 2;
    rooms[8].connections[0] = &rooms[5]; // Room Six
    rooms[8].connections[1] = &rooms[11]; // Room Twelve
    rooms[8].num_players = 0;
    rooms[8].num_monsters = 0;

    // Initialize room 10
    rooms[9].room_number = 10;
    rooms[9].name = allocate_string("Overgrown Temple");
    rooms[9].description = allocate_string("A crumbling temple entwined with vines and magic.");
    rooms[9].num_connections = 2;
    rooms[9].connections[0] = &rooms[6]; // Room Seven
    rooms[9].connections[1] = &rooms[12]; // Room Thirteen
    rooms[9].num_players = 0;
    rooms[9].num_monsters = 0;

    // Initialize room 11
    rooms[10].room_number = 11;
    rooms[10].name = allocate_string("Glimmering Glade");
    rooms[10].description = allocate_string("Moonlight filters through the canopy, casting silvery beams onto the lush undergrowth, which sparkles as if sprinkled with stardust. Colorful flowers bloom in vibrant patches, their petals shimmering with an otherworldly glow. The gentle sound of a nearby stream adds a serene melody to the atmosphere, while the air is filled with the sweet scent of blooming jasmine and wild lavender.");
    rooms[10].num_connections = 1;
    rooms[10].connections[0] = &rooms[7]; // Room Eight
    rooms[10].num_players = 0;
    rooms[10].num_monsters = 0;

    // Initialize room 12
    rooms[11].room_number = 12;
    rooms[11].name = allocate_string("Cursed Hollow");
    rooms[11].description = allocate_string("Gnarled trees twist towards the sky, their branches like skeletal fingers grasping at the fog that swirls around your feet. A chilling wind whistles through the hollow, carrying whispers of lost souls. Patches of blackened earth and wilting flowers are scattered throughout, marking the remains of what once thrived here.");
    rooms[11].num_connections = 1;
    rooms[11].connections[0] = &rooms[8]; // Room Nine
    rooms[11].num_players = 0;
    rooms[11].num_monsters = 0;

    // Initialize room 13
    rooms[12].room_number = 13;
    rooms[12].name = allocate_string("Aincent Ruins");
    rooms[12].description = allocate_string("Intricate carvings tell the stories of a once-great civilization, now lost to time. As you navigate through the maze of fallen pillars and broken walls, the echoes of the past seem to whisper around you, and a sense of reverence washes over you. The air is thick with the scent of damp earth and aged stone, evoking a deep connection to history.");
    rooms[12].num_connections = 1;
    rooms[12].connections[0] = &rooms[9]; // Room Ten
    rooms[12].num_players = 0;
    rooms[12].num_monsters = 0;

}

// Function to initialize the final room and add it to 1st room's connection
void initialize_final_room(){
    //Properly add connection to first room
    rooms[0].num_connections = 2;
    rooms[0].connections[1] = &rooms[13];

    //Initialize room 14
    rooms[13].room_number = 14;
    rooms[13].name = allocate_string("The Heart of the Woods");
    rooms[13].description = allocate_string("A beautiful yet haunting glade where the forest's magic converges");
    rooms[13].num_connections = 1;
    rooms[13].connections[0] = &rooms[0]; // Room One
    rooms[13].num_players = 0;
    rooms[13].num_monsters = 0;
}

// Function to populate all ROOMs with correct MONSTERs
void populate_rooms_with_monsters() {
    // Check if there's space for more monsters
    if (num_monsters >= MAX_MONSTERS) {
        printf("Maximum number of monsters reached.\n");
        return;
    }

    // Create monster instances
    CharacterState monster1 = {
        .alive = 1,
        .join_battle = 1,
        .monster = 1,
        .started = 1,
        .ready = 1,
        .type = 0, // Example type
        .name = "Flower Sprite",
        .flags = 0xb8,
        .attack = 5,
        .defense = 2,
        .regen = 1,
        .health = 10,
        .gold = 5,
        .room_number = 2,
        .description_length = strlen("Tiny, ethereal creatures resembling colorful flowers with wings. They flit through the air, leaving trails of shimmering pollen. Flower Sprites are mischievous but gentle, often playing tricks on those who disturb their meadow. However, they can defend their territory with sharp thorns when threatened."),
        .description = (char *)malloc(strlen("Tiny, ethereal creatures resembling colorful flowers with wings. They flit through the air, leaving trails of shimmering pollen. Flower Sprites are mischievous but gentle, often playing tricks on those who disturb their meadow. However, they can defend their territory with sharp thorns when threatened.") + 1),
        .description_received = 0,
        .total_received = 0,
        .client_fd = -1 // Not applicable for monsters
    };
    if (monster1.description != NULL) {
        strcpy(monster1.description, "Tiny, ethereal creatures resembling colorful flowers with wings. They flit through the air, leaving trails of shimmering pollen. Flower Sprites are mischievous but gentle, often playing tricks on those who disturb their meadow. However, they can defend their territory with sharp thorns when threatened.");
    }

    CharacterState monster2 = {
        .alive = 1,
        .join_battle = 1,
        .monster = 1,
        .started = 1,
        .ready = 1,
        .type = 0,
        .name = "Glade Stalker",
        .flags = 0xb8,
        .attack = 8,
        .defense = 4,
        .regen = 2,
        .health = 20,
        .gold = 10,
        .room_number = 5,
        .description_length = strlen("Agile forest creatures with sleek bodies covered in mottled green and brown fur, allowing them to blend seamlessly with their surroundings. Their eyes glint with intelligence, and they hunt in packs, using teamwork to ambush unsuspecting travelers. Glade Stalkers are known for their speed and cunning."),
        .description = (char *)malloc(strlen("Agile forest creatures with sleek bodies covered in mottled green and brown fur, allowing them to blend seamlessly with their surroundings. Their eyes glint with intelligence, and they hunt in packs, using teamwork to ambush unsuspecting travelers. Glade Stalkers are known for their speed and cunning.") + 1),
        .description_received = 0,
        .total_received = 0,
        .client_fd = -1
    };
    if (monster2.description != NULL) {
        strcpy(monster2.description, "Agile forest creatures with sleek bodies covered in mottled green and brown fur, allowing them to blend seamlessly with their surroundings. Their eyes glint with intelligence, and they hunt in packs, using teamwork to ambush unsuspecting travelers. Glade Stalkers are known for their speed and cunning.");
    }
    CharacterState monster3 = {
        .alive = 1,
        .join_battle = 1,
        .monster = 1,
        .started = 1,
        .ready = 1,
        .type = 0,
        .name = "Queen of the Pixies",
        .flags = 0xb8,
        .attack = 8,
        .defense = 4,
        .regen = 2,
        .health = 20,
        .gold = 10,
        .room_number = 8,
        .description_length = strlen("A radiant figure adorned in sparkling leaves and flowers, the Queen of the Pixies commands respect among her kin. With a crown of luminescent blooms, she weaves powerful enchantments that can manipulate nature itself. Her laughter echoes like chimes, but her wrath can unleash wild magic upon intruders."),
        .description = (char *)malloc(strlen("A radiant figure adorned in sparkling leaves and flowers, the Queen of the Pixies commands respect among her kin. With a crown of luminescent blooms, she weaves powerful enchantments that can manipulate nature itself. Her laughter echoes like chimes, but her wrath can unleash wild magic upon intruders.") + 1),
        .description_received = 0,
        .total_received = 0,
        .client_fd = -1
    };
    if (monster3.description != NULL) {
        strcpy(monster3.description, "A radiant figure adorned in sparkling leaves and flowers, the Queen of the Pixies commands respect among her kin. With a crown of luminescent blooms, she weaves powerful enchantments that can manipulate nature itself. Her laughter echoes like chimes, but her wrath can unleash wild magic upon intruders.");
    }
    CharacterState monster4 = {
        .alive = 1,
        .join_battle = 1,
        .monster = 1,
        .started = 1,
        .ready = 1,
        .type = 0,
        .name = "Cursed Spirit",
        .flags = 0xb8,
        .attack = 8,
        .defense = 4,
        .regen = 2,
        .health = 20,
        .gold = 10,
        .room_number = 3,
        .description_length = strlen("Wispy, shadowy figures that drift through the air, shrouded in sorrow and regret. Cursed Spirits whisper mournful tales of their past, seeking to ensnare the hearts of the living. They can drain the strength of those who come too close, leaving their victims weakened and vulnerable."),
        .description = (char *)malloc(strlen("Wispy, shadowy figures that drift through the air, shrouded in sorrow and regret. Cursed Spirits whisper mournful tales of their past, seeking to ensnare the hearts of the living. They can drain the strength of those who come too close, leaving their victims weakened and vulnerable.") + 1),
        .description_received = 0,
        .total_received = 0,
        .client_fd = -1
    };
    if (monster4.description != NULL) {
        strcpy(monster4.description, "Wispy, shadowy figures that drift through the air, shrouded in sorrow and regret. Cursed Spirits whisper mournful tales of their past, seeking to ensnare the hearts of the living. They can drain the strength of those who come too close, leaving their victims weakened and vulnerable.");
    } 
    CharacterState monster5 = {
        .alive = 1,
        .join_battle = 1,
        .monster = 1,
        .started = 1,
        .ready = 1,
        .type = 0,
        .name = "Spectral Hound",
        .flags = 0xb8,
        .attack = 8,
        .defense = 4,
        .regen = 2,
        .health = 20,
        .gold = 10,
        .room_number = 6,
        .description_length = strlen("Ghostly canines with glowing eyes and translucent bodies, Spectral Hounds roam in packs, hunting down those who wander too far into the hollow. Their eerie howls send chills down the spine, and they strike swiftly, using their agility to overpower their prey."),
        .description = (char *)malloc(strlen("Ghostly canines with glowing eyes and translucent bodies, Spectral Hounds roam in packs, hunting down those who wander too far into the hollow. Their eerie howls send chills down the spine, and they strike swiftly, using their agility to overpower their prey.") + 1),
        .description_received = 0,
        .total_received = 0,
        .client_fd = -1
    };
    if (monster5.description != NULL) {
        strcpy(monster5.description, "Ghostly canines with glowing eyes and translucent bodies, Spectral Hounds roam in packs, hunting down those who wander too far into the hollow. Their eerie howls send chills down the spine, and they strike swiftly, using their agility to overpower their prey.");
    } 
    CharacterState monster6 = {
        .alive = 1,
        .join_battle = 1,
        .monster = 1,
        .started = 1,
        .ready = 1,
        .type = 0,
        .name = "Wraith Lord",
        .flags = 0xb8,
        .attack = 8,
        .defense = 4,
        .regen = 2,
        .health = 20,
        .gold = 10,
        .room_number = 9,
        .description_length = strlen("A formidable specter draped in tattered robes, the Wraith Lord exudes an aura of despair. With a face obscured by shadows, he wields dark magic and commands the spirits of the fallen. His presence is accompanied by a chilling wind, and his attacks can sap the very essence of life."),
        .description = (char *)malloc(strlen("A formidable specter draped in tattered robes, the Wraith Lord exudes an aura of despair. With a face obscured by shadows, he wields dark magic and commands the spirits of the fallen. His presence is accompanied by a chilling wind, and his attacks can sap the very essence of life.") + 1),
        .description_received = 0,
        .total_received = 0,
        .client_fd = -1
    };
    if (monster6.description != NULL) {
        strcpy(monster6.description, "A formidable specter draped in tattered robes, the Wraith Lord exudes an aura of despair. With a face obscured by shadows, he wields dark magic and commands the spirits of the fallen. His presence is accompanied by a chilling wind, and his attacks can sap the very essence of life.");
    }
    CharacterState monster7 = {
        .alive = 1,
        .join_battle = 1,
        .monster = 1,
        .started = 1,
        .ready = 1,
        .type = 0,
        .name = "Vengeful Spirit",
        .flags = 0xb8,
        .attack = 8,
        .defense = 4,
        .regen = 2,
        .health = 20,
        .gold = 10,
        .room_number = 4,
        .description_length = strlen("Ethereal beings bound to the ruins, these spirits are manifestations of anger and sorrow. They appear as flickering lights or shadowy figures, their features twisted by grief. Vengeful Spirits seek to protect their ancient home, attacking those who show disrespect or intent to plunder."),
        .description = (char *)malloc(strlen("Ethereal beings bound to the ruins, these spirits are manifestations of anger and sorrow. They appear as flickering lights or shadowy figures, their features twisted by grief. Vengeful Spirits seek to protect their ancient home, attacking those who show disrespect or intent to plunder.") + 1),
        .description_received = 0,
        .total_received = 0,
        .client_fd = -1
    };
    if (monster7.description != NULL) {
        strcpy(monster7.description, "Ethereal beings bound to the ruins, these spirits are manifestations of anger and sorrow. They appear as flickering lights or shadowy figures, their features twisted by grief. Vengeful Spirits seek to protect their ancient home, attacking those who show disrespect or intent to plunder.");
    }
    CharacterState monster8 = {
        .alive = 1,
        .join_battle = 1,
        .monster = 1,
        .started = 1,
        .ready = 1,
        .type = 0,
        .name = "Animated Armor",
        .flags = 0xb8,
        .attack = 8,
        .defense = 4,
        .regen = 2,
        .health = 20,
        .gold = 10,
        .room_number = 7,
        .description_length = strlen("Once noble knights, these armors have been enchanted to guard the ruins. Clattering and creaking, they move with surprising agility, wielding swords and shields made of ancient metal. Their eyes glow with a faint blue light, and they are relentless in their duty to defend the secrets of the past."),
        .description = (char *)malloc(strlen("Once noble knights, these armors have been enchanted to guard the ruins. Clattering and creaking, they move with surprising agility, wielding swords and shields made of ancient metal. Their eyes glow with a faint blue light, and they are relentless in their duty to defend the secrets of the past.") + 1),
        .description_received = 0,
        .total_received = 0,
        .client_fd = -1
    };
    if (monster8.description != NULL) {
        strcpy(monster8.description, "Once noble knights, these armors have been enchanted to guard the ruins. Clattering and creaking, they move with surprising agility, wielding swords and shields made of ancient metal. Their eyes glow with a faint blue light, and they are relentless in their duty to defend the secrets of the past.");
    }
    CharacterState monster9 = {
        .alive = 1,
        .join_battle = 1,
        .monster = 1,
        .started = 1,
        .ready = 1,
        .type = 0,
        .name = "Golem Sentinel",
        .flags = 0xb8,
        .attack = 8,
        .defense = 4,
        .regen = 2,
        .health = 20,
        .gold = 10,
        .room_number = 10,
        .description_length = strlen("A massive construct of stone and magic, the Golem Sentinel stands as the last line of defense for the ancient ruins. Its body is covered in intricate carvings that glow with an inner light. It moves slowly but strikes with devastating force, and its elemental magic can reshape the battlefield."),
        .description = (char *)malloc(strlen("A massive construct of stone and magic, the Golem Sentinel stands as the last line of defense for the ancient ruins. Its body is covered in intricate carvings that glow with an inner light. It moves slowly but strikes with devastating force, and its elemental magic can reshape the battlefield.") + 1),
        .description_received = 0,
        .total_received = 0,
        .client_fd = -1
    };
    if (monster9.description != NULL) {
        strcpy(monster9.description, "A massive construct of stone and magic, the Golem Sentinel stands as the last line of defense for the ancient ruins. Its body is covered in intricate carvings that glow with an inner light. It moves slowly but strikes with devastating force, and its elemental magic can reshape the battlefield.");
    }
    // Add monsters to the global array
    monsters[num_monsters++] = monster1;
    monsters[num_monsters++] = monster2;
    monsters[num_monsters++] = monster3;
    monsters[num_monsters++] = monster4;
    monsters[num_monsters++] = monster5;
    monsters[num_monsters++] = monster6;
    monsters[num_monsters++] = monster7;
    monsters[num_monsters++] = monster8;
    monsters[num_monsters++] = monster9;

    // Assign monsters to their respective rooms
    for (size_t i = 0; i < num_monsters; i++) {
        CharacterState *monster = &monsters[i];
        Room *room = &rooms[monster->room_number - 1]; // Assuming room numbers are 1-indexed

        // Add to the room's monster list if space allows
        if (room->num_monsters < MAX_MONSTERS) {
            room->monsters[room->num_monsters++] = monster;
        }
    }
}

// Function to populate final ROOM with final boss
void populate_final_room(){
    CharacterState monster10 = {
        .alive = 1,
        .join_battle = 1,
        .monster = 1,
        .started = 1,
        .ready = 1,
        .type = 0,
        .name = "The Forest Guardian",
        .flags = 0xb8,
        .attack = 8,
        .defense = 4,
        .regen = 2,
        .health = 20,
        .gold = 10,
        .room_number = 14,
        .description_length = strlen("A majestic being embodying the essence of the forest, the Forest Guardian appears as a towering figure made of wood, leaves, and living vines. Its eyes shine with wisdom and power, and it can manipulate nature to protect its domain. The Guardian is both a fierce protector and a benevolent spirit, defending the heart of the woods from those who threaten it."),
        .description = (char *)malloc(strlen("A majestic being embodying the essence of the forest, the Forest Guardian appears as a towering figure made of wood, leaves, and living vines. Its eyes shine with wisdom and power, and it can manipulate nature to protect its domain. The Guardian is both a fierce protector and a benevolent spirit, defending the heart of the woods from those who threaten it.") + 1),
        .description_received = 0,
        .total_received = 0,
        .client_fd = -1
    };
    if (monster10.description != NULL) {
        strcpy(monster10.description, "A majestic being embodying the essence of the forest, the Forest Guardian appears as a towering figure made of wood, leaves, and living vines. Its eyes shine with wisdom and power, and it can manipulate nature to protect its domain. The Guardian is both a fierce protector and a benevolent spirit, defending the heart of the woods from those who threaten it.");
    }
    monsters[num_monsters++] = monster10;

    CharacterState *monster = &monsters[9];
    Room *room = &rooms[13]; // Assuming room numbers are 1-indexed

    // Add to the room's monster list if space allows
    if (room->num_monsters < MAX_MONSTERS) {
        room->monsters[room->num_monsters++] = monster;
    }
}

// Function to handle room change requests
void handle_change_room(ThreadArgs *args, uint16_t requested_room_number) {
    int client_fd = args->client_fd;
    CharacterState *player = find_pp_by_fd(client_fd);
    
    // Check if the requested room exists
    Room *new_room = &rooms[requested_room_number - 1];
    Room *old_room = &rooms[player->room_number - 1];

    if (new_room == NULL) {
        // Send an error if the room is invalid
        send_error_message(client_fd, 1, "Invalid room number.");
        return;
    }
    
    bool is_valid_connection = false;
    for (size_t i = 0; i < old_room->num_connections; i++) {
        if (old_room->connections[i]->room_number == requested_room_number) {
            is_valid_connection = true;
            break;
        }
    }

    if (!is_valid_connection) {
        send_error_message(client_fd, 1, "You cannot access that room from here.");
        return;
    }

    // Remove player from the current room
    remove_player_from_room(&rooms[player->room_number - 1], player); // Correct room reference

    // Update player's room number
    player->room_number = new_room->room_number;

    // Notify existing players in the old room about the player leaving
    for (size_t i = 0; i < old_room->num_players; i++) {
        if (old_room->players[i] != NULL && old_room->players[i] != player) {
            send_character_message(old_room->players[i]->client_fd, player, player->name, player->description);
        }
    }

    // Add player to the new room
    add_player_to_room(new_room, player);
    
    // Notify existing players in the new room about the new player
    for (size_t i = 0; i < new_room->num_players; i++) {
        if (new_room->players[i] != NULL && new_room->players[i] != player) {
            send_character_message(new_room->players[i]->client_fd, player, player->name, player->description);
        }
    }
    
    send_room_message(client_fd, new_room, player);

    // Send the updated CHARACTER message for the player
    send_character_message(client_fd, player, player->name, player->description);


    // Send CHARACTER messages for all players in the new room
    for (size_t i = 0; i < new_room->num_players; i++) {
        if (new_room->players[i] != NULL) {
            send_character_message(client_fd, new_room->players[i], new_room->players[i]->name, new_room->players[i]->description);
        }
    }

    // Send CHARACTER messages for all monsters in the new room
    for (size_t i = 0; i < new_room->num_monsters; i++) {
        if (new_room->monsters[i] != NULL) {
            send_character_message(client_fd, new_room->monsters[i], new_room->monsters[i]->name, new_room->monsters[i]->description);
        }
    }

    // Send CONNECTION messages for the new room
    for (size_t i = 0; i < new_room->num_connections; i++) {
        send_connection_message(client_fd, new_room->connections[i]);
    }
}

// Function for calculating damage upon fight request
void calculate_damage(CharacterState *attacker, CharacterState *defender) {
    if (!is_alive(attacker) || !is_alive(defender)) {
        printf("One of the characters is not alive.\n");
        return;
    }

    // Damage calculation logic
    int damage = attacker->attack - defender->defense;
    if (damage < 0) damage = 0;

    defender->health -= damage;

    // Update alive flag if health falls to 0 or below
    if (defender->health <= 0) {
        defender->flags &= ~0x80;
        defender->health = 0;
        printf("%s has been defeated! Alive: %d\n", defender->name, defender->flags & 0x80);
    }
}

// Function to handle fight request
void handle_fight(CharacterState *player, CharacterState *monster) {
    // Check if both characters are alive
    if (!is_alive(player)) {
        printf("%s cannot fight because they are not alive.\n", player->name);
        return;
    }
    
    if (!is_alive(monster)) {
        printf("%s cannot fight because they are not alive.\n", monster->name);
        return;
    }

    // Player attacks the monster
    printf("%s attacks %s!\n", player->name, monster->name);
    calculate_damage(player, monster);

    // Send character messages to other players in the room
    notify_players_in_room(player, monster);

    // Check if the monster is still alive
    if (!is_alive(monster)) {
        printf("%s has been defeated!\n", monster->name);
        return; // Monster has been defeated; exit the fight function
    }

    // Monster attacks the player
    printf("%s attacks %s!\n", monster->name, player->name);
    calculate_damage(monster, player);

    // Send character messages to other players in the room
    notify_players_in_room(player, monster);

    // Check if the player is still alive
    if (!is_alive(player)) {
        printf("%s has been defeated!\n", player->name);
        return; // Player has been defeated; exit the fight function
    }
}

// Function for sending CHARACTER messages to correct players in room
void notify_players_in_room(CharacterState *player, CharacterState *monster) {
    Room *room = &rooms[player->room_number - 1]; // Assuming room numbers are 1-indexed

    for (size_t i = 0; i < room->num_players; i++) {
        if (room->players[i] != NULL && room->players[i] != player) {
            // Send character update messages for both player and monster
            send_character_message(room->players[i]->client_fd, player, player->name, player->description);
            send_character_message(room->players[i]->client_fd, monster, monster->name, monster->description);
        }
    }
}

// Function for getting monster pointer based on a room
CharacterState *get_monster_in_room(int room_number, int monster_index) {
    // Check if the room number is valid
    if (room_number < 1 || room_number > MAX_ROOMS) {
        printf("Invalid room number.\n");
        return NULL;
    }

    Room *room = &rooms[room_number - 1]; // Access the room (1-indexed to 0-indexed)

    // Check if the monster index is valid
    if (monster_index < 0 || monster_index >= room->num_monsters) {
        printf("Invalid monster index.\n");
        return NULL;
    }

    // Return the requested monster
    return room->monsters[monster_index];
}

// Function for handling loot request
void handle_loot(ThreadArgs *args) {
    int client_fd = args->client_fd;
    uint8_t buffer[32];
    ssize_t bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);

    if (bytes_received <= 0) {
        if (bytes_received == 0) {
            printf("Client disconnected\n");
        } else {
            perror("recv");
        }
        return;
    }

    // Extract the target player's name (ensure null-termination)
    char target_name[NAME_LENGTH + 1];
    memcpy(target_name, buffer, NAME_LENGTH);
    target_name[NAME_LENGTH] = '\0';

    // Find the target character (monster or player) in the room
    Room *room = find_room_by_character(find_pp_by_fd(client_fd));
    if (room == NULL) {
        printf("Could not find the room for the character.\n");
        return;
    }

    CharacterState *target = NULL;

    // Check for dead monsters in the room (case insensitive)
    for (size_t i = 0; i < room->num_monsters; i++) {
        if (room->monsters[i] != NULL && strcasecmp(room->monsters[i]->name, target_name) == 0 && !is_alive(room->monsters[i])) {
            target = room->monsters[i];
            break;
        }
    }

    // Check for dead players in the room (case insensitive)
    if (target == NULL) {
        pthread_mutex_lock(&player_mutex);
        for (size_t i = 0; i < player_count; i++) {
            if (strcasecmp(global_players[i].name, target_name) == 0 && !is_alive(&global_players[i])) {
                target = &global_players[i];
                break;
            }
        }
        pthread_mutex_unlock(&player_mutex);
    }

    // If the target is found and is dead, loot their gold
    if (target != NULL) {
        // Add gold to the player's character state
        CharacterState *player = find_pp_by_fd(client_fd);
        player->gold += target->gold;

        // Send updated CHARACTER message to the player
        send_character_message(client_fd, player, player->name, player->description);

        printf("%s looted %d gold from %s.\n", player->name, target->gold, target_name);
        // Optionally, set the target's gold to 0
        target->gold = 0;
    } else {
        printf("No dead player or monster found with the name: %s\n", target_name);
        // Optionally send an error message back to the player
        send_error_message(client_fd, 6, "No dead target found to loot.");
    }
}

// Function for handling CHARACTER creation
void handle_character(ThreadArgs *args) {
    int client_fd = args->client_fd;
    CharacterState state = {0};
    state.description = NULL;
    state.description_received = 0;
    state.total_received = 0;
    state.client_fd = client_fd;

    uint16_t remaining_stat_limit = STAT_LIMIT_MAX;

    while (1) {
        uint8_t buffer[1024];
        uint8_t description_buffer[512];
        ssize_t bytes_received = recv(client_fd, buffer, 47, MSG_WAITALL);

        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                printf("Client disconnected\n");
            } else {
                perror("recv");
            }
            return;
        }

        // Handle the initial part of the message
        if (state.total_received == 0) {
            if (bytes_received < 47) { // Adjusted to 47 to align with the expected length
                printf("Received insufficient data: %ld bytes\n", bytes_received);
                continue;
            }

            // Now buffer[0] is not CHARACTER_TYPE, so we skip it
            memcpy(state.name, buffer, NAME_LENGTH);
            state.name[NAME_LENGTH] = '\0';
            state.flags = buffer[32];
            state.attack = buffer[33] | (buffer[34] << 8);
            state.defense = buffer[35] | (buffer[36] << 8);
            state.regen = buffer[37] | (buffer[38] << 8);
            state.health = (int16_t)(buffer[39] | (buffer[40] << 8));
            state.gold = buffer[41] | (buffer[42] << 8);
            state.room_number = buffer[43] | (buffer[44] << 8);
            state.description_length = buffer[45] | (buffer[46] << 8);
            state.total_received += 47;

            // Allocate memory for description
            state.description = (char *)malloc(state.description_length + 1);
            if (state.description == NULL) {
                perror("malloc");
                return;
            }

            // Check if description length is valid
            if (state.description_length > sizeof(description_buffer)) {
                fprintf(stderr, "Description length exceeds buffer size\n");
                free(state.description);
                return;
            }

            ssize_t desc_bytes_received = recv(client_fd, description_buffer, state.description_length, MSG_WAITALL);
            if (desc_bytes_received < 0) {
                perror("recv for description");
                free(state.description);
                return;
            }

            // Ensure we received the expected amount of data
            if (desc_bytes_received != state.description_length) {
                fprintf(stderr, "Received incorrect description length: expected %d, got %ld\n",
                        state.description_length, desc_bytes_received);
                free(state.description);
                return;
            }

            // Copy the description correctly
            memcpy(state.description, description_buffer, state.description_length);
            state.description[state.description_length] = '\0'; // Null-terminate


            // Validate individual stats
            uint16_t total_stats = state.attack + state.defense + state.regen;
            if (total_stats > remaining_stat_limit) {
                // Calculate exceeding amount
                uint16_t excess = total_stats - remaining_stat_limit;

                // Send error message
                char error_message[128];
                snprintf(error_message, sizeof(error_message), "Total stats exceed limit by %d", excess);
                send_error_message(client_fd, 4, error_message);

                // Await corrected stats
                int try_again = 0;
                while (try_again < 5) {
                    try_again++;
                    bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);
                    if (bytes_received <= 0) {
                        if (bytes_received == 0) {
                            printf("Client disconnected\n");
                        } else {
                            perror("recv");
                        }
                        return;
                    }

                    /*
                    if (buffer[0] != CHARACTER_TYPE) {
                        printf("Received message with unexpected type %d\n", buffer[0]);
                        continue;
                    }
                    */

                    // Extract corrected stats (again shifting indices by 1)
                    state.attack = buffer[34] | (buffer[35] << 8);
                    state.defense = buffer[36] | (buffer[37] << 8);
                    state.regen = buffer[38] | (buffer[39] << 8);

                    total_stats = state.attack + state.defense + state.regen;
                    if (total_stats <= remaining_stat_limit) {
                        break; // Valid stats
                    } else {
                        snprintf(error_message, sizeof(error_message), "Corrected stats still exceed limit by %d", total_stats - remaining_stat_limit);
                        send_error_message(client_fd, 4, error_message);
                    }
                }
                if(try_again >= 5){
                    handle_client_disconnection(client_fd);
                }
            }

            // Check for existing player with the same name
            CharacterState *existing_player = NULL;

            pthread_mutex_lock(&player_mutex);
            for (size_t i = 0; i < player_count; i++) {
                if (strcasecmp(global_players[i].name, state.name) == 0) {
                    existing_player = &global_players[i];
                    break;
                }
            }

            // If a match is found, reuse that player
            if (existing_player) {
                printf("Reusing existing character: %s\n", existing_player->name);

                // Assign the new client's state to the existing character
                existing_player->client_fd = client_fd; // Update client file descriptor
                existing_player->flags = 0xC8;     // Update flags

                // Free old description and assign new one
                free(existing_player->description);
                existing_player->description_length = state.description_length;
                existing_player->description = (char *)malloc(state.description_length + 1);
                if (existing_player->description != NULL) {
                    strcpy(existing_player->description, state.description);
                }

                pthread_mutex_unlock(&player_mutex);
                send_accept_message(client_fd, CHARACTER_TYPE);
                return; // Exit after updating existing player
            }

            // Initialize new character if no match is found
            if (player_count < MAX_PLAYERS) {
                // Set default values
                state.room_number = 1;  
                state.flags = 0xC8;     
                state.health = 100;     
                state.gold = 0;
                state.regen = 0;

                global_players[player_count++] = state; // Store the character state
            }

            pthread_mutex_unlock(&player_mutex);

            // Print valid character data
            printf("Received valid character data:\n");
            printf("Name: %s\n", state.name);
            printf("Flags: %02X\n", state.flags);
            printf("Attack: %d\n", state.attack);
            printf("Defense: %d\n", state.defense);
            printf("Regen: %d\n", state.regen);
            printf("Health: %d\n", state.health);
            printf("Gold: %d\n", state.gold);
            printf("Room Number: %d\n", state.room_number);
            printf("Description Length: %d\n", state.description_length);
            printf("Description: %s\n", state.description);

            // Send ACCEPT message after confirming valid stats
            send_accept_message(client_fd, CHARACTER_TYPE);
            break; // Exit the main loop after sending the accept message
        }
    }
}





int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);  // Ignore SIGPIPE signals

    struct sockaddr_in sad;
    if (argc > 1)
        sad.sin_port = htons(atoi(argv[1]));
    else
        sad.sin_port = htons(5014);
    sad.sin_addr.s_addr = INADDR_ANY;
    sad.sin_family = AF_INET;

    int skt = socket(AF_INET, SOCK_STREAM, 0);
    if (skt == -1) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    if (setsockopt(skt, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(skt);
        return 1;
    }

    if (bind(skt, (struct sockaddr *)(&sad), sizeof(struct sockaddr_in))) {
        perror("bind");
        close(skt);
        return 1;
    }
    
    if (listen(skt, MAX_CONNECTIONS)) {
        perror("listen");
        close(skt);
        return 1;
    }

    initialize_rooms();
    populate_rooms_with_monsters();

    while (1) {
        int client_fd;
        struct sockaddr_in client_address;
        socklen_t address_size = sizeof(struct sockaddr_in);
        
        client_fd = accept(skt, (struct sockaddr *)(&client_address), &address_size);
        if (client_fd == -1) {
            perror("accept");
            continue;  // Allow the server to accept new connections
        }

        printf("Connection made from address %s\n", inet_ntoa(client_address.sin_addr));
        send_version_message(client_fd);
        send_game_message(client_fd);

        // Check if we have reached the maximum thread limit
        if (thread_count >= MAX_CONNECTIONS) {
            fprintf(stderr, "Maximum number of clients reached.\n");
            close(client_fd);
            continue;
        }

        // Allocate memory for thread arguments
        ThreadArgs *args = (ThreadArgs *)malloc(sizeof(ThreadArgs));
        if (args == NULL) {
            perror("malloc");
            close(client_fd);
            continue;
        }

        args->client_fd = client_fd;

        
        // Store the thread args in the global array
        thread_args[thread_count++] = args; // Store the pointer

        // Create a new thread for handling the client
        pthread_t thread;
        if (pthread_create(&thread, NULL, client_handler, args) != 0) {
            perror("pthread_create");
            free(args); // Free the allocated memory if thread creation fails
            close(client_fd);
            continue;
        }
        pthread_detach(thread);
    }

    if (close(skt) == -1) {
        perror("close skt");
    }
    return 0;
}