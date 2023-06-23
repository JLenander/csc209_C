#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "friends.h"

#define INPUT_BUFFER_SIZE 256
#define INPUT_ARG_MAX_NUM 12
#define DELIM " \n"


/* 
 * Print a formatted error message to stderr.
 */
void error(char *msg) {
    fprintf(stderr, "Error: %s\n", msg);
}

/* 
 * Read and process commands
 * Return:  -1 for quit command
 *          0 otherwise
 */
int process_args(int cmd_argc, char **cmd_argv, User **user_list_ptr) {
    User *user_list = *user_list_ptr;

    if (cmd_argc <= 0) {
        return 0;
    } else if (strcmp(cmd_argv[0], "quit") == 0 && cmd_argc == 1) {
        return -1;
    } else if (strcmp(cmd_argv[0], "add_user") == 0 && cmd_argc == 2) {
        switch (create_user(cmd_argv[1], user_list_ptr)) {
            case 1:
                error("user by this name already exists");
                break;
            case 2:
                error("username is too long");
                break;
        }

    } else if (strcmp(cmd_argv[0], "list_users") == 0 && cmd_argc == 1) {
		char *buf = list_users(user_list);
		printf("%s", buf);
		free(buf);
    } else if (strcmp(cmd_argv[0], "make_friends") == 0 && cmd_argc == 3) {
        switch (make_friends(cmd_argv[1], cmd_argv[2], user_list)) {
            case 1:
                error("users are already friends");
                break;
            case 2:
                error("at least one user you entered has the max number of friends");
                break;
            case 3:
                error("you must enter two different users");
                break;
            case 4:
                error("at least one user you entered does not exist");
                break;
        }
    } else if (strcmp(cmd_argv[0], "post") == 0 && cmd_argc >= 4) {
        // first determine how long a string we need
        int space_needed = 0;
        for (int i = 3; i < cmd_argc; i++) {
            space_needed += strlen(cmd_argv[i]) + 1;
        }

        // allocate the space
        char *contents = malloc(space_needed);
        if (contents == NULL) {
            perror("malloc");
            exit(1);
        }

        // copy in the bits to make a single string
        strcpy(contents, cmd_argv[3]);
        for (int i = 4; i < cmd_argc; i++) {
            strcat(contents, " ");
            strcat(contents, cmd_argv[i]);
        }

        User *author = find_user(cmd_argv[1], user_list);
        User *target = find_user(cmd_argv[2], user_list);
        switch (make_post(author, target, contents)) {
            case 1:
                error("the users are not friends");
                break;
            case 2:
                error("at least one user you entered does not exist");
                break;
        }
    } else if (strcmp(cmd_argv[0], "profile") == 0 && cmd_argc == 2) {
        User *user = find_user(cmd_argv[1], user_list);
		if (user == NULL) {
			error("user not found");
		} else {
			char *buf = print_user(user);
			printf("%s", buf);
			free(buf);
		}
    } else {
        error("Incorrect syntax");
    }
    return 0;
}


/*
 * Tokenize the string stored in cmd.
 * Return the number of tokens, and store the tokens in cmd_argv.
 */
int tokenize(char *cmd, char **cmd_argv) {
    int cmd_argc = 0;
    char *next_token = strtok(cmd, DELIM);    
    while (next_token != NULL) {
        if (cmd_argc >= INPUT_ARG_MAX_NUM - 1) {
            error("Too many arguments!");
            cmd_argc = 0;
            break;
        }
        cmd_argv[cmd_argc] = next_token;
        cmd_argc++;
        next_token = strtok(NULL, DELIM);
    }

    return cmd_argc;
}


int main(int argc, char* argv[]) {
    int batch_mode = (argc == 2);
    char input[INPUT_BUFFER_SIZE];
    FILE *input_stream;

    // Create the heads of the empty data structure
    User *user_list = NULL;

    if (batch_mode) {
        input_stream = fopen(argv[1], "r");
        if (input_stream == NULL) {
            perror("Error opening file");
            exit(1);
        }
    } else {
        // interactive mode 
        input_stream = stdin;
    }

    printf("Welcome to FriendMe! (Local version)\nPlease type a command:\n> ");
    
    while (fgets(input, INPUT_BUFFER_SIZE, input_stream) != NULL) {
        // only echo the line in batch mode since in interactive mode the user
        // just typed the line
        if (batch_mode) {
            printf("%s", input);
        }

        char *cmd_argv[INPUT_ARG_MAX_NUM];
        int cmd_argc = tokenize(input, cmd_argv);

        if (cmd_argc > 0 && process_args(cmd_argc, cmd_argv, &user_list) == -1) {
            break; // can only reach if quit command was entered
        }

        printf("> ");
    }

    if (batch_mode) {
        fclose(input_stream);
    }

    return 0;
 }
