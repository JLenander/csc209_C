#include "friends.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


/*
 * Create a new user with the given name.  Insert it at the tail of the list
 * of users whose head is pointed to by *user_ptr_add.
 *
 * Return:
 *   - 0 on success.
 *   - 1 if a user by this name already exists in this list.
 *   - 2 if the given name cannot fit in the 'name' array
 *       (don't forget about the null terminator).
 */
int create_user(const char *name, User **user_ptr_add) {
    if (strlen(name) >= MAX_NAME) {
        return 2;
    }

    User *new_user = malloc(sizeof(User));
    if (new_user == NULL) {
        perror("malloc");
        exit(1);
    }
    strncpy(new_user->name, name, MAX_NAME); // name has max length MAX_NAME - 1

    for (int i = 0; i < MAX_NAME; i++) {
        new_user->profile_pic[i] = '\0';
    }

    new_user->first_post = NULL;
    new_user->next = NULL;
    for (int i = 0; i < MAX_FRIENDS; i++) {
        new_user->friends[i] = NULL;
    }

    // Add user to list
    User *prev = NULL;
    User *curr = *user_ptr_add;
    while (curr != NULL && strcmp(curr->name, name) != 0) {
        prev = curr;
        curr = curr->next;
    }

    if (*user_ptr_add == NULL) {
        *user_ptr_add = new_user;
        return 0;
    } else if (curr != NULL) {
        free(new_user);
        return 1;
    } else {
        prev->next = new_user;
        return 0;
    }
}


/*
 * Return a pointer to the user with this name in
 * the list starting with head. Return NULL if no such user exists.
 *
 * NOTE: You'll likely need to cast a (const User *) to a (User *)
 * to satisfy the prototype without warnings.
 */
User *find_user(const char *name, const User *head) {
    while (head != NULL && strcmp(name, head->name) != 0) {
        head = head->next;
    }

    return (User *)head;
}


/*
 * Return the usernames of all users in the list starting at curr.
 * The string returned will list the users one per line.
 */
char *list_users(const User *curr) {
    char *list_header = "User List\n";
	// First, determine the size of the string we need.
	int str_size = strlen(list_header);
	const User *curr_user = curr;
    while (curr_user != NULL) {
		str_size += 2 + strlen(curr_user->name);  // Account for tab and newline characters with the +2
        curr_user = curr_user->next;
    }
	str_size += 1;  // Account for the null terminator

	// Now construct the string
	char *user_list_str = malloc(str_size);
	if (user_list_str == NULL) {
		perror("user list malloc");
		exit(1);
	}

	// Copy over the heading first. This also fills the rest of user_list with the null terminator
	strncpy(user_list_str, list_header, str_size);
	while (curr != NULL) {
		// Use of strcat is safe here because we've already looped through these strings and counted
		// the number of bytes total so no buffer overruns are possible here.
		// Properly using strncat here would mean keeping track of the remaining size of the buffer which is
		// inefficient as we have already counted so strcat is used here instead.

		strcat(user_list_str, "\t");
		strcat(user_list_str, curr->name);
		strcat(user_list_str, "\n");
		curr = curr->next;
	}
	// strncpy null terminates the string but we verify this anyways
	user_list_str[str_size - 1] = '\0';

	return user_list_str;
}


/*
 * Make two users friends with each other.  This is symmetric - a pointer to
 * each user must be stored in the 'friends' array of the other.
 *
 * New friends must be added in the first empty spot in the 'friends' array.
 *
 * Return:
 *   - 0 on success.
 *   - 1 if the two users are already friends.
 *   - 2 if the users are not already friends, but at least one already has
 *     MAX_FRIENDS friends.
 *   - 3 if the same user is passed in twice.
 *   - 4 if at least one user does not exist.
 *
 * Do not modify either user if the result is a failure.
 * NOTE: If multiple errors apply, return the *largest* error code that applies.
 */
int make_friends(const char *name1, const char *name2, User *head) {
    User *user1 = find_user(name1, head);
    User *user2 = find_user(name2, head);

    if (user1 == NULL || user2 == NULL) {
        return 4;
    } else if (user1 == user2) { // Same user
        return 3;
    }

    int i, j;
    for (i = 0; i < MAX_FRIENDS; i++) {
        if (user1->friends[i] == NULL) { // Empty spot
            break;
        } else if (user1->friends[i] == user2) { // Already friends.
            return 1;
        }
    }

    for (j = 0; j < MAX_FRIENDS; j++) {
        if (user2->friends[j] == NULL) { // Empty spot
            break;
        }
    }

    if (i == MAX_FRIENDS || j == MAX_FRIENDS) { // Too many friends.
        return 2;
    }

    user1->friends[i] = user2;
    user2->friends[j] = user1;
    return 0;
}


/*
 * Return a string representing the post <post>.
 * Use localtime to identify the time and date.
 * <post> must not be NULL.
 */
