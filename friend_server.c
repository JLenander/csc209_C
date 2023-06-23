#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "friends.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef PORT
	#define PORT 59211
#endif
#define DELIM " \n"
#define BUF_SIZE 256
#define INPUT_ARG_MAX_NUM 12
#define MAX_BACKLOG 5

// This struct forms a linked list structure where each item contains a User, a buffer
// exclusively for this user, an int keeping track of how many bytes are in the buffer
// and a sockfd for the current active connection for this user
// (or -1 if no active connection)
typedef struct client_connection {
    int sock_fd;
    char *buf;
    int in_buf;
    User *user;
    struct client_connection *next_client;
} Client;


/*
 * Send a message to the client with write. <message> must be terminated by a newline character.
 * Return 0 if the message was successful.
 * Return -1 if the client was closed (this function does not handle removing the client).
 */
int message_client(Client *client, char *message) {
    // Make a copy of the message that is guaranteed to be mutable for this function to use.
    char mutable_msg[strlen(message) + 1];
    strncpy(mutable_msg, message, strlen(message) + 1);

    // We need to send each line from list_users as a separate message to ensure we don't go past the buffer size.
    // Stores the index of the start of the line in mutable_msg (for multiple lines)
    int line_start = 0;
    // Stores the index of the newline character (the end of the line).
    int where = 0;
    while ((line_start < strlen(message) - 1) && (where = strchrnul(&mutable_msg[line_start], '\n') - mutable_msg) != 0) {
        // If the message to send does not have a terminating newline (which should not happen) truncate the last message to
        // add a newline character in.
        if (where >= strlen(message)) {
            fprintf(stderr, "[Server] ERROR message has no terminating newline, adjusting\n");
            where = strlen(message) - 1;
        }

        // Null terminate the message at the newline to separate it from the other lines
        // so string functions can be used below
        mutable_msg[where] = '\0';
        // line will contain the full line to send including the null terminator and the network newline
        char line[where - line_start + 3];
        strncpy(line, &mutable_msg[line_start], where - line_start);
        line[where - line_start] = '\r';
        line[where - line_start + 1] = '\n';
        line[where - line_start + 2] = '\0';

        // Write in a loop to ensure the message is written.
        int num_to_write = strlen(line);
        int num_wrote = 0;
        while ((num_wrote = write(client->sock_fd, &line[num_wrote], num_to_write)) < num_to_write) {
            if (num_wrote == -1) {
                // The client disconnected.
                return -1;
            }
            // Update what's left to write.
            num_to_write -= num_wrote;
        }

        // Set the start of the message to be the next character (start of another line or null terminator)
        if (where < strlen(message)) {
            line_start = where + 1;
        }
    }

    return 0;
}

/*
 * Returns a pointer to the Client with a user that has username <username> from the
 * linked list structure <client_list> or NULL if no such user exists.
 */
Client *find_client_by_username(char *username, Client *client_list) {
    while (client_list != NULL) {
        if (strcmp(client_list->user->name, username) == 0) {
            return client_list;
        }
        client_list = client_list->next_client;
    }

    return NULL;
}

/*
 * Returns a pointer to the Client with a sock_fd that equals <sock_fd> from the
 * linked list structure <client_list> or NULL if no such client exists.
 */
Client *find_client_by_sockfd(int sock_fd, Client *client_list) {
    while (client_list != NULL) {
        if (client_list->sock_fd == sock_fd) {
            return client_list;
        }
        client_list = client_list->next_client;
    }

    return NULL;
}

/*
 * Removes the client specified by the file descriptor <sock_fd> from the linked list
 * <client_list> as this client is no longer connected.
 * Does nothing if no client has <sock_fd> as their file descriptor.
 * <client_list> must not be NULL.
 * Returns the head of the client_list with the client removed.
 */
Client *remove_client(int sock_fd, Client *client_list) {
    if (client_list->sock_fd == sock_fd) {
        Client *new_head = client_list->next_client;

        // Free the string buffer for the client.
        free(client_list->buf);
        free(client_list);

        return new_head;
    }

    Client *prev = client_list;
    Client *curr = client_list->next_client;

    // Loop through to find the client with the sock_fd
    while (curr != NULL && curr->sock_fd != sock_fd) {
        prev = curr;
        curr = curr->next_client;
    }

    if (curr != NULL) {
        // Found the client at curr
        Client *client_to_remove = curr;
        prev->next_client = curr->next_client;

        free(client_to_remove->buf);
        free(client_to_remove);
    }

    return client_list;
}

