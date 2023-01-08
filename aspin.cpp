#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define PS1 "~> "
#define BUFFER_SIZE 1024
#define ALIAS_USAGE \
    "Usage of alias:\n" \
    "alias                      - Display a list of all aliases\n" \
    "alias alias_name='command' - Add a new alias\n" \
    "alias -r alias_name        - Remove a single alias\n" \
    "alias -c                   - Remove all aliases" \

typedef struct Alias alias_t;
typedef struct Alias* alias_ptr_t;

// singly linked list to hold a list of alias names and commands
struct Alias
{
    char* name;
    char* command;
    alias_ptr_t next;
};

// returns a pointer to a heap allocated alias struct
// filled with a copy of the passed name and command
alias_ptr_t alias_create(char* name, char* command)
{
    alias_ptr_t new_alias_ptr = (alias_ptr_t) malloc(sizeof(alias_t));

    *new_alias_ptr =
        (alias_t) {
            .name = name,
            .command = command,
            .next = NULL
        };

    return new_alias_ptr;
}

// frees the particular alias struct
void alias_free(alias_ptr_t alias_ptr)
{
    if(alias_ptr != NULL)
    {
        free(alias_ptr->name);
        free(alias_ptr->command);

        *alias_ptr =
            (alias_t) {
                .name = NULL,
                .command = NULL,
                .next = NULL
            };

        free(alias_ptr);
    }
}

// recursively frees the entire alias data structure with the
// passed argument as the head of the list
alias_ptr_t alias_destroy(alias_ptr_t alias_ptr)
{
    if(alias_ptr)
    {
        alias_destroy(alias_ptr->next);
        alias_free(alias_ptr);
    }

    return NULL;
}

// removes elements from the alias list that have a matching name
// and returns the new head of the list
alias_ptr_t alias_remove(alias_ptr_t alias_ptr, const char* name)
{
    if(alias_ptr == NULL)
    {
        // empty list
        return NULL;
    }

    if(strcmp(alias_ptr->name, name) == 0)
    {
        // first element matches
        alias_ptr_t next = alias_ptr->next;
        alias_free(alias_ptr);
        return next;
    }

    // removing from the middle to the end of the list
    alias_ptr_t iterator = alias_ptr;

    while(iterator->next != NULL)
    {
        if(strcmp(iterator->next->name, name) == 0)
        {
            alias_ptr_t next = iterator->next->next;
            alias_free(iterator->next);
            iterator->next = next;
            break;
        }

        iterator = iterator->next;
    }

    return alias_ptr;
}

// adds a new (name, command) pair to the list of aliases
// by first removing any element with the passed name and
// returns the new head of the list of aliases
alias_ptr_t alias_add(alias_ptr_t alias_ptr, const char* name, const char* command)
{
    alias_ptr = alias_remove(alias_ptr, name);

    alias_ptr_t new_alias_ptr = alias_create(strdup(name), strdup(command));

    if(alias_ptr == NULL)
    {
        // list was empty
        return new_alias_ptr;
    }

    // going to the end of the alias list
    alias_ptr_t end_ptr = alias_ptr;
    while(end_ptr->next)
        end_ptr = end_ptr->next;

    // adding new_alias_ptr to the end
    end_ptr->next = new_alias_ptr;

    return alias_ptr;
}

// recursively displays the entire alias data structure
void alias_display(const alias_ptr_t alias_ptr)
{
    if(alias_ptr)
    {
        printf("%s=\"%s\"\n", alias_ptr->name, alias_ptr->command);
        alias_display(alias_ptr->next);
    }
}

// recursively searches for a command with the passed name
// in the entire alias data structure and returns NULL if
// not found
char* alias_query(const alias_ptr_t alias_ptr, const char* name)
{
    if(alias_ptr == NULL)
    {
        // not found
        return NULL;
    }

    if(strcmp(alias_ptr->name, name) == 0)
    {
        // match found
        return alias_ptr->command;
    }

    // search in the rest
    return alias_query(alias_ptr->next, name);
}

// executes non-alias-prefixed commands using system
void execute_other_command(char* command, const alias_ptr_t alias_ptr)
{
    char* query = alias_query(alias_ptr, command);

    if(query != NULL)
    {
        command = query;
    }

    system(command);
}

// executes alias commands
alias_ptr_t execute_alias_command(char* command, alias_ptr_t alias_ptr)
{
    bool incorrect_usage = false;

    if(strncmp(command, "alias -", strlen("alias -")) == 0)
    {
        // alias with options
        if(command[7] == 'c')
        {
            // remove all aliases
            alias_ptr = alias_destroy(alias_ptr);
        }
        else if(command[7] == 'r')
        {
            // remove a single alias
            char alias_name[BUFFER_SIZE];
            sscanf(command, "alias -r %s", alias_name);
            alias_ptr = alias_remove(alias_ptr, alias_name);
        }
        else
        {
            incorrect_usage = true;
        }
    }
    else if(command[5] != '\0')
    {
        // set alias
        char* alias_name;
        char* alias_command;

        // start of alias name
        alias_name = command + 6;

        // finding assignment operator
        char* iterator = alias_name + 1;
        bool assignment_found = false;
        while(*iterator != '\0')
        {
            if(*iterator == '=')
            {
                assignment_found = true;
                break;
            }

            ++iterator;
        }

        if(assignment_found)
        {
            *iterator = '\0'; // replacing assignment operator with '\0'
            ++iterator;

            // quote at start of alias command
            if(*iterator == '\'')
            {
                alias_command = ++iterator;

                // finding ending quote after alias command
                bool quote_found = false;
                while(*iterator != '\0')
                {
                    if(*iterator == '\'')
                    {
                        quote_found = true;
                        break;
                    }

                    ++iterator;
                }

                if(quote_found)
                {
                    *iterator = '\0'; // replacing quote with '\0'

                    // adding alias
                    alias_ptr = alias_add(alias_ptr, alias_name, alias_command);
                }
                else
                {
                    incorrect_usage = true;
                }
            }
            else
            {
                incorrect_usage = true;
            }
        }
        else
        {
            incorrect_usage = true;
        }
    }
    else
    {
        // display aliases
        alias_display(alias_ptr);
    }

    if(incorrect_usage)
    {
        // incorrect usage
        puts("Incorrect usage.");
        puts(ALIAS_USAGE);
    }

    return alias_ptr;
}

int main()
{
    // declaring the alias
    alias_ptr_t alias_ptr = NULL;

    // declaring the input buffer
    char command[BUFFER_SIZE];

    // shell loop
    for(;;)
    {
        // printing shell prefix
        printf(PS1);

        // reading in the command
        if(fgets(command, BUFFER_SIZE, stdin) == NULL)
        {
            break;
        }

        // removing newline
        command[strlen(command) - 1] = '\0';

        // routing commands
        if(strncmp(command, "alias", strlen("alias")) == 0)
        {
            alias_ptr = execute_alias_command(command, alias_ptr);
        }
        else
        {
            execute_other_command(command, alias_ptr);
        }
    }

    // destroying the alias
    alias_ptr = alias_destroy(alias_ptr);

    return 0;
}
