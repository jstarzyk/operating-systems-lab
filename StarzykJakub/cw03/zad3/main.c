#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <wait.h>
#include <errno.h>
#include <sys/resource.h>

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

struct rlimit *_util_get_curr_limits(int resource)
{
    struct rlimit *rlptr = malloc(sizeof(struct rlimit));
    
    if (getrlimit(resource, rlptr) == -1)
    {
        fprintf(stderr, "getrlimit failed for PID=%d\n", getpid());
        exit(EXIT_FAILURE);
    }
    
    return rlptr;
}

void _util_set_new_limits(int resource, struct rlimit *rlptr)
{
    if (setrlimit(resource, rlptr) == -1)
    {
        fprintf(stderr, "setrlimit failed for PID=%d, errno=%d\n", getpid(), errno);
        exit(EXIT_FAILURE);
    }
}

void _replace_limits(int resource, const rlim_t new_value)
{
    struct rlimit *curr = _util_get_curr_limits(resource);
    rlim_t old_curr = curr->rlim_cur;
    rlim_t old_max = curr->rlim_max;
    curr->rlim_max = new_value;
    if (curr->rlim_cur > new_value) {
        curr->rlim_cur = new_value;
    }
    printf("%d (cur): %lu -> %lu\n", resource, old_curr, curr->rlim_cur);
    printf("%d (max): %lu -> %lu\n", resource, old_max, curr->rlim_max);
    _util_set_new_limits(resource, curr);
    free(curr);
}

int _run_command(char **line_arguments, int n_args, const rlim_t proc_time, const rlim_t virt_size)
{
    pid_t child_pid = fork();

    if (child_pid == 0)
    {
        _print_arguments(line_arguments, n_args);
        _replace_limits(RLIMIT_CPU, proc_time);
        _replace_limits(RLIMIT_AS, virt_size);
        printf("\n");
        
        fflush(stdout);
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


long diff_time_val(struct timeval start, struct timeval end) {
    long start_micros = start.tv_usec + start.tv_sec * 1000000;
    long end_micros = end.tv_usec + end.tv_sec * 1000000;
    return end_micros - start_micros;
}

struct TimevalResult {
    struct timeval user;
    struct timeval sys;
};
typedef struct TimevalResult TimevalResult;

void run_interpreter(const char *file, const rlim_t proc_time, const rlim_t virt_size)
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
        
        TimevalResult start;
        TimevalResult end;
        struct rusage usage_child;
        getrusage(RUSAGE_CHILDREN, &usage_child);
        start.user = usage_child.ru_utime;
        start.sys = usage_child.ru_stime;

        if (_run_command(line_arguments, n_args, proc_time, virt_size) == EXIT_FAILURE)
        {
            fprintf(stderr, "%s failed\n", line_arguments[0]);
            exit(EXIT_FAILURE);
        }
        
        getrusage(RUSAGE_CHILDREN, &usage_child);
        end.user = usage_child.ru_utime;
        end.sys = usage_child.ru_stime;
        
        double sys, user;
        sys = diff_time_val(start.sys, end.sys) / 1000000.0;
        user = diff_time_val(start.user, end.user) / 1000000.0;

        printf("\nU: %.6f s\n", user);
        printf("S: %.6f s\n\n\n", sys);
        
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

char *_parse_file(char *file)
{
    if (!_is_valid_file(file))
    {
        fprintf(stderr, "file doesn't exist\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        return file;
    }
}

rlim_t _parse_proc_time(char *time)
{
    return (rlim_t)atoll(time);
}

rlim_t _parse_virt_size(char *size)
{
    return (rlim_t)(atoll(size) * 1000000);
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("usage: main path_to_file proc_limit virt_limit\n");
        printf("  proc_limit - in seconds\n  virt_limit - in megabytes\n");
        exit(EXIT_SUCCESS);
    }
    
    const char *file = _parse_file(argv[1]);
    const rlim_t proc_time = _parse_proc_time(argv[2]);
    const rlim_t virt_size = _parse_virt_size(argv[3]);
    run_interpreter(file, proc_time, virt_size);

    exit(EXIT_SUCCESS);
}
