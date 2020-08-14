// Copyright 2019 Google LLC. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <iostream>
#include <iterator>
#include <vector>

#include "pathauditor/util/statusor.h"
#include "pathauditor/file_event.h"
#include "pathauditor/libc/logging.h"
#include "pathauditor/pathauditor.h"
#include "pathauditor/process_information.h"
#include "pathauditor/util/status_macros.h"

typedef int (*orig_open_type)(const char *file, int oflag, ...);
typedef int (*orig_open64_type)(const char *file, int oflag, ...);
typedef int (*orig_openat_type)(int dirfd, const char *file, int oflag, ...);
typedef int (*orig_openat64_type)(int dirfd, const char *file, int oflag, ...);
typedef int (*orig_creat_type)(const char *file, mode_t mode);
typedef int (*orig_creat64_type)(const char *file, mode_t mode);
typedef int (*orig_chdir_type)(const char *path);
typedef int (*orig_chmod_type)(const char *file, mode_t mode);
typedef int (*orig_chown_type)(const char *file, uid_t owner, gid_t group);
typedef int (*orig_execv_type)(const char *path, char *const argv[]);
typedef int (*orig_execl_type)(const char *path, const char *arg, ...);
typedef int (*orig_execve_type)(const char *path, char *const argv[],
                                char *const envp[]);
typedef int (*orig_execle_type)(const char *path, const char *arg, ...);
typedef int (*orig_execvp_type)(const char *file, char *const argv[]);
typedef int (*orig_execlp_type)(const char *file, const char *arg, ...);
typedef FILE *(*orig_fopen_type)(const char *filename, const char *modes);
typedef FILE *(*orig_fopen64_type)(const char *filename, const char *modes);
typedef FILE *(*orig_freopen_type)(const char *filename, const char *modes,
                                   FILE *stream);
typedef FILE *(*orig_freopen64_type)(const char *filename, const char *modes,
                                     FILE *stream);
typedef int (*orig_truncate_type)(const char *file, off_t length);
typedef int (*orig_truncate64_type)(const char *file, off64_t length);
typedef int (*orig_mkdir_type)(const char *path, mode_t mode);
typedef int (*orig_mkdirat_type)(int fd, const char *path, mode_t mode);
typedef int (*orig_link_type)(const char *from, const char *to);
typedef int (*orig_linkat_type)(int fromfd, const char *from, int tofd,
                                const char *to, int flags);
typedef int (*orig_unlink_type)(const char *name);
typedef int (*orig_unlinkat_type)(int dirfd, const char *name, int flags);
typedef int (*orig_remove_type)(const char *filename);
typedef int (*orig_rmdir_type)(const char *path);
typedef int (*orig_mount_type)(const char *special_file, const char *dir,
                               const char *fstype, unsigned rwflag, void *data);
typedef int (*orig_umount_type)(const char *special_file);
typedef int (*orig_umount2_type)(const char *special_file, int flags);
typedef int (*orig_rename_type)(const char *oldpath, const char *newpath);
typedef int (*orig_renameat_type)(int olddirfd, const char *oldpath,
                                  int newdirfd, const char *newpath);
typedef int (*orig_symlink_type)(const char *from, const char *to);
typedef int (*orig_symlinkat_type)(const char *from, int newdirfd,
                                   const char *to);
typedef int (*orig_lchown_type)(const char *file, uid_t owner, gid_t group);
typedef int (*orig_chroot_type)(const char *path);
typedef int (*orig_fchmodat_type)(int fd, const char *file, mode_t mode,
                                  int flag);
typedef int (*orig_fchownat_type)(int fd, const char *file, uid_t owner,
                                  gid_t group, int flag);

namespace pathauditor {

// boolean var to avoid sanitizing function calls made while handling another
// call;  otherwise results in infinite recursion
ABSL_CONST_INIT thread_local bool sanitizing = false;

void LibcFileEventIsUserControlled(const FileEvent &file_event,
                                   const char *function_name) {
  if (sanitizing) {
    return;
  }
  sanitizing = true;

  StatusOr<bool> result =
      FileEventIsUserControlled(SameProcessInformation(), file_event);
  if (!result.ok()) {
    LogError(result.status());
  } else if (*result) {
    LogInsecureAccess(file_event, function_name);
  }

  sanitizing = false;
}

}  // namespace pathauditor

