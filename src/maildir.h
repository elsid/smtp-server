#ifndef SMTP_SERVER_MAILDIR_H
#define SMTP_SERVER_MAILDIR_H

#define PATH_SIZE 256

typedef struct maildir {
    char __path[PATH_SIZE];
} maildir_t;

int maildir_init(maildir_t *maildir, const char *path, const char *recipient);
int maildir_create_file(const maildir_t *maildir, const char *filename);
int maildir_remove_file(const maildir_t *maildir, const char *filename);
int maildir_move_to_new(const maildir_t *maildir, const char *filename);
int maildir_clone_file(const maildir_t *src, const maildir_t *dst,
    const char *filename);

#endif