/*
 * Send a message to all connected clients with a user with username <username>.
 * <client_list> is the head of the linked list client structure.
 * Returns the head of the linked list client structure in case any clients are closed.
 */
Client *message_to_users(char *username, Client *client_list, char *message) {
    Client *curr_client = client_list;

    while (curr_client != NULL) {
        if (curr_client->user != NULL
            && strcmp(curr_client->user->name, username) == 0
            && message_client(curr_client, message) == -1) {
            // We tried to send the message but the Client disconnected. Before we remove it, go to the next client.
            int disconnected_client_fd = curr_client->sock_fd;
            curr_client = curr_client->next_client;
            client_list = remove_client(disconnected_client_fd, client_list);
        } else {
            curr_client = curr_client->next_client;
        }
    }

    return client_list;
}

/*
 * Adds or retrieves the user with username <username> to the client specified by <client_fd>
 * If no user exists, creates a new user with <username>.
 * If a user exists with the username <username>, adds this user to the client.
 */
void add_user_to_client(char *username, int client_fd, Client *client_list, User **user_list_ptr) {
    Client *client = find_client_by_sockfd(client_fd, client_list);

    // Check if the username is within the limits
    if (strlen(username) >= MAX_NAME) {
        username[MAX_NAME - 1] = '\0';
        // Inform the user that their username was truncated.
        char truncated_msg[BUF_SIZE];
        snprintf(truncated_msg, BUF_SIZE, "Username too long, truncated to %d characters.\n", MAX_NAME - 1);
        if (message_client(client, truncated_msg) == -1) {
            remove_client(client->sock_fd, client_list);
            return;
        }
    }

    // Find the user from the list if it already exists.
    User *user = find_user(username, *user_list_ptr);

    // Create the User with the username <username> if it doesn't exist
    if (user == NULL) {
        if (create_user(username, user_list_ptr) != 0) {
            // We should never get here if the preconditions are respected.
            fprintf(stderr, "Create user failed\n");
            return;
        }
        user = find_user(username, *user_list_ptr);

		// Send a welcome message
		if (message_client(client, "Welcome!\n") == -1) {
            remove_client(client->sock_fd, client_list);
            return;
        }

    } else {
		// The user exists so all we have to do is print the "Welcome back" message
        if (message_client(client, "Welcome Back!\n") == -1) {
            remove_client(client->sock_fd, client_list);
            return;
        }
	}

    // Inform the client that they can write user commands now
    if (message_client(client, "You may enter user commands now:\n") == -1) {
        remove_client(client->sock_fd, client_list);
        return;
    }

    client->user = user;
}

/*
 * Adds a client to the tail of the linked list structure with no user (this function
 * is used for new connections before they have sent their username)
 * Returns the first client in the list
 */
Client *add_client(Client *client_list, int client_fd) {
    // Create the Client struct
    Client *new_client = malloc(sizeof(Client));
    if (new_client == NULL) {
        perror("Client struct malloc");
        exit(1);
    }

    // Initialize the struct values
    new_client->sock_fd = client_fd;
    new_client->buf = malloc(BUF_SIZE);
    if (new_client->buf == NULL) {
        perror("new client malloc");
        exit(1);
    }
    new_client->buf[0] = '\0';  // Ensure the buffer starts null-terminated.
    new_client->in_buf = 0;
    new_client->user = NULL;
    new_client->next_client = NULL;

    // If the list is empty, new_client is the head.
    if (client_list == NULL) {
        return new_client;
    }

    // Insert the new client at the tail of the linked list.
    Client *curr_client = client_list;
    while (curr_client->next_client != NULL) {
        curr_client = curr_client->next_client;
    }
    curr_client->next_client = new_client;
    return client_list;
}


/*
 * Accept a connection. Note that a new file descriptor is created for
 * communication with the client. The initial socket descriptor is used
 * to accept connections, but the new socket is used to communicate.
 * <client_fd> is set to the file descriptor of the new client or -1 if the client disconnected immediately
 * Return the head of the client list with the new client in it.
 */
