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
const int max_args = 100;


FILE *_try_open(const char *file)
{
    FILE *f = fopen(file, "r");

    if (f == NULL)
    {
        fclose(f);
        fprintf(stderr, "error opening file for reading\n");
        exit(EXIT_FAILURE);
    }

    return f;
}

char *_get_command_line(FILE *f, char *buffer, int size)
{
    char *res_fgets = fgets(buffer, size, f);

    if (res_fgets != NULL)
    {
        size_t line_size = strlen(buffer) + 1;
        char *line = malloc(line_size * sizeof(char));
        memcpy(line, buffer, line_size * sizeof(char));
        return line;
    }
    else if (ferror(f))
    {
        fprintf(stderr, "error getting line from file\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        return NULL;
    }
}

int _parse_command_line(char *line, int max_args, char **result)
{
    char delim[] = " \t\n";
    char *arg = strtok(line, delim);
    int size = 0;

    while (size < max_args && arg != NULL)
    {
        result[size++] = arg;
        arg = strtok(NULL, delim);
    }

    result[size] = NULL;

    return size;
}


void _print_arguments(char **line_arguments, int n_args)
{
    printf("PID: %d\n", getpid());
    printf("\'%s\'\n", line_arguments[0]);

    for (int i = 1; i < n_args; i++)
    {
        printf("\t\'%s\'\n", line_arguments[i]);
    }
}

int _run_command(char **line_arguments, int n_args)
{
    pid_t child_pid = fork();

    if (child_pid == 0)
    {
        _print_arguments(line_arguments, n_args);
        int res_execvp = execvp(line_arguments[0], line_arguments);

        if (res_execvp == -1)
        {
            _exit(errno);
        }

        _exit(EXIT_SUCCESS);
    }
    else if (child_pid == -1)
    {
        fprintf(stderr, "fork failed\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        int child_status;

        if (waitpid(child_pid, &child_status, 0) == -1)
        {
            fprintf(stderr, "waitpid failed\n");
            exit(EXIT_FAILURE);
        }

        if (WIFEXITED(child_status))
        {
            if (WEXITSTATUS(child_status) != EXIT_SUCCESS)
            {
                return EXIT_FAILURE;
            }
        }
    }

    return EXIT_SUCCESS;
}

void run_interpreter(const char *file)
{
    FILE *f = _try_open(file);
    char buffer[buffer_size];
    char *line;

    while ((line = _get_command_line(f, buffer, buffer_size)) != NULL)
    {
        char **line_arguments = malloc((max_args + 1) * sizeof(char *));
        int n_args = _parse_command_line(line, max_args, line_arguments);

        if (n_args == 0)
        {
            continue;
        }

        if (_run_command(line_arguments, n_args) == EXIT_FAILURE)
        {
            fprintf(stderr, "%s failed\n", line_arguments[0]);
            exit(EXIT_FAILURE);
        }

        free(line_arguments);
        free(line);
    }
}

bool _is_valid_file(const char *file)
{
    if (access(file, F_OK) == -1)
    {
        return false;
    }
    else
    {
        return true;
    }
}

char *_parse_args(int argc, char *argv[])
{
    if (argc == 1)
    {
        printf("usage: main path_to_file\n");
        exit(EXIT_SUCCESS);
    }
    else
    {
        return argv[1];
    }
}


int main(int argc, char *argv[])
{
    const char *file = _parse_args(argc, argv);

    if (!_is_valid_file(file))
    {
        fprintf(stderr, "file doesn't exist\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        run_interpreter(file);
    }

    exit(EXIT_SUCCESS);
}
