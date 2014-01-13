#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <pwd.h>

#include "mysh.h"

#define ASCII_NUL '\0'
#define MAX_INPUT_LENGTH 1024

int main() {
    char curr_path[PATH_MAX];
    char curr_user[LOGIN_NAME_MAX];
    struct passwd *pw;
    char *homedir;

    char input[MAX_INPUT_LENGTH];
    char **tokenized_input, **tokenized_it;

    Command *cmd_ll;  /* The cmd linked list */
    int ll_size;  /* Size of the cmd linked list */

    /* TODO: Check for errors. */
    getlogin_r(curr_user, LOGIN_NAME_MAX);
    pw = getpwuid(getuid());
    homedir = pw->pw_dir;

    /* Loop the shell prompt, waiting for input */
    while (1) {
        /* Get current user and path */
        getcwd(curr_path, PATH_MAX);
        /* TODO: Check for errors. */

        /* Print prompt */
        printf("%s:%s> ", curr_user, curr_path);

        /* Get user input and tokenize it */
        fgets(input, MAX_INPUT_LENGTH, stdin);
        tokenized_input = tokenizer(input);

        /* Interpret tokenized input */
        if (!tokenized_input || !*tokenized_input) {
            continue;
        } else if (strcmp(tokenized_input[0], "exit") == 0) {
            break;
        } else if (strcmp(tokenized_input[0], "cd") == 0 ||
                   strcmp(tokenized_input[0], "chdir") == 0) {

            if (tokenized_input[1] == NULL) {
                free(tokenized_input[1]);
                tokenized_input[1] = strdup(homedir);
            }

            /* chdir changes current directory. Returns nonzero if error. */
            if (chdir(tokenized_input[1])) {
                if (errno == ENOTDIR) {
                    fputs("A component of the path is not a directory.\n", stderr);
                } else if (errno == EACCES) {
                    fputs("Access denied.\n", stderr);
                } else if (errno == EIO) {
                    fputs("IO error.\n", stderr);
                } else {
                    fputs("Error changing directory.\n", stderr);
                }
            }
        } else {
            cmd_ll = make_cmd_ll(tokenized_input, &ll_size);
            if (cmd_ll == NULL) {
                fputs("Invalid entry. Command not supported.\n", stderr);
            }
            else {
                exec_cmd(curr_path, cmd_ll, ll_size);
            }
        }

        /* Free tokenized input */
        tokenized_it = tokenized_input;
        while (*tokenized_it != NULL) {
            free(*tokenized_it);
            tokenized_it++;
        }
        free(tokenized_input);
    }

    return 0;
}

char * concat(char *str1, char *str2) {
    char *buffer;
    int i, len;

    /* + 1 because of the NULL at the end */
    len = (int) (strlen(str1) + strlen(str2) + 1);

    buffer = (char*) malloc(sizeof(char *) * len);

    /* Malloc failed */
    if (buffer == NULL) {
        fputs("Fatal error: Could not allocate memory. Aborting.\n", stderr);
        exit(1);
    }
        
    for (i = 0; i < (int) strlen(str1); i++) {
        buffer[i] = str1[i];
    }
    for (i = strlen(str1); i < (len - 1); i++) {
        buffer[i] = str2[i - strlen(str1)];
    }
    buffer[i] = ASCII_NUL;

    return buffer;
}

