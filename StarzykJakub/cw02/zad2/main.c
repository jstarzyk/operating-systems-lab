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

char path_buf[PATH_MAX + 1];

void _print_stat(const struct stat *info, const char *path) {
    char *mod_date = _convert_time_t(info->st_mtim.tv_sec);
    char permissions[10];
    _convert_permissions(permissions, info->st_mode);
    realpath(path, path_buf);
    printf("%s\t%ld\t%s   %s\n", permissions, info->st_size, mod_date, path_buf);
    free(mod_date);
}

void _change_dir(char *dir) {
    if (chdir(dir) == -1) {
        fprintf(stderr, "chdir to %s failed\n", dir);
        exit(EXIT_FAILURE);
    }
}

void _print1(char *dir_str, time_t mod_date, char relation) {
    _change_dir(dir_str);

    DIR *dir = opendir(".");
    struct dirent *entry;
    struct stat info;

    while ((entry = readdir(dir)) != NULL && lstat(entry->d_name, &info) != -1) {
        if (S_ISDIR(info.st_mode)) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            _print1(entry->d_name, mod_date, relation);
            _change_dir("..");
        } else if (S_ISREG(info.st_mode) && _assert_mod_date(&info, mod_date, relation)) {
            _print_stat(&info, entry->d_name);
        }
    }

    closedir(dir);
}

void print1(Params *params) {
    _print1(params->dir, params->mod_date, params->relation);
}


static time_t _mod_date;
static char _relation;

int _print2(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    if (typeflag == FTW_F && _assert_mod_date(sb, _mod_date, _relation)) {
        _print_stat(sb, fpath);
    } else if (typeflag == FTW_DNR) {
        fprintf(stderr, "could not read directory %s\n", fpath);
    }
    return 0;
}

void print2(Params *params) {
    if (nftw(params->dir, _print2, FTW_D, FTW_PHYS) == -1) {
        fprintf(stderr, "nftw error");
    }
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

int _parse_implementation_variant(Params *params, char *str) {
    if (strcmp(str, "1") == 0) {
        params->use_nftw = false;
    } else if (strcmp(str, "2") == 0) {
        params->use_nftw = true;
    } else {
        fprintf(stderr, "implementation variant not specified\n");
        return -1;
    }

    return 0;
}

Params *parse_args(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "program requires 4 arguments");
        exit(EXIT_FAILURE);
    }

    Params *parsed = calloc(1, sizeof(Params));

    if (_parse_dir(parsed, argv[1]) == -1 ||
            _parse_relation(parsed, argv[2]) == -1 ||
            _parse_date(parsed, argv[3]) == -1 ||
            _parse_implementation_variant(parsed, argv[4]) == -1
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
    printf("use_nftw: %d\n", params->use_nftw);
}

int main(int argc, char *argv[]) {
    Params *parsed = parse_args(argc, argv);
//     _test_print_params(parsed);

    if (!parsed->use_nftw) {
        print1(parsed);
    } else {
        _mod_date = parsed->mod_date;
        _relation = parsed->relation;
        print2(parsed);
    }

    exit(EXIT_SUCCESS);
}
