#include <stdio.h>
#include <sys/stat.h>

#include "log.h"
#include "maildir.h"

static int make_path(const char *path, const __mode_t mode)
{
    char tmp[PATH_SIZE];

    if (snprintf(tmp, sizeof(tmp), "%s", path) < 0) {
        CALL_ERR("snprintf");
        return -1;
    }

    const size_t len = strlen(tmp);

    if(tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for(char *p = tmp + 1; *p; p++) {
        if(*p == '/') {
            *p = 0;

            if (mkdir(tmp, mode) < 0) {
                if (errno != EEXIST) {
                    CALL_ERR_ARGS("mkdir", "%s, %d", tmp, mode);
                    return -1;
                }
            }

            *p = '/';
        }
    }

    return mkdir(tmp, mode);
}

int maildir_init(maildir_t *maildir, const char *path, const char *recipient)
{
    const size_t path_len = strlen(path);

    if (path_len + strlen(recipient) + strlen("/Maildir") + 2 > sizeof(maildir->__path)) {
        PRINT_STDERR("%s", "path too long");
        return -1;
    }

    const char *recipient_delim = strchr(recipient, '@');

    if (NULL == recipient_delim) {
        PRINT_STDERR("%s", "recipient delim not found");
        return -1;
    }

    const char *domain = recipient_delim + 1;

    memset(maildir->__path, 0, sizeof(maildir->__path));
    strncpy(maildir->__path, path, sizeof(maildir->__path));
    snprintf(maildir->__path + path_len, sizeof(maildir->__path) - path_len, "/%s/", domain);
    strncpy(maildir->__path + strlen(maildir->__path), recipient, recipient_delim - recipient);
    strcat(maildir->__path, "/Maildir");

    static const char *sub_paths[] = {"tmp", "new", "cur"};
    static const size_t sub_paths_count = sizeof(sub_paths) / sizeof(*sub_paths);

    for (size_t i = 0; i < sub_paths_count; ++i) {
        char full_path[PATH_SIZE];

        if (snprintf(full_path, sizeof(full_path), "%s/%s", maildir->__path, sub_paths[i]) < 0) {
            return -1;
        }

        if (make_path(full_path, S_IRWXU | S_IRWXG | S_IRWXO) < 0) {
            if (EEXIST != errno) {
                CALL_ERR_ARGS("mkdir", "%s", full_path);
                return -1;
            }
        }
    }

    return 0;
}

int maildir_create_file(const maildir_t *maildir, const char *filename)
{
    char file_path[PATH_SIZE];

    if (snprintf(file_path, sizeof(file_path), "%s/tmp/%s", maildir->__path, filename) < 0) {
        CALL_ERR("snprintf");
        return -1;
    }

    const int fd = open(file_path, O_CREAT | O_WRONLY | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if (fd < 0) {
        CALL_ERR_ARGS("open", "%s", file_path);
        return -1;
    }

    return fd;
}

int maildir_remove_file(const maildir_t *maildir, const char *filename)
{
    char tmp_file_path[PATH_SIZE];

    if (snprintf(tmp_file_path, sizeof(tmp_file_path), "%s/tmp/%s", maildir->__path, filename) < 0) {
        CALL_ERR_ARGS("open", "%s %s %s", "%s/tmp/%s", maildir->__path, filename);
        return -1;
    }

    if (remove(tmp_file_path) < 0) {
        CALL_ERR_ARGS("remove", "%s", tmp_file_path);
        return -1;
    }

    return 0;
}

int maildir_move_to_new(const maildir_t *maildir, const char *filename)
{
    char tmp_file_path[PATH_SIZE];

    if (snprintf(tmp_file_path, sizeof(tmp_file_path), "%s/tmp/%s", maildir->__path, filename) < 0) {
        CALL_ERR_ARGS("open", "%s %s %s", "%s/tmp/%s", maildir->__path, filename);
        return -1;
    }

    char new_file_path[PATH_SIZE];

    if (snprintf(new_file_path, sizeof(new_file_path), "%s/new/%s", maildir->__path, filename) < 0) {
        CALL_ERR_ARGS("open", "%s %s %s", "%s/new/%s", maildir->__path, filename);
        return -1;
    }

    if (rename(tmp_file_path, new_file_path) < 0) {
        CALL_ERR_ARGS("rename", "%s %s", tmp_file_path, new_file_path);
        return -1;
    }

    return 0;
}

int maildir_clone_file(const maildir_t *src, const maildir_t *dst,
    const char *filename)
{
    char src_file_path[PATH_SIZE];

    if (snprintf(src_file_path, sizeof(src_file_path), "%s/new/%s", src->__path, filename) < 0) {
        CALL_ERR_ARGS("open", "%s %s %s", "%s/new/%s", src->__path, filename);
        return -1;
    }

    char dst_file_path[PATH_SIZE];

    if (snprintf(dst_file_path, sizeof(dst_file_path), "%s/new/%s", dst->__path, filename) < 0) {
        CALL_ERR_ARGS("open", "%s %s %s", "%s/new/%s", dst->__path, filename);
        return -1;
    }

    if (link(src_file_path, dst_file_path) < 0) {
        CALL_ERR_ARGS("link", "%s %s", src_file_path, dst_file_path);
        return -1;
    }

    return 0;
}
