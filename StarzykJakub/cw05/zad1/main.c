#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <wait.h>
#include <errno.h>

const int buffer_size = 4096;
const int max_args = 10;
const int max_commands = 20;


FILE *_try_open(const char *file) {
    FILE *f = fopen(file, "r");

    if (f == NULL) {
        fclose(f);
        fprintf(stderr, "error opening file for reading\n");
        exit(EXIT_FAILURE);
    }

    return f;
}

char *_get_command_line(FILE *f, char *buffer, int size) {
    char *res_fgets = fgets(buffer, size, f);

    if (res_fgets != NULL) {
        size_t line_size = strlen(buffer) + 1;
        char *line = malloc(line_size * sizeof(char));
        memcpy(line, buffer, line_size * sizeof(char));
        return line;
    } else if (ferror(f)) {
        fprintf(stderr, "error getting line from file\n");
        exit(EXIT_FAILURE);
    } else {
        return NULL;
    }
}


int _split_by_delim(char *line, char *delim, int max_args, char **result) {
    char *arg = strtok(line, delim);
    int size = 0;

    while (size < max_args && arg != NULL) {
        result[size++] = arg;
        arg = strtok(NULL, delim);
    }

    result[size] = NULL;

    return size;
}


void _print_arguments(char **line_arguments, int n_args) {
    printf("PID: %d\n", getpid());
    printf("\'%s\'\n", line_arguments[0]);

    for (int i = 1; i < n_args; i++) {
        printf("\t\'%s\'\n", line_arguments[i]);
    }
}


int _run_command_line2(char **commands, int _n_commands) {
    printf("\n");
    char ***command_args = malloc(_n_commands * sizeof(char **));
    int valid_commands = -1;
    int all_commands = 0;

    while (all_commands < _n_commands) {
        char **tmp = malloc((max_args + 1) * sizeof(char *));
        int n = _split_by_delim(commands[all_commands], " \t\n", max_args, tmp);

        if (n > 0) {
            if (valid_commands > -1) {
                printf("| ");
            }
            command_args[++valid_commands] = tmp;
            for (int i = 0; i < n; i++) {
                printf("%s ", command_args[valid_commands][i]);
            }
        } else {
            free(tmp);
        }

        all_commands++;
    }

    int n_commands = valid_commands + 1;

    if(n_commands < 1) {
        return 0;
    }

    printf("\n");

    int pipes[n_commands - 1][2];

    for (int i = 0; i < n_commands; i++) {
        bool next_command_exists = (i < n_commands - 1);
        bool previous_command_exists = (i > 0);

        if (next_command_exists) {
            pipe(pipes[i]);
        }

        pid_t pid = fork();

        if (pid == 0) {
            if (previous_command_exists) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
                close(pipes[i - 1][0]);
                close(pipes[i - 1][1]);
            }

            if (next_command_exists) {
                close(pipes[i][0]);
                dup2(pipes[i][1], STDOUT_FILENO);
                close(pipes[i][1]);
            }

            execvp(command_args[i][0], command_args[i]);
            _exit(EXIT_FAILURE);
        } else {
            if (previous_command_exists) {
                close(pipes[i - 1][0]);
                close(pipes[i - 1][1]);
            }
        }
    }

//    if (n_commands > 1) {
//        close(pipes[n_commands - 2][0]);
//        close(pipes[n_commands - 2][1]);
//    }

    while (wait(NULL) <= 0) {
        usleep(100000);
    }

    for (int i = 0; i < _n_commands; i++) {
        free(command_args[i]);
    }

    free(command_args);
    usleep(500000);
    return 0;
}

void run_interpreter(const char *file) {
    FILE *f = _try_open(file);
    char buffer[buffer_size];
    char *line;

    int i = 1;
    while ((line = _get_command_line(f, buffer, buffer_size)) != NULL) {
        char **commands = malloc((max_commands + 1) * sizeof(char *));
        int n_commands = _split_by_delim(line, "|", max_commands, commands);

        if (n_commands == 0) {
            continue;
        }

        if (_run_command_line2(commands, n_commands) != 0) {
            fprintf(stderr, "%d. line failed\n", i);
            exit(EXIT_FAILURE);
        }

        i++;
        free(commands);
        free(line);
    }
}

bool _is_valid_file(const char *file) {
    if (access(file, F_OK) == -1) {
        return false;
    } else {
        return true;
    }
}

char *_parse_args(int argc, char *argv[]) {
    if (argc == 1) {
        printf("usage: main path_to_file\n");
        exit(EXIT_SUCCESS);
    } else {
        return argv[1];
    }
}


int main(int argc, char *argv[]) {
    const char *file = _parse_args(argc, argv);

    if (!_is_valid_file(file)) {
        fprintf(stderr, "file doesn't exist\n");
        exit(EXIT_FAILURE);
    } else {
        run_interpreter(file);
        exit(EXIT_SUCCESS);
    }

}
