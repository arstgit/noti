#define _XOPEN_SOURCE 500
#define _DEFAULT_SOURCE
#include <ftw.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
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
  sigset_t blockSet, prevMask;
  int efd;
  struct epoll_event ev, events[MAX_EVENTS];
  int nfds;
  pid_t cpid = 1, w;
  struct timeval elap, prev, now = {0, 0};
  struct timespec delay = {0, 500000000}; // 0.5 second

  if (argc < 3) {
    printf("Usage: %s PATH COMMAND\n");
    exit(EXIT_FAILURE);
  }

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

  sigemptyset(&blockSet);
  sigaddset(&blockSet, SIGHUP);
  if (sigprocmask(SIG_BLOCK, &blockSet, &prevMask) == -1) {
    perror("sigprocmask1");
    exit(EXIT_FAILURE);
  }

  for (;;) {
    prev = now;
    if (gettimeofday(&now, NULL) == -1) {
      perror("gettimeofday");
      exit(EXIT_FAILURE);
    }
    timersub(&now, &prev, &elap);

    if (elap.tv_sec >= 4) {
      // if (elap.tv_sec >= 1 || elap.tv_usec > 500000000) {
      if (cpid != 1) {
        w = kill(0, SIGHUP);
        if (w == -1) {
          perror("kill");
          exit(EXIT_FAILURE);
        }

        w = waitpid(cpid, NULL, 0);
        if (w == -1) {
          perror("waitpid");
          exit(EXIT_FAILURE);
        }
      }

      cpid = fork();
      if (cpid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
      }
      if (cpid == 0) {
        if (sigprocmask(SIG_SETMASK, &prevMask, NULL) == -1) {
          perror("sigprocmask2");
          exit(EXIT_FAILURE);
        }

        printf("\n\n----------%ld %ld----------\n\n", now.tv_sec, now.tv_usec);

        nanosleep(&delay, NULL);

        execvp(argv[2], argv + 2);
      }
    }

    nfds = epoll_wait(efd, events, MAX_EVENTS, -1);
    if (nfds == -1) {
      perror("epoll_wait");
      if (cpid != 1) {
        w = kill(0, SIGHUP);
        if (w == -1) {
          perror("kill");
          exit(EXIT_FAILURE);
        }

        w = waitpid(cpid, NULL, 0);
        if (w == -1) {
          perror("waitpid");
          exit(EXIT_FAILURE);
        }
      }
      exit(EXIT_FAILURE);
    }
  }

  close(ifd);
  exit(EXIT_SUCCESS);
}