Client *accept_connection(int fd, Client *client_list, int *client_fd) {
    *client_fd = accept(fd, NULL, NULL);
    if (*client_fd < 0) {
        perror("server: accept");
        close(fd);
        exit(1);
    }

    // Add a new empty client to the linked list structure.
    client_list = add_client(client_list, *client_fd);

    // Send the initial instruction message to ask for their username to the client
    if (message_client(find_client_by_sockfd(*client_fd, client_list), "Please enter your username:\n")) {
        // The client disconnected.
        remove_client(*client_fd, client_list);
        return client_list;
    }

    return client_list;
}

/*
 * Search the first n characters of buf for a network newline (\r\n).
 * Return one plus the index of the '\n' of the first network newline,
 * or -1 if no network newline is found. The return value is the index into buf
 * where the current line ends.
 */
int find_network_newline(const char *buf, int n) {
    for (int i = 0; i < n; i++) {
        // The first network newline will be a \n preceded by a \r
        if (buf[i] == '\n') {
            if (i > 0 && buf[i - 1] == '\r') {
                // Found the network newline
                return i + 1;
            }
        }
    }
    return -1;
}

/*
 * Returns a dynamically allocated string containing the string <msg>.
 * <msg> must be a null_terminated string.
 */
char *alloc_str(char *msg) {
	char *return_msg = malloc(strlen(msg) + 1);
	if (return_msg == NULL) {
		perror("Return message malloc");
		exit(1);
	}

	// Use of strcpy here is safe due to specific malloc before and precondition.
	strcpy(return_msg, msg);

	return return_msg;
}

/*
 * Tokenize the string stored in cmd.
 * Return the number of tokens, and store the tokens in cmd_argv.
 * If there are too many arguments, return -1.
 */
int tokenize(char *cmd, char **cmd_argv) {
	int cmd_argc = 0;
	char *next_token = strtok(cmd, DELIM);
	while (next_token != NULL) {
		if (cmd_argc >= INPUT_ARG_MAX_NUM - 1) {
			// There are too many arguments.
			cmd_argc = 0;
			return -1;
			break;
		}
		cmd_argv[cmd_argc] = next_token;
		cmd_argc++;
		next_token = strtok(NULL, DELIM);
	}

	return cmd_argc;
}

/*
 * Read and process commands
 * <first_user> is the user who sent the command (the first user for certain operations like post)
 * <user_list_ptr> is a list of pointers to users.
 * <return_msg> is set to point to a null_terminated string with the message associated or an empty string
 * if there is no message. (The returned string is dynamically allocated if it isn't empty).
 * with the result of the action (for failure and success).
 * Return:  -2 for quit command
 *          -1 for an error
 *          0 otherwise
 */