Command * make_cmd_ll(char **tokenized, int *ll_size) {
    int i, argv_ind;
    char **cmd_argv;
    Command *cmd, *cur_cmd, *cmd_ll_root;


    /* Initialize linked list size to 0 */
    *ll_size = 0;

    /* The current command we are parsing */
    cmd = (Command *) malloc(sizeof(Command));
    cmd->stdin_loc = NULL;
    cmd->stdout_loc = NULL;
    cmd->stderr_loc = NULL;
    cmd->next = NULL;
    cmd_ll_root = cmd;
    cur_cmd = cmd;

    /* Malloc failed */
    if (cmd == NULL) {
        fputs("Fatal error: Could not allocate memory. Aborting.\n", stderr);
        exit(1);
    }

    i = 0;
    while (tokenized[i] != NULL) {
        if (strcmp(tokenized[i], "|") == 0) {
            if (tokenized[i + 1] == NULL) {
                fputs("Invalid pipe specified.\n", stderr);
                return NULL;
            }
            /* We are piping to a new command so create that command struct */
            cmd = (Command *) malloc(sizeof(Command));

            /* Malloc failed */
            if (cmd == NULL) {
                fputs("Fatal error: Could not allocate memory. Aborting.\n", stderr);
                exit(1);
            }

            /* Initialize its fields */
            cmd->argc = 0;
            cmd->stdin_loc = NULL;
            cmd->stdout_loc = NULL;
            cmd->stderr_loc = NULL;
            cmd->next = NULL;
            cur_cmd->next = cmd;
            cur_cmd = cmd; 
            i++; 
            continue;
        }
        
        if ((i == 0) || (strcmp(tokenized[i - 1], "|") == 0)) {
            cmd->process = strdup(tokenized[i]);
        }

        if (strcmp(tokenized[i], "<") == 0) {
            if (tokenized[i + 1] == NULL) {
                fputs("Invalid redirect specified.\n", stderr);
                return NULL;
            }
            cmd->stdin_loc = strdup(tokenized[i + 1]);
            i = i + 2;
        }
        else if (strcmp(tokenized[i], ">") == 0) {
            if (tokenized[i + 1] == NULL) {
                fputs("Invalid redirect specified.\n", stderr);
                return NULL;
            }
            cmd->stdout_loc = strdup(tokenized[i + 1]);
            i = i + 2;
        }
        else {
            i++;
            cmd->argc++;
        }
    }
    
    i = 0;
    argv_ind = 0;
    /* Loop for setting command structs' argv fields. */
    cur_cmd = cmd_ll_root;
    /* cmd_ll_root->argc + 1 because we include NULL at the end */
    cmd_argv = (char **) malloc(sizeof(char *) * (cmd_ll_root->argc + 1));

    /* Malloc failed */
    if (cmd_argv == NULL) {
        fputs("Fatal error: Could not allocate memory. Aborting.\n", stderr);
        exit(1);
    }

    while (tokenized[i] != NULL) {
        if (strcmp(tokenized[i], "|") == 0) {
            cur_cmd->argv = cmd_argv;
            cur_cmd->argv[argv_ind] = NULL;
            cur_cmd = cur_cmd->next;
            cmd_argv = (char **) malloc(sizeof(char *) * 
                (cmd_ll_root->argc + 1));

            /* Malloc failed */
            if (cmd_argv == NULL) {
                fputs("Fatal error: Could not allocate memory. Aborting.\n", stderr);
                exit(1);
            }

            argv_ind = 0;
            (*ll_size)++;
        }
        if (strcmp(tokenized[i], ">") == 0 
            || strcmp(tokenized[i], "<") == 0) {
            i = i + 2;
        }
        else if (strcmp(tokenized[i], "|") == 0) {
            i++;
        }
        else {
            cmd_argv[argv_ind] = strdup(tokenized[i]);
            i++;
            argv_ind++;
        }
    }
    /* Set the last command struct's argv */
    cur_cmd->argv = cmd_argv;
    (*ll_size)++;
    
    return cmd_ll_root;
}

void exec_cmd(char *curr_path, Command *cmd, int num_cmds) {
    int i;
    pid_t pid;
    int remaining;
    int ret_val;
    int pipefd[2];

    /* Inspired by: 
        http://stackoverflow.com/questions/876605/multiple-child-process */

    /* Array of children pids */
    pid_t * pids = (pid_t *) malloc(num_cmds * sizeof(pid_t));

    /* Malloc failed */
    if (pids == NULL) {
        fputs("Fatal error: Could not allocate memory. Aborting.\n", stderr);
        exit(1);
    }
    
    for (i = 0; i < num_cmds; i++) {
        pid = fork();
        pids[i] = pid;
        
        if (pipe(pipefd) == -1) {
            fputs("Fatal error: Could not pipe. Aborting.\n", stderr);
            exit(1);
        }

        if (pid < 0) {
            fputs("Fatal error: Could not fork. Aborting.\n", stderr);
            exit(1);
        }
        else if (pid == 0) {
            /* We're in child process here */
            /* Close the write end, because the child is only using the read
             * end of the pipe.
             */
            close(pipefd[1]); 
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
            
            /* TODO: Set up redirects here? */

            execve(concat("/bin/", cmd->process), cmd->argv, NULL);

            /* If we're here, execve failed. Let's try to run it on
            *  current wd
            */

            execve(concat(concat(curr_path, "/"), cmd->process), cmd->argv, NULL);

            /* If we're here, second execve failed. */
            fputs("Fatal error: Could not execve. Aborting.\n", stderr);
            exit(1);

            /* TODO: If one command fails, should all of them fail? */

        }
        /* We're in the parent process here */
        /* Close the read end, because the parent process only writes to
         * the pipe.
         */
        else {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);

            /* Advance to next command */
            cmd = cmd->next;
        }
    }

    /* Wait for children (Note that we're at parent process here, since
    * all children have exec'ed before this point
    */
    remaining = num_cmds;

    /* While there's still children alive */
    while (remaining != 0) {

        /* For each spawned child */
        for (i = 0; i < num_cmds; i++) {

            /* If children is alive */
            if (pids[i] != 0) {
                /* Wait for this child */
                waitpid(pids[i], &ret_val, 0);

                /* Set this pid to NULL to denote dead child */
                pids[i] = 0;

                /* We have one less remaining child to wait for */
                remaining--;
            }
        }
    }
}

