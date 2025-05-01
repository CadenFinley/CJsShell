#include "shell_env_loader.h"

void load_shell_env(void) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe");
        exit(1);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        // child: run the shell to dump its env
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
            perror("dup2");
            _exit(1);
        }
        close(pipefd[1]);
        char *shell = getenv("SHELL");
        if (!shell) shell = "/bin/bash";
        execlp(shell, shell, "-li", "-c", "env -0", (char*)NULL);
        perror("execlp");
        _exit(1);
    }

    // parent: read back NUL-terminated env
    close(pipefd[1]);
    char  buf[4096];
    ssize_t n;
    // we'll accumulate into a dynamic buffer
    size_t   cap = 0, len = 0;
    char    *all = NULL;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
        if (len + n > cap) {
            cap = (cap + n) * 2;
            all = realloc(all, cap);
            if (!all) {
                perror("realloc");
                exit(1);
            }
        }
        memcpy(all + len, buf, n);
        len += n;
    }
    close(pipefd[0]);
    waitpid(pid, NULL, 0);

    // parse NUL-delimited "KEY=VALUE\0KEY=VALUE\0…"
    size_t off = 0;
    while (off < len) {
        char *eq = memchr(all + off, '=', len - off);
        if (!eq) break;
        size_t klen = eq - (all + off);
        char *val = eq + 1;
        // find terminating NUL
        char *nul = memchr(val, '\0', len - off - klen - 1);
        if (!nul) break;
        size_t vlen = nul - val;
        // copy out key & value into C-strings
        char key[256];
        if (klen >= sizeof(key)) { off += klen + vlen + 1; continue; }
        memcpy(key, all + off, klen);
        key[klen] = '\0';
        char *value = strndup(val, vlen);
        if (!value) { perror("strndup"); exit(1); }
        setenv(key, value, 1);
        free(value);
        off += klen + vlen + 1;
    }
    free(all);
}