int process_args(int cmd_argc, char **cmd_argv, User *first_user, User **user_list_ptr, Client *client_list, char **return_msg) {
	User *user_list = *user_list_ptr;

	if (cmd_argc <= 0) {
		return 0;
	} else if (strcmp(cmd_argv[0], "quit") == 0 && cmd_argc == 1) {
		return -2;
	} else if (strcmp(cmd_argv[0], "list_users") == 0 && cmd_argc == 1) {
        *return_msg = list_users(user_list);
	} else if (strcmp(cmd_argv[0], "make_friends") == 0 && cmd_argc == 2) {
        // Setup the messages notifying the users in the success place.
        // We cannot place this in the switch statement as the case is a label
        // so we set it up here for use in case 0.
        char new_friend_author_msg[BUF_SIZE];
        snprintf(new_friend_author_msg, BUF_SIZE, "You are now friends with %s!\n", cmd_argv[1]);
        char new_friend_target_msg[BUF_SIZE];
        snprintf(new_friend_target_msg, BUF_SIZE, "You are now friends with %s!\n", first_user->name);
		switch (make_friends(first_user->name, cmd_argv[1], user_list)) {
            case 0:
                // Success, notify the new friend if they are online
                message_to_users(cmd_argv[1], client_list, new_friend_target_msg);
                message_to_users(first_user->name, client_list, new_friend_author_msg);
                break;
			case 1:
				*return_msg = alloc_str("users are already friends\n");
				return -1;
				break;
			case 2:
				*return_msg = alloc_str("at least one user you entered has the max number of friends\n");
				return -1;
				break;
			case 3:
				*return_msg = alloc_str("you must enter two different users\n");
				return -1;
				break;
			case 4:
				*return_msg = alloc_str("at least one user you entered does not exist\n");
				return -1;
				break;
		}
	} else if (strcmp(cmd_argv[0], "post") == 0 && cmd_argc >= 3) {
		// first determine how long a string we need
		int space_needed = 0;
		for (int i = 2; i < cmd_argc; i++) {
			space_needed += strlen(cmd_argv[i]) + 1;
		}

		// allocate the space
		char *contents = malloc(space_needed);
		if (contents == NULL) {
			perror("malloc");
			exit(1);
		}

		// copy in the bits to make a single string
		strcpy(contents, cmd_argv[2]);
		for (int i = 3; i < cmd_argc; i++) {
			strcat(contents, " ");
			strcat(contents, cmd_argv[i]);
		}

		User *author = first_user;
		User *target = find_user(cmd_argv[1], user_list);

        // Setup the message notifying the other user of the post's contents
        // We cannot place this in the switch statement as the case is a label
        // so we set it up here for use in case 0.
        char post_msg[BUF_SIZE];
        snprintf(post_msg, BUF_SIZE, "Message from %s: %s\n", author->name, contents);
		switch (make_post(author, target, contents)) {
            case 0:
                // Success, notify the target of the message if they are online
                message_to_users(target->name, client_list, post_msg);
                break;
			case 1:
				// We no longer need the contents so free it on error.
				free(contents);
				*return_msg = alloc_str("the users are not friends\n");
				return -1;
				break;
			case 2:
				// We no longer need the contents so free it on error.
				free(contents);
				*return_msg = alloc_str("at least one user you entered does not exist\n");
				return -1;
				break;
		}
	} else if (strcmp(cmd_argv[0], "profile") == 0 && cmd_argc == 2) {
		User *user = find_user(cmd_argv[1], user_list);
		if (user == NULL) {
			*return_msg = alloc_str("user not found\n");
			return -1;
		} else {
			*return_msg = print_user(user);
		}
	} else {
		*return_msg = alloc_str("Incorrect syntax\n");
		return -1;
	}

	// No error occurred, no message to print.
	return 0;
}

/*
 * Read a message from the client with sock_fd <fd> and set the username or process the arguments.
 * This client should be ready to be read (checked with select before calling this function).
 * Return the fd if it has been closed or 0 otherwise.
 *
 * There are two different types of input that we could
 * receive: a username, or a command. If we have not yet received
 * the username, then we should copy buf to username.  Otherwise, the
 * input will be a command to process.
 */
int read_from(int fd, Client *client_list, User **user_list) {
    // Ensure buffer starts null terminated
    Client *client = find_client_by_sockfd(fd, client_list);

    // There may still be stuff in the buffer so set the room and after pointer variables appropriately
    int room = BUF_SIZE - client->in_buf;
    char *after = &(client->buf[client->in_buf]);

    int num_read;
    num_read = read(fd, after, room);
    if (num_read == 0) {
        // The client disconnected
        printf("[Server] Discovered client %d is closed\n", client->sock_fd);
        return client->sock_fd;
    }

    // Update inbuf based on how many bytes were just read
    client->in_buf += num_read;
    room -= num_read;

    int where;

    // Use a loop to read continuously until we get a network newline. The loop structure handles having multiple
    // lines in the buffer.
    // Update where to be the location after this network newline and process the input(s) accordingly.
    while ((where = find_network_newline(client->buf, client->in_buf)) > 0) {
        // Where is now the index into buf immediately after the first network newline.
        // Null terminate the buffer at the carriage return part of the network newline.
        // (where is guaranteed to be >= 2 as a network newline is two characters).
        client->buf[where - 2] = '\0';

        // Check if this is a username or a command
        if (client->user->name == NULL) {
            if (num_read == 0) {
                // Client was closed
                return fd;
            }

            // This call either identifies the user from existing users or adds a new user to the user_list
            add_user_to_client(client->buf, client->sock_fd, client_list, user_list);

            // Server message acknowledging new connection
            printf("[Server] User at %d now has username %s\n", fd, client->user->name);
        } else {
            // The message we send back to the client.
            char *return_msg = "";
            char *cmd_argv[INPUT_ARG_MAX_NUM];
            int cmd_argc = tokenize(client->buf, cmd_argv);

            if (process_args(cmd_argc, cmd_argv, client->user, user_list, client_list, &return_msg) == -2) {
                // The user has quit by sending the quit command.
                printf("[Server] User at %d has quit using quit command\n", fd);
                return fd;
            } else {
                // The command was processed. Check if there is a return message.
                if (cmd_argc < 0) {
                    // In the case that there were too many arguments in tokenize, process_args will return 0
                    // without processing any of the commands because cmd_argc would have been set to -1.
                    // This warrants this error return message to the user.
                    return_msg = alloc_str("Too many arguments!\n");
                }

                if (return_msg[0] != '\0') {
                    // Send the non-empty return message back to the client.
                    if (message_client(client, return_msg) == -1) {
                        // Free the return message as we have sent it.
                        free(return_msg);
                        return fd;
                    }
                    // Free the return message as we have sent it.
                    free(return_msg);
                }
            }

            // Server message to acknowledge that we processed a command from the user.
            printf("[Server] Processed command from User %d\n", fd);
        }

        // The input has been processed, now update the unprocessed contents of the buffer to the
        // beginning so it can be processed
        memmove(client->buf, &client->buf[where], BUF_SIZE - where);
        client->in_buf = client->in_buf - where;
    }

    // Now that every full line in the buffer has been processed, update after and room
    // for any more partial reads.
    after = &client->buf[client->in_buf];
    room = BUF_SIZE - client->in_buf;

    // No more reads from the client.
    return 0;
}

