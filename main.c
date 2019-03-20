#define _XOPEN_SOURCE 500
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <time.h>
#include <unistd.h>

#define MAX_EVENTS 10

static int ifd;

static int walk(const char *fpath, const struct stat *sb, int tflag,
                struct FTW *ftwbuf) {
  int wd;

  if (tflag != FTW_D)
    return 0;

  wd = inotify_add_watch(ifd, fpath, IN_MODIFY);
  if (wd == -1) {
    perror("inotify_add_watch");
    exit(EXIT_FAILURE);
  }

  return 0;
}

int main(int argc, char *argv[]) {
  int efd;
  struct epoll_event ev, events[MAX_EVENTS];
  int nfds;
  char *command;
  size_t len = 0, prev_len;

  if (argc < 3) {
    printf("Usage: %s PATH COMMAND\n");
    exit(EXIT_FAILURE);
  }

  command = (char *)malloc(1);
  if (command == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }
  for (int i = 2; i < argc; i++) {
    prev_len = len;
    if (i > 2)
      command[prev_len - 1] = ' ';
    len = len + 1 + strlen(argv[i]);

    command = (char *)realloc(command, len);
    if (command == NULL) {
      perror("realloc");
      exit(EXIT_FAILURE);
    }
    strcpy(command + prev_len, argv[i]);
  }

  printf("Command to be executed: %s\n", command);

  ifd = inotify_init1(IN_CLOEXEC | IN_NONBLOCK);
  if (ifd == -1) {
    perror("inotify_init1");
    exit(EXIT_FAILURE);
  }

  if (nftw(argv[1], walk, 20, 0) == -1) {
    perror("nftw");
    exit(EXIT_FAILURE);
  }

  efd = epoll_create1(EPOLL_CLOEXEC);
  if (efd == -1) {
    perror("epoll_create1");
    exit(EXIT_FAILURE);
  }

  ev.events = EPOLLIN | EPOLLET;
  ev.data.fd = ifd;
  if (epoll_ctl(efd, EPOLL_CTL_ADD, ifd, &ev) == -1) {
    perror("epoll_ctl");
    exit(EXIT_FAILURE);
  }

  for (time_t prev = 0;;) {
    nfds = epoll_wait(efd, events, MAX_EVENTS, -1);
    if (nfds == -1) {
      perror("epoll_wait");
      exit(EXIT_FAILURE);
    }
    if (time(NULL) - prev > 1) {
      prev = time(NULL);
      system(command);
    }
  }

  free(command);
  close(ifd);
  exit(EXIT_SUCCESS);
}