char *print_post(const Post *post) {
	// Determine the size of the string we need
	int str_size = 0;
	// +7 accounts for the "From: " and the newline
	str_size += strlen(post->author) + 7;
	// +7 accounts for the "Date: " and the newline
	str_size += strlen(asctime(localtime(post->date))) + 7;
	// +1 accounts for the newline
	str_size += strlen(post->contents) + 1;
	str_size += 1;  // Account for null terminator

	// Allocate space for string
	char *post_str = malloc(str_size);
	if (post_str == NULL) {
		perror("post malloc");
		exit(1);
	}

    // Add items to the string
	snprintf(post_str,
			 str_size,
			 "From: %s\nDate: %s\n%s\n",
             post->author,
             asctime(localtime(post->date)),
             post->contents);

	return post_str;
}


/*
 * Return a string representing a user profile.
 * For an example of the required output format, see the example output
 * linked from the handout.
 * <user> must not be NULL.
 */
char *print_user(const User *user) {
	// Declare a few known strings to minimize error in counting
	// The string used to separate different parts of the profile
	char *separator = "------------------------------------------\n";
	int sep_size = strlen(separator);
	char *friends_list_header = "Friends:\n";
	char *post_list_header = "Posts:\n";
	char *post_separator = "\n===\n\n";

	// Determine the size of the string we need
	int str_size = 0;
	// +8 accounts for "Name: " and two newlines
	str_size += strlen(user->name) + 8;
	str_size += sep_size;
	str_size += strlen(friends_list_header);
	// Loop through and count the size of all of the user's friend's names
	// plus an additional character for the newline
	for (int i = 0; i < MAX_FRIENDS && user->friends[i] != NULL; i++) {
		str_size += strlen(user->friends[i]->name) + 1;
	}
	str_size += sep_size;
	str_size += strlen(post_list_header);
	// Loop through and count the size of the posts
	const Post *curr = user->first_post;
	while (curr != NULL) {
		char *post_str = print_post(curr);
		str_size += strlen(post_str);
		curr = curr->next;
		if (curr != NULL) { // Only add the separator if there is another post
			str_size += strlen(post_separator);
		}
		// Free the post string after counting the length instead of trying to keep track of all
        // of the posts here. print_post should not differ from call to call with the same args.
		free(post_str);
	}
	str_size += sep_size;
	str_size += 1;  // Account for the null terminator.

	// Allocate space for the string
	char *profile_str = malloc(str_size);
	if (profile_str == NULL) {
		perror("User Profile malloc");
		exit(1);
	}

	// Now construct the string. strcat is safe here as we have counted the size of the strings already
	// and we know they are null terminated. Properly using strncat here would mean keeping track of the remaining
	// size of the buffer which is inefficient as we have already counted so strcat is used here instead.
    // Add the name
    snprintf(profile_str, str_size, "Name: %s\n\n", user->name);
	strcat(profile_str, separator);

    // Add the friend list.
    strcat(profile_str, friends_list_header);
    for (int i = 0; i < MAX_FRIENDS && user->friends[i] != NULL; i++) {
        strcat(profile_str, user->friends[i]->name);
		strcat(profile_str, "\n");
    }
	strcat(profile_str, separator);

    // Add the post list.
    strcat(profile_str, post_list_header);
    curr = user->first_post;  // curr is first defined above when calculating the string size
    while (curr != NULL) {
        strcat(profile_str, print_post(curr));
        curr = curr->next;
        if (curr != NULL) {
            strcat(profile_str, post_separator);
        }
    }
	strcat(profile_str, separator);

	// profile_str should be null terminated from the last call to strcat but ensure this happens anyways
	profile_str[str_size - 1] = '\0';

	return profile_str;
}


/*
 * Make a new post from 'author' to the 'target' user,
 * containing the given contents, IF the users are friends.
 *
 * Insert the new post at the *front* of the user's list of posts.
 *
 * Use the 'time' function to store the current time.
 *
 * 'contents' is a pointer to heap-allocated memory - you do not need
 * to allocate more memory to store the contents of the post.
 *
 * Return:
 *   - 0 on success
 *   - 1 if users exist but are not friends
 *   - 2 if either User pointer is NULL
 */
int make_post(const User *author, User *target, char *contents) {
    if (target == NULL || author == NULL) {
        return 2;
    }

    int friends = 0;
    for (int i = 0; i < MAX_FRIENDS && target->friends[i] != NULL; i++) {
        if (strcmp(target->friends[i]->name, author->name) == 0) {
            friends = 1;
            break;
        }
    }

    if (friends == 0) {
        return 1;
    }

    // Create post
    Post *new_post = malloc(sizeof(Post));
    if (new_post == NULL) {
        perror("malloc");
        exit(1);
    }
    strncpy(new_post->author, author->name, MAX_NAME);
    new_post->contents = contents;
    new_post->date = malloc(sizeof(time_t));
    if (new_post->date == NULL) {
        perror("malloc");
        exit(1);
    }
    time(new_post->date);
    new_post->next = target->first_post;
    target->first_post = new_post;

    return 0;
}