extern "C" {
int open(const char *file, int oflag, ...) {
  mode_t mode = 0;
  if (oflag & O_CREAT || oflag & O_TMPFILE) {
    va_list args;
    va_start(args, oflag);
    mode = va_arg(args, mode_t);
    va_end(args);
  }

  std::vector<std::string> path_args = {file};
  std::vector<uint64_t> args = {0, static_cast<uint64_t>(oflag), mode};
  pathauditor::FileEvent file_event(SYS_open, args, path_args);

  pathauditor::LibcFileEventIsUserControlled(file_event, "open");

  orig_open_type orig_open;
  orig_open = reinterpret_cast<orig_open_type>(dlsym(RTLD_NEXT, "open"));
  return orig_open(file, oflag, mode);
}

int open64(const char *file, int oflag, ...) {
  mode_t mode = 0;
  if (oflag & O_CREAT || oflag & O_TMPFILE) {
    va_list args;
    va_start(args, oflag);
    mode = va_arg(args, mode_t);
    va_end(args);
  }

  std::vector<std::string> path_args = {file};
  std::vector<uint64_t> args = {0, static_cast<uint64_t>(oflag), mode};
  pathauditor::FileEvent file_event(SYS_open, args, path_args);

  pathauditor::LibcFileEventIsUserControlled(file_event, "open64");
  orig_open64_type orig_open64;
  orig_open64 = reinterpret_cast<orig_open64_type>(dlsym(RTLD_NEXT, "open64"));
  return orig_open64(file, oflag, mode);
}

int openat(int dirfd, const char *file, int oflag, ...) {
  mode_t mode = 0;
  if (oflag & O_CREAT || oflag & O_TMPFILE) {
    va_list args;
    va_start(args, oflag);
    mode = va_arg(args, mode_t);
    va_end(args);
  }

  std::vector<std::string> path_args = {file};
  std::vector<uint64_t> args = {static_cast<uint64_t>(dirfd), 0,
                              static_cast<uint64_t>(oflag), mode};
  pathauditor::FileEvent file_event(SYS_openat, args, path_args);

  pathauditor::LibcFileEventIsUserControlled(file_event, "openat");
  orig_openat_type orig_openat;
  orig_openat = reinterpret_cast<orig_openat_type>(dlsym(RTLD_NEXT, "openat"));
  return orig_openat(dirfd, file, oflag, mode);
}

int openat64(int dirfd, const char *file, int oflag, ...) {
  mode_t mode = 0;
  if (oflag & O_CREAT || oflag & O_TMPFILE) {
    va_list args;
    va_start(args, oflag);
    mode = va_arg(args, mode_t);
    va_end(args);
  }

  std::vector<std::string> path_args = {file};
  std::vector<uint64_t> args = {static_cast<uint64_t>(dirfd), 0,
                              static_cast<uint64_t>(oflag), mode};
  pathauditor::FileEvent file_event(SYS_openat, args, path_args);

  pathauditor::LibcFileEventIsUserControlled(file_event, "openat64");
  orig_openat64_type orig_openat64;
  orig_openat64 =
      reinterpret_cast<orig_openat64_type>(dlsym(RTLD_NEXT, "openat64"));
  return orig_openat64(dirfd, file, oflag, mode);
}

int creat(const char *file, mode_t mode) {
  std::vector<std::string> path_args = {file};
  uint64_t flags = O_CREAT | O_WRONLY | O_TRUNC;
  std::vector<uint64_t> args = {0, flags, mode};
  pathauditor::FileEvent file_event(SYS_open, args, path_args);

  pathauditor::LibcFileEventIsUserControlled(file_event, "creat");

  orig_creat_type orig_creat;
  orig_creat = reinterpret_cast<orig_creat_type>(dlsym(RTLD_NEXT, "creat"));
  return orig_creat(file, mode);
}

int creat64(const char *file, mode_t mode) {
  std::vector<std::string> path_args = {file};
  uint64_t flags = O_CREAT | O_WRONLY | O_TRUNC;
  std::vector<uint64_t> args = {0, flags, mode};
  pathauditor::FileEvent file_event(SYS_open, args, path_args);

  pathauditor::LibcFileEventIsUserControlled(file_event, "creat64");

  orig_creat64_type orig_creat64;
  orig_creat64 =
      reinterpret_cast<orig_creat64_type>(dlsym(RTLD_NEXT, "creat64"));
  return orig_creat64(file, mode);
}

int chdir(const char *path) {
  std::vector<std::string> path_args = {path};
  std::vector<uint64_t> args = {0};
  pathauditor::FileEvent file_event(SYS_chdir, args, path_args);

  pathauditor::LibcFileEventIsUserControlled(file_event, "chdir");

  orig_chdir_type orig_chdir;
  orig_chdir = reinterpret_cast<orig_chdir_type>(dlsym(RTLD_NEXT, "chdir"));
  return orig_chdir(path);
}

int chmod(const char *file, mode_t mode) {
  std::vector<std::string> path_args = {file};
  std::vector<uint64_t> args = {0, mode};
  pathauditor::FileEvent file_event(SYS_chmod, args, path_args);

  pathauditor::LibcFileEventIsUserControlled(file_event, "chmod");

  orig_chmod_type orig_chmod;
  orig_chmod = reinterpret_cast<orig_chmod_type>(dlsym(RTLD_NEXT, "chmod"));
  return orig_chmod(file, mode);
}

int fchmodat(int fd, const char *file, mode_t mode, int flag) {
  std::vector<std::string> path_args = {file};
  std::vector<uint64_t> args = {static_cast<uint64_t>(fd), 0, mode,
                              static_cast<uint64_t>(flag)};
  pathauditor::FileEvent file_event(SYS_fchmodat, args, path_args);

  pathauditor::LibcFileEventIsUserControlled(file_event, "fchmodat");

  orig_fchmodat_type orig_fchmodat;
  orig_fchmodat =
      reinterpret_cast<orig_fchmodat_type>(dlsym(RTLD_NEXT, "fchmodat"));
  return orig_fchmodat(fd, file, mode, flag);
}

int chown(const char *file, uid_t owner, gid_t group) {
  std::vector<std::string> path_args = {file};
  std::vector<uint64_t> args = {0, owner, group};
  pathauditor::FileEvent file_event{SYS_chown, args, path_args};

  pathauditor::LibcFileEventIsUserControlled(file_event, "chown");

  orig_chown_type orig_chown;
  orig_chown = reinterpret_cast<orig_chown_type>(dlsym(RTLD_NEXT, "chown"));
  return orig_chown(file, owner, group);
}

int lchown(const char *file, uid_t owner, gid_t group) {
  std::vector<std::string> path_args = {file};
  std::vector<uint64_t> args = {0, owner, group};
  pathauditor::FileEvent file_event{SYS_lchown, args, path_args};

  pathauditor::LibcFileEventIsUserControlled(file_event, "lchown");

  orig_lchown_type orig_lchown;
  orig_lchown = reinterpret_cast<orig_lchown_type>(dlsym(RTLD_NEXT, "lchown"));
  return orig_lchown(file, owner, group);
}

int fchownat(int fd, const char *file, uid_t owner, gid_t group, int flag) {
  std::vector<std::string> path_args = {file};
  std::vector<uint64_t> args = {static_cast<uint64_t>(fd), 0, owner, group,
                              static_cast<uint64_t>(flag)};
  pathauditor::FileEvent file_event{SYS_fchownat, args, path_args};

  pathauditor::LibcFileEventIsUserControlled(file_event, "fchownat");

  orig_fchownat_type orig_fchownat;
  orig_fchownat =
      reinterpret_cast<orig_fchownat_type>(dlsym(RTLD_NEXT, "fchownat"));
  return orig_fchownat(fd, file, owner, group, flag);
}

int execl(const char *path, const char *arg, ...) {
  std::vector<char *> argv;
  argv.push_back(const_cast<char *>(arg));

  va_list va_args;
  va_start(va_args, arg);
  char *va_arg;
  do {
    va_arg = va_arg(va_args, char *);
    argv.push_back(va_arg);
  } while (va_arg != nullptr);
  va_end(va_args);

  std::vector<std::string> path_args = {path};
  std::vector<uint64_t> args = {0, reinterpret_cast<uint64_t>(&argv[0]), 0};
  pathauditor::FileEvent file_event{SYS_execve, args, path_args};
  pathauditor::LibcFileEventIsUserControlled(file_event, "execl");

  // cannot call execl with variable args; call execve instead
  orig_execve_type orig_execve;
  orig_execve = reinterpret_cast<orig_execve_type>(dlsym(RTLD_NEXT, "execve"));
  return orig_execve(path, &argv[0], nullptr);
}

int execv(const char *path, char *const argv[]) {
  std::vector<std::string> path_args = {path};
  std::vector<uint64_t> args = {0, reinterpret_cast<uint64_t>(argv), 0};
  pathauditor::FileEvent file_event{SYS_execve, args, path_args};

  pathauditor::LibcFileEventIsUserControlled(file_event, "execv");

  orig_execv_type orig_execv;
  orig_execv = reinterpret_cast<orig_execv_type>(dlsym(RTLD_NEXT, "execv"));
  return orig_execv(path, argv);
}

int execve(const char *path, char *const argv[], char *const envp[]) {
  std::vector<std::string> path_args = {path};
  std::vector<uint64_t> args = {0, reinterpret_cast<uint64_t>(argv),
                              reinterpret_cast<uint64_t>(envp)};
  pathauditor::FileEvent file_event{SYS_execve, args, path_args};

  pathauditor::LibcFileEventIsUserControlled(file_event, "execve");

  orig_execve_type orig_execve;
  orig_execve = reinterpret_cast<orig_execve_type>(dlsym(RTLD_NEXT, "execve"));
  return orig_execve(path, argv, envp);
}

int execle(const char *path, const char *arg, ...) {
  // same as execl but last argument is envp
  orig_execle_type orig_execle;
  orig_execle = reinterpret_cast<orig_execle_type>(dlsym(RTLD_NEXT, "execle"));
  return orig_execle(path, arg);
}

int execvp(const char *file, char *const argv[]) {
  // complicated handling of PATH env var
  orig_execvp_type orig_execvp;
  orig_execvp = reinterpret_cast<orig_execvp_type>(dlsym(RTLD_NEXT, "execvp"));
  return orig_execvp(file, argv);
}

int execlp(const char *file, const char *arg, ...) {
  // complicated handling of PATH env var
  orig_execlp_type orig_execlp;
  orig_execlp = reinterpret_cast<orig_execlp_type>(dlsym(RTLD_NEXT, "execlp"));
  return orig_execlp(file, arg);
}

FILE *fopen(const char *filename, const char *modes) {
  std::vector<std::string> path_args = {filename};
  // the mode doesn't matter for pathauditor
  std::vector<uint64_t> args = {0, O_RDONLY};
  pathauditor::FileEvent file_event(SYS_open, args, path_args);

  pathauditor::LibcFileEventIsUserControlled(file_event, "fopen");

  orig_fopen_type orig_fopen;
  orig_fopen = reinterpret_cast<orig_fopen_type>(dlsym(RTLD_NEXT, "fopen"));
  return orig_fopen(filename, modes);
}

FILE *fopen64(const char *filename, const char *modes) {
  std::vector<std::string> path_args = {filename};
  std::vector<uint64_t> args = {
      0, O_RDONLY};  // the mode doesn't matter for pathauditor
  pathauditor::FileEvent file_event(SYS_open, args, path_args);

  pathauditor::LibcFileEventIsUserControlled(file_event, "fopen64");

  orig_fopen64_type orig_fopen64;
  orig_fopen64 =
      reinterpret_cast<orig_fopen64_type>(dlsym(RTLD_NEXT, "fopen64"));
  return orig_fopen64(filename, modes);
}

FILE *freopen(const char *filename, const char *modes, FILE *stream) {
  std::vector<std::string> path_args = {filename};
  std::vector<uint64_t> args = {
      0, O_RDONLY};  // the mode doesn't matter for pathauditor
  pathauditor::FileEvent file_event(SYS_open, args, path_args);

  pathauditor::LibcFileEventIsUserControlled(file_event, "freopen");

  orig_freopen_type orig_freopen;
  orig_freopen =
      reinterpret_cast<orig_freopen_type>(dlsym(RTLD_NEXT, "freopen"));
  return orig_freopen(filename, modes, stream);
}

FILE *freopen64(const char *filename, const char *modes, FILE *stream) {
  std::vector<std::string> path_args = {filename};
  std::vector<uint64_t> args = {
      0, O_RDONLY};  // the mode doesn't matter for pathauditor
  pathauditor::FileEvent file_event(SYS_open, args, path_args);

  pathauditor::LibcFileEventIsUserControlled(file_event, "fopen64");

  orig_freopen64_type orig_freopen64;
  orig_freopen64 =
      reinterpret_cast<orig_freopen64_type>(dlsym(RTLD_NEXT, "freopen64"));
  return orig_freopen64(filename, modes, stream);
}

int truncate(const char *file, off_t length) {
  std::vector<std::string> path_args = {file};
  std::vector<uint64_t> args = {0, static_cast<uint64_t>(length)};
  pathauditor::FileEvent file_event(SYS_truncate, args, path_args);

  pathauditor::LibcFileEventIsUserControlled(file_event, "truncate");

  orig_truncate_type orig_truncate;
  orig_truncate =
      reinterpret_cast<orig_truncate_type>(dlsym(RTLD_NEXT, "truncate"));
  return orig_truncate(file, length);
}

int truncate64(const char *file, off64_t length) {
  std::vector<std::string> path_args = {file};
  std::vector<uint64_t> args = {0, static_cast<uint64_t>(length)};
  pathauditor::FileEvent file_event(SYS_truncate, args, path_args);

  pathauditor::LibcFileEventIsUserControlled(file_event, "truncate64");
  orig_truncate64_type orig_truncate64;
  orig_truncate64 =
      reinterpret_cast<orig_truncate64_type>(dlsym(RTLD_NEXT, "truncate64"));
  return orig_truncate64(file, length);
}

int mkdir(const char *path, mode_t mode) {
  std::vector<std::string> path_args = {path};
  std::vector<uint64_t> args = {0, mode};
  pathauditor::FileEvent file_event{SYS_mkdir, args, path_args};

  pathauditor::LibcFileEventIsUserControlled(file_event, "mkdir");

  orig_mkdir_type orig_mkdir;
  orig_mkdir = reinterpret_cast<orig_mkdir_type>(dlsym(RTLD_NEXT, "mkdir"));
  return orig_mkdir(path, mode);
}

int mkdirat(int fd, const char *path, mode_t mode) {
  std::vector<std::string> path_args = {path};
  std::vector<uint64_t> args = {static_cast<uint64_t>(fd), 0, mode};
  pathauditor::FileEvent file_event{SYS_mkdirat, args, path_args};

  pathauditor::LibcFileEventIsUserControlled(file_event, "mkdirat");

  orig_mkdirat_type orig_mkdirat;
  orig_mkdirat =
      reinterpret_cast<orig_mkdirat_type>(dlsym(RTLD_NEXT, "mkdirat"));
  return orig_mkdirat(fd, path, mode);
}

int link(const char *from, const char *to) {
  std::vector<std::string> path_args = {from, to};
  std::vector<uint64_t> args = {};
  pathauditor::FileEvent file_event{SYS_link, args, path_args};

  pathauditor::LibcFileEventIsUserControlled(file_event, "link");

  orig_link_type orig_link;
  orig_link = reinterpret_cast<orig_link_type>(dlsym(RTLD_NEXT, "link"));
  return orig_link(from, to);
}

int linkat(int fromfd, const char *from, int tofd, const char *to, int flags) {
  std::vector<std::string> path_args = {from, to};
  std::vector<uint64_t> args = {static_cast<uint64_t>(fromfd), 0,
                              static_cast<uint64_t>(tofd), 0,
                              static_cast<uint64_t>(flags)};
  pathauditor::FileEvent file_event{SYS_linkat, args, path_args};

  pathauditor::LibcFileEventIsUserControlled(file_event, "linkat");

  orig_linkat_type orig_linkat;
  orig_linkat = reinterpret_cast<orig_linkat_type>(dlsym(RTLD_NEXT, "linkat"));
  return orig_linkat(fromfd, from, tofd, to, flags);
}

int unlink(const char *name) {
  std::vector<std::string> path_args = {name};
  std::vector<uint64_t> args = {0};
  pathauditor::FileEvent file_event{SYS_unlink, args, path_args};

  pathauditor::LibcFileEventIsUserControlled(file_event, "unlink");

  orig_unlink_type orig_unlink;
  orig_unlink = reinterpret_cast<orig_unlink_type>(dlsym(RTLD_NEXT, "unlink"));
  return orig_unlink(name);
}

int unlinkat(int dirfd, const char *name, int flags) {
  std::vector<std::string> path_args = {name};
  std::vector<uint64_t> args = {static_cast<uint64_t>(dirfd), 0,
                              static_cast<uint64_t>(flags)};
  pathauditor::FileEvent file_event{SYS_unlinkat, args, path_args};

  pathauditor::LibcFileEventIsUserControlled(file_event, "unlinkat");

  orig_unlinkat_type orig_unlinkat;
  orig_unlinkat =
      reinterpret_cast<orig_unlinkat_type>(dlsym(RTLD_NEXT, "unlinkat"));
  return orig_unlinkat(dirfd, name, flags);
}

int remove(const char *filename) {
  std::vector<std::string> path_args = {filename};

  struct stat stat_buf;
  // different behaviour if directory/regular file
  if (stat(filename, &stat_buf)) {
    std::cerr << "cannot stat " << filename << "\n";
  } else {
    if (S_ISDIR(stat_buf.st_mode)) {
      std::vector<uint64_t> args = {static_cast<uint64_t>(AT_FDCWD), 0,
                                  AT_REMOVEDIR};
      pathauditor::FileEvent file_event{SYS_unlinkat, args, path_args};
      pathauditor::LibcFileEventIsUserControlled(file_event, "remove");
    } else {
      std::vector<uint64_t> args = {0};
      pathauditor::FileEvent file_event{SYS_unlink, args, path_args};
      pathauditor::LibcFileEventIsUserControlled(file_event, "remove");
    }
  }

  orig_remove_type orig_remove;
  orig_remove = reinterpret_cast<orig_remove_type>(dlsym(RTLD_NEXT, "remove"));
  return orig_remove(filename);
}

int rmdir(const char *path) {
  std::vector<std::string> path_args = {path};
  std::vector<uint64_t> args = {static_cast<uint64_t>(AT_FDCWD), 0, AT_REMOVEDIR};
  pathauditor::FileEvent file_event{SYS_unlinkat, args, path_args};

  pathauditor::LibcFileEventIsUserControlled(file_event, "rmdir");

  orig_rmdir_type orig_rmdir;
  orig_rmdir = reinterpret_cast<orig_rmdir_type>(dlsym(RTLD_NEXT, "rmdir"));
  return orig_rmdir(path);
}

int mount(const char *special_file, const char *dir, const char *fstype,
          unsigned rwflag, void *data) {
  if (special_file == nullptr) {
    special_file = "";
  }
  std::vector<std::string> path_args = {special_file, dir};
  std::vector<uint64_t> args = {0, 0, reinterpret_cast<uint64_t>(fstype),
                              static_cast<uint64_t>(rwflag),
                              reinterpret_cast<uint64_t>(data)};
  pathauditor::FileEvent file_event{SYS_mount, args, path_args};

  pathauditor::LibcFileEventIsUserControlled(file_event, "mount");

  orig_mount_type orig_mount;
  orig_mount = reinterpret_cast<orig_mount_type>(dlsym(RTLD_NEXT, "mount"));
  return orig_mount(special_file, dir, fstype, rwflag, data);
}

int umount(const char *special_file) {
  std::vector<std::string> path_args = {special_file};
  std::vector<uint64_t> args = {0, 0};
  pathauditor::FileEvent file_event{SYS_umount2, args, path_args};

  pathauditor::LibcFileEventIsUserControlled(file_event, "umount");

  orig_umount_type orig_umount;
  orig_umount = reinterpret_cast<orig_umount_type>(dlsym(RTLD_NEXT, "umount"));
  return orig_umount(special_file);
}

int umount2(const char *special_file, int flags) {
  std::vector<std::string> path_args = {special_file};
  std::vector<uint64_t> args = {0, static_cast<uint64_t>(flags)};
  pathauditor::FileEvent file_event{SYS_umount2, args, path_args};

  pathauditor::LibcFileEventIsUserControlled(file_event, "umount2");

  orig_umount2_type orig_umount2;
  orig_umount2 =
      reinterpret_cast<orig_umount2_type>(dlsym(RTLD_NEXT, "umount2"));
  return orig_umount2(special_file, flags);
}

int rename(const char *oldpath, const char *newpath) {
  std::vector<std::string> path_args = {oldpath, newpath};
  std::vector<uint64_t> args = {};
  pathauditor::FileEvent file_event{SYS_rename, args, path_args};

  pathauditor::LibcFileEventIsUserControlled(file_event, "rename");

  orig_rename_type orig_rename;
  orig_rename = reinterpret_cast<orig_rename_type>(dlsym(RTLD_NEXT, "rename"));
  return orig_rename(oldpath, newpath);
}

int renameat(int olddirfd, const char *oldpath, int newdirfd,
             const char *newpath) {
  std::vector<std::string> path_args = {oldpath, newpath};
  std::vector<uint64_t> args = {static_cast<uint64_t>(olddirfd), 0,
                              static_cast<uint64_t>(newdirfd), 0};
  pathauditor::FileEvent file_event{SYS_renameat, args, path_args};

  pathauditor::LibcFileEventIsUserControlled(file_event, "renameat");

  orig_renameat_type orig_renameat;
  orig_renameat =
      reinterpret_cast<orig_renameat_type>(dlsym(RTLD_NEXT, "renameat"));
  return orig_renameat(olddirfd, oldpath, newdirfd, newpath);
}

int symlink(const char *from, const char *to) {
  std::vector<std::string> path_args = {from, to};
  std::vector<uint64_t> args = {};
  pathauditor::FileEvent file_event{SYS_symlink, args, path_args};

  pathauditor::LibcFileEventIsUserControlled(file_event, "symlink");

  orig_symlink_type orig_symlink;
  orig_symlink =
      reinterpret_cast<orig_symlink_type>(dlsym(RTLD_NEXT, "symlink"));
  return orig_symlink(from, to);
}

int symlinkat(const char *from, int newdirfd, const char *to) {
  std::vector<std::string> path_args = {from, to};
  std::vector<uint64_t> args = {0, static_cast<uint64_t>(newdirfd), 0};
  pathauditor::FileEvent file_event{SYS_symlinkat, args, path_args};

  pathauditor::LibcFileEventIsUserControlled(file_event, "symlinkat");

  orig_symlinkat_type orig_symlinkat;
  orig_symlinkat =
      reinterpret_cast<orig_symlinkat_type>(dlsym(RTLD_NEXT, "symlinkat"));
  return orig_symlinkat(from, newdirfd, to);
}

int chroot(const char *path) {
  std::vector<std::string> path_args = {path};
  std::vector<uint64_t> args = {0};
  pathauditor::FileEvent file_event{SYS_chroot, args, path_args};

  pathauditor::LibcFileEventIsUserControlled(file_event, "chroot");

  orig_chroot_type orig_chroot;
  orig_chroot = reinterpret_cast<orig_chroot_type>(dlsym(RTLD_NEXT, "chroot"));
  return orig_chroot(path);
}
}