char ** tokenizer(char * str) {
    int num_tokens;
    char ** tokens;

    char * token;
    int token_length;
    
    int token_index;

    char in_string_char;
    char * str_it;

    /*
     * Count number of tokens.
     */

    str_it = str;
    token_length = 0;
    in_string_char = ASCII_NUL;
    token = str;
    num_tokens = 1;

    while (1) {
        token_length++;

        /* Quotation handling */
        if (in_string_char) {
            if (*str_it == in_string_char) {
                in_string_char = ASCII_NUL;

                if (token_length > 1) {
                    /* End string token, don't include this quotation mark */
                    num_tokens++;
                }

                token = str_it + 1;
                token_length = 0;
            }
        } else if (*str_it == '\'' || *str_it == '"') {
            in_string_char = *str_it;

            /* Start token, don't include this quotation mark */
            token = str_it + 1;
            token_length = 0;
        /* Whitespace handling */
        } else if (*str_it == ' ' ||
                   *str_it == '\t' ||
                   *str_it == '\n' ||
                   *str_it == ASCII_NUL) {
            if (token_length > 1) {
                /*
                 * If this whitespace isn't start of token,
                 * end token, don't include this whitespace.
                 */
                num_tokens++;
            }

            if (*str_it == ASCII_NUL) {
                break;
            }

            /* Start new token after whitespace */
            token = str_it + 1;
            token_length = 0;
        /* Redirection and pipe handling */
        } else if (*str_it == '<' ||
                   *str_it == '>' ||
                   *str_it == '|') {
            if (token_length > 1) {
                num_tokens++;
            }
            /* Redirection or pipe symbol is its own token */
            num_tokens++;

            /* Start new token */
            token = str_it + 1;
            token_length = 0;
        }

        str_it++;
    }


    /*
     * Allocate token array.
     */

    tokens = (char **) malloc(sizeof(char *) * num_tokens);
    if (tokens == NULL) {
        fputs("Fatal error: Could not allocate memory. Aborting.\n", stderr);
        exit(1);
    }
    tokens[num_tokens - 1] = NULL;
    

    /*
     * Populate token array.
     */

    str_it = str;
    token_length = 0;
    in_string_char = ASCII_NUL;
    token = str;
    token_index = 0;

    while (1) {
        token_length++;

        /* Quotation handling */
        if (in_string_char) {
            if (*str_it == in_string_char) {
                in_string_char = ASCII_NUL;

                if (token_length > 1) {
                    /* End string token, don't include this quotation mark */
                    tokens[token_index] = strndup(token, token_length - 1);
                    token_index++;
                }

                token = str_it + 1;
                token_length = 0;
            }
        } else if (*str_it == '\'' || *str_it == '"') {
            in_string_char = *str_it;

            /* Start token, don't include this quotation mark */
            token = str_it + 1;
            token_length = 0;
        /* Whitespace handling */
        } else if (*str_it == ' ' ||
                   *str_it == '\t' ||
                   *str_it == '\n' ||
                   *str_it == ASCII_NUL) {
            if (token_length > 1) {
                /*
                 * If this whitespace isn't start of token,
                 * end token, don't include this whitespace.
                 */
                tokens[token_index] = strndup(token, token_length - 1);
                token_index++;
            }

            if (*str_it == ASCII_NUL) {
                break;
            }

            /* Start new token after whitespace */
            token = str_it + 1;
            token_length = 0;
        /* Redirection and pipe handling */
        } else if (*str_it == '<' ||
                   *str_it == '>' ||
                   *str_it == '|') {
            if (token_length > 1) {
                tokens[token_index] = strndup(token, token_length - 1);
                token_index++;
            }
            /* Redirection or pipe symbol is its own token */
            tokens[token_index] = strndup(str_it, 1);
            token_index++;

            /* Start new token */
            token = str_it + 1;
            token_length = 0;
        }

        str_it++;
    }
    
    return tokens;
}