int main() {
    // Create the socket FD.
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("server: socket");
        exit(1);
    }

    // Set information about the port (and IP) we want to be connected to.
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;

	// The following lines ensure the port is released as soon as the process terminates
	int on = 1;
	int status = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (const char *) &on, sizeof(on));
	if (status == -1) {
		perror("setsockopt -- REUSEADDR");
	}

    // This should always be zero. On some systems, it won't error if you
    // forget, but on others, you'll get mysterious errors. So zero it.
    memset(&server.sin_zero, 0, 8);

    // Bind the selected port to the socket.
    if (bind(sock_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("server: bind");
        close(sock_fd);
        exit(1);
    }

    // Announce willingness to accept connections on this socket.
    if (listen(sock_fd, MAX_BACKLOG) < 0) {
        perror("server: listen");
        close(sock_fd);
        exit(1);
    }

    // The client accept - message accept loop. First, we prepare to listen to multiple
    // file descriptors by initializing a set of file descriptors.
    int max_fd = sock_fd;
    fd_set all_fds;
    FD_ZERO(&all_fds);
    FD_SET(sock_fd, &all_fds);

    // Setup the list of clients
    Client *client_list = NULL;
    // Setup the list of users
    User *user_list = NULL;

    while (1) {
        // select updates the fd_set it receives, so we always use a copy and retain the original.
        fd_set listen_fds = all_fds;
        if (select(max_fd + 1, &listen_fds, NULL, NULL, NULL) == -1) {
            perror("server: select");
            exit(1);
        }

        // Is it the original socket? Create a new connection ...
        if (FD_ISSET(sock_fd, &listen_fds)) {
            int client_fd;
            client_list = accept_connection(sock_fd, client_list, &client_fd);
            if (client_fd == -1) {
                // Client closed the connection immediately
                continue;
            }
            if (client_fd > max_fd) {
                max_fd = client_fd;
            }
            FD_SET(client_fd, &all_fds);
            printf("[Server] Accepted connection\n");
        }

        // Check the clients for if they have reads available.
        Client *curr_client = client_list;
        while (curr_client != NULL) {
            if (FD_ISSET(curr_client->sock_fd, &listen_fds)) {
                // Note: never reduces max_fd
                int client_closed = read_from(curr_client->sock_fd, client_list, &user_list);
                if (client_closed > 0) {
                    FD_CLR(client_closed, &all_fds);
                    printf("[Server] Client %d disconnected\n", client_closed);
                    // Remove the struct from the linked list of active client connections
                    client_list = remove_client(client_closed, client_list);
                }
            }
            curr_client = curr_client->next_client;
        }
    }

    // Should never get here.
	return 1;
}
