#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <dirent.h>
#include <limits.h>
#include <ftw.h>
#include <wait.h>


struct Params {
    char *dir;
    char relation;
    time_t mod_date;
    bool use_nftw;
};
typedef struct Params Params;

char *_convert_time_t(time_t time) {
    char *buffer = malloc(20 * sizeof(char));
    strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", localtime(&time));
    return buffer;
}

bool _assert_mod_date(const struct stat *info, time_t mod_date, char relation) {
    if (relation == '<') {
        return (info->st_mtim.tv_sec < mod_date);
    } else if (relation == '>') {
        return (info->st_mtim.tv_sec > mod_date);
    } else {
        return (info->st_mtim.tv_sec == mod_date);
    }
}

void _convert_permissions(char *perms, unsigned int st_mode) {
    perms[0] = S_ISDIR(st_mode) ? 'd' : '.';
    perms[1] = (st_mode & S_IRUSR) ? 'r' : '-';
    perms[2] = (st_mode & S_IWUSR) ? 'w' : '-';
    perms[3] = (st_mode & S_IXUSR) ? 'x' : '-';
    perms[4] = (st_mode & S_IRGRP) ? 'r' : '-';
    perms[5] = (st_mode & S_IWGRP) ? 'w' : '-';
    perms[6] = (st_mode & S_IXGRP) ? 'x' : '-';
    perms[7] = (st_mode & S_IROTH) ? 'r' : '-';
    perms[8] = (st_mode & S_IWOTH) ? 'w' : '-';
    perms[9] = (st_mode & S_IXOTH) ? 'x' : '-';
}

void _print_stat(const struct stat *info, const char *path) {
    char *mod_date = _convert_time_t(info->st_mtim.tv_sec);
    char permissions[10];
    _convert_permissions(permissions, info->st_mode);
    char *full = realpath(path, NULL);
    printf("%d ", getpid());
    printf("%s\t%ld\t%s   %s\n", permissions, info->st_size, mod_date, full);
    free(full);
    free(mod_date);
}

char *make_full_path(char *dir, char *elem) {
    size_t dir_len = strlen(dir);
    size_t elem_len = strlen(elem);
    char *result = malloc((dir_len + elem_len + 2) * sizeof(char));
    if (result == NULL) {
        return NULL;
    }
    sprintf(result, "%s/%s", dir, elem);
    return result;
}

void _print1(char *dir_str, time_t mod_date, char relation) {
    DIR *dir = opendir(dir_str);
    struct dirent *entry;
    struct stat info;

    while ((entry = readdir(dir)) != NULL) {
        char *full_path = make_full_path(dir_str, entry->d_name);
        if (lstat(full_path, &info) == -1) {
            continue;
        }

        if (S_ISDIR(info.st_mode)) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            pid_t child_pid = fork();
            if (child_pid == 0) {
                _print1(full_path, mod_date, relation);
                _exit(0);
            } else {
                waitpid(child_pid, NULL, 0);
            }
        } else if (S_ISREG(info.st_mode) && _assert_mod_date(&info, mod_date, relation)) {
            _print_stat(&info, full_path);
        }
    }
    closedir(dir);
}

void print1(Params *params) {
    char *full_path = realpath(params->dir, NULL);
    _print1(full_path, params->mod_date, params->relation);
    free(full_path);
}


int _parse_dir(Params *parsed, char *str) {
    DIR *dir = opendir(str);

    if (dir == NULL) {
        fprintf(stderr, "error while opening directory %s", str);
        return -1;
    } else {
        closedir(dir);
        parsed->dir = str;
        return 0;
    }
}

int _parse_relation(Params *parsed, char *str) {
    if (strcmp(str, "<") != 0 &&
            strcmp(str, ">") != 0 &&
            strcmp(str, "=") != 0
       ) {
        fprintf(stderr, "%s is not a valid relation\n", str);
        return -1;
    } else {
        parsed->relation = str[0];
        return 0;
    }
}

int _parse_date(Params *parsed, char *str) {
    struct tm tm = {0};
    const char *format = "%Y-%m-%d %H:%M:%S";
    char *result = strptime(str, format, &tm);

    if (result == NULL) {
        fprintf(stderr, "invalid date format, should be %s\n", format);
        return -1;
    } else {
        parsed->mod_date = mktime(&tm);
        return 0;
    }
}

Params *parse_args(int argc, char *argv[]) {
    if (argc == 1) {
        printf("usage: ./main <dir> <relation> <date>\n");
        exit(EXIT_SUCCESS);
    }
    else if (argc != 4) {
        fprintf(stderr, "program requires 3 arguments");
        exit(EXIT_FAILURE);
    }

    Params *parsed = calloc(1, sizeof(Params));

    if (_parse_dir(parsed, argv[1]) == -1 ||
            _parse_relation(parsed, argv[2]) == -1 ||
            _parse_date(parsed, argv[3]) == -1
       ) {
        free(parsed);
        exit(EXIT_FAILURE);
    }

    return parsed;
}

void _test_print_params(Params *params) {
    printf("dir: %s\n", params->dir);
    printf("relation: %c\n", params->relation);
    printf("mod_date: %ld\n", params->mod_date);
}

int main(int argc, char *argv[]) {
    Params *parsed = parse_args(argc, argv);
//     _test_print_params(parsed);

    print1(parsed);

    exit(EXIT_SUCCESS);
}
