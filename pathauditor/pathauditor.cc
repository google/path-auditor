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

#include "pathauditor/pathauditor.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/magic.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <unistd.h>
// include fs.h last since it clashes with mount.h
#include <linux/fs.h>

#include <cstdint>
#include <deque>

#include <glog/logging.h>
#include "pathauditor/util/path.h"
#include "pathauditor/util/cleanup.h"
#include "absl/container/fixed_array.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "pathauditor/util/canonical_errors.h"
#include "pathauditor/util/status.h"
#include "pathauditor/util/status_macros.h"

#ifndef FS_IOC_GETFLAGS
#define FS_IOC_GETFLAGS 0x80086601
#endif

#ifndef FS_IOC_SETFLAGS
#define FS_IOC_SETFLAGS 0x40086602
#endif

#ifndef FS_IMMUTABLE_FL
#define FS_IMMUTABLE_FL 16
#endif

#ifndef CGROUP2_SUPER_MAGIC
#define CGROUP2_SUPER_MAGIC 0x63677270
#endif

namespace pathauditor {

namespace {

// O_PATH is not enough since we want to check the immutable flag and the ioctl
// fails with an O_PATH file descriptor.
constexpr int kDirOpenFlags = O_RDONLY;

StatusOr<bool> FdIsImmutable(int fd) {
  int32_t flags;
  if (ioctl(fd, FS_IOC_GETFLAGS, &flags) < 0) {
    if (errno == ENOTTY) {
      // ENOTTY is returned if the filesystem doesn't support this option
      return false;
    }
    return FailedPreconditionError("ioctl(FS_IOC_GETFLATS) failed.");
  }
  return flags & FS_IMMUTABLE_FL;
}

StatusOr<int> ResolveDirFd(const ProcessInformation &proc_info,
                                 absl::string_view path,
                                 absl::optional<int> at_fd) {
  int dir_fd;
  if (IsAbsolutePath(path)) {
    PATHAUDITOR_ASSIGN_OR_RETURN(dir_fd, proc_info.RootFileDescriptor(kDirOpenFlags));
  } else if (!at_fd.has_value() || at_fd == AT_FDCWD) {
    PATHAUDITOR_ASSIGN_OR_RETURN(dir_fd, proc_info.CwdFileDescriptor(kDirOpenFlags));
  } else {
    PATHAUDITOR_ASSIGN_OR_RETURN(
        dir_fd, proc_info.DupDirFileDescriptor(at_fd.value(), kDirOpenFlags));
  }

  return dir_fd;
}

StatusOr<bool> FileIsUserWritable(
    const ProcessInformation &proc_info, absl::string_view file,
    absl::optional<int> at_fd = absl::optional<int>()) {
  PATHAUDITOR_ASSIGN_OR_RETURN(int dir_fd, ResolveDirFd(proc_info, file, at_fd));
  auto close_dir_fd = MakeCleanup([&dir_fd]() { close(dir_fd); });

  struct stat sb;
  if (fstatat(dir_fd, std::string(file).c_str(), &sb, 0) == -1) {
    if (errno != ENOENT) {
      return FailedPreconditionError(
          absl::StrCat("Couldn't fstatat ", file));
    }
    // The file doesn't exist so it's not writable
    return false;
  }

  // is not a regular file
  if (!S_ISREG(sb.st_mode)) {
    return false;
  }
  // is not owned by root
  if (sb.st_uid != 0) {
    return true;
  }
  // is not owned by the root group and is group writable or is writable by
  // others
  if ((sb.st_gid != 0 && sb.st_mode & S_IWGRP) || sb.st_mode & S_IWOTH) {
    return true;
  }

  return false;
}

StatusOr<bool> FileIsUserControlled(int dir_fd, absl::string_view file) {
  // Filter out special files
  if (file == "." || file == "..") {
    return false;
  }

  // if either the dir or the file are immutable the access is safe
  PATHAUDITOR_ASSIGN_OR_RETURN(bool dir_is_immutable, FdIsImmutable(dir_fd));
  if (dir_is_immutable) {
    return false;
  }

  int file_fd = openat(dir_fd, std::string(file).c_str(), O_RDONLY);
  if (file_fd == -1) {
    if (errno != ENOENT) {
      return FailedPreconditionError(
          absl::StrCat("Couldn't open file for immutable check ", file));
    }
  } else {
    PATHAUDITOR_ASSIGN_OR_RETURN(bool file_is_immutable, FdIsImmutable(file_fd));
    close(file_fd);
    if (file_is_immutable) {
      return false;
    }
  }

  struct statfs fs_buf;
  if (fstatfs(dir_fd, &fs_buf) == -1) {
    return FailedPreconditionError("fstatfs(dir_fd) failed");
  }

  // ignore proc and cgroup filesystems
  if (fs_buf.f_type == PROC_SUPER_MAGIC ||
      fs_buf.f_type == CGROUP_SUPER_MAGIC ||
      fs_buf.f_type == CGROUP2_SUPER_MAGIC) {
    return false;
  }

  struct stat sb;
  if (fstat(dir_fd, &sb) == -1) {
    return FailedPreconditionError("fstat(dir_fd) failed");
  }

  // non-root owner
  if (sb.st_uid != 0) {
    return true;
  }

  // root owned dir that is writable by a user
  if ((sb.st_gid != 0 && sb.st_mode & S_IWGRP) || sb.st_mode & S_IWOTH) {
    // if not sticky the file is controlled
    if (!(sb.st_mode & S_ISVTX)) {
      return true;
    }

    // For sticky dirs you can only replace a file if you're the directory owner
    // or the owner of the file.
    // We already checked above if the directory is user owned.
    // This leaves the cases where the file is user owned or non-existent.

    // check if the file is owned by non-root
    struct stat next_sb;
    if (fstatat(dir_fd, std::string(file).c_str(), &next_sb, AT_SYMLINK_NOFOLLOW) ==
        -1) {
      if (errno != ENOENT) {
        return FailedPreconditionError(
            absl::StrCat("Couldn't fstatat ", file));
      }
      // The file doesn't exist but it could be created by a user
      return true;
    }
    if (next_sb.st_uid != 0) {
      return true;
    }
  }

  return false;
}

}  // namespace

// The algorithm is roughly:
// * keep a fd open to the current directory we're in
//  * absolute path => open(/)
//  * AT_FDCWD => open(.)
// * iterate over the path segments
//  * dir => check perms and enter
//  * relative link => prepend to remaining path
//  * absolute link => prepend to remaining path and start at /
StatusOr<bool> PathIsUserControlled(const ProcessInformation &proc_info,
                                          absl::string_view path,
                                          absl::optional<int> at_fd,
                                          unsigned int max_iteration_count) {
  PATHAUDITOR_ASSIGN_OR_RETURN(int dir_fd, ResolveDirFd(proc_info, path, at_fd));
  auto close_dir_fd = MakeCleanup([&dir_fd]() { close(dir_fd); });

  std::deque<std::string> path_queue = absl::StrSplit(path, '/', absl::SkipEmpty());

  for (size_t i = 0; i < max_iteration_count; i++) {
    if (path_queue.empty()) {
      return false;
    }

    std::string elem = path_queue.front();
    path_queue.pop_front();

    if (elem == ".") {
      continue;
    }

    // Check if the next path element is user controlled
    PATHAUDITOR_ASSIGN_OR_RETURN(bool access_is_unsafe, FileIsUserControlled(dir_fd, elem));
    if (access_is_unsafe) {
      return true;
    }

    // Check if the element actually exists. We need to check after
    // FileIsUserControlled since a non-existent file could still be created
    // by a user if the directory is writable.
    struct stat sb;
    if (fstatat(dir_fd, elem.c_str(), &sb, AT_SYMLINK_NOFOLLOW) == -1) {
      if (errno != ENOENT) {
        return FailedPreconditionError(
            absl::StrCat("Could not stat path element ", elem));
      } else {
        return false;
      }
    }

    // Symlinks in /proc are magic. We can just follow them in the stat call.
    // If the file is a symlink and the current directory is in proc, just
    // follow the symlink instead.
    if ((sb.st_mode & S_IFMT) == S_IFLNK) {
      struct statfs fs_buf;
      if (fstatfs(dir_fd, &fs_buf) == -1) {
        return FailedPreconditionError("fstatfs(dir_fd) failed");
      }

      if (fs_buf.f_type == PROC_SUPER_MAGIC) {
        if (fstatat(dir_fd, elem.c_str(), &sb, 0) == -1) {
          return FailedPreconditionError(absl::StrCat(
              "Could not stat path element without nofollow", elem));
        }
      }
    }

    switch (sb.st_mode & S_IFMT) {
      case S_IFDIR: {
        // Change into the directory
        int new_fd = openat(dir_fd, elem.c_str(), kDirOpenFlags);
        if (new_fd == -1) {
          return FailedPreconditionError(
              absl::StrCat("Couldn't openat next elem ", elem));
        }
        close(dir_fd);
        dir_fd = new_fd;
        break;
      }
      case S_IFLNK: {
        // Read the link and prepend the result to our path queue
        absl::FixedArray<char> link_buf(PATH_MAX);
        ssize_t link_len = readlinkat(dir_fd, elem.c_str(), link_buf.data(),
                                      link_buf.memsize());
        if (link_len == -1) {
          return FailedPreconditionError(
              absl::StrCat("Could not read link for path element ", elem));
        }
        if ((size_t)link_len >= link_buf.memsize()) {
          return FailedPreconditionError(
              absl::StrCat("Link is larger than PATH_MAX ",
                           std::string(link_buf.data(), link_buf.memsize())));
        }
        link_buf[link_len] = 0;
        // If the path is absolute, change to /
        if (IsAbsolutePath(link_buf.data())) {
          PATHAUDITOR_ASSIGN_OR_RETURN(int new_fd,
                           proc_info.RootFileDescriptor(kDirOpenFlags));
          close(dir_fd);
          dir_fd = new_fd;
        }
        // prepend the link elements to our path queue
        std::deque<std::string> link_queue =
            absl::StrSplit(link_buf.data(), '/', absl::SkipEmpty());
        path_queue.insert(path_queue.begin(), link_queue.begin(),
                          link_queue.end());
        break;
      }
      default:
        if (!path_queue.empty()) {
          return FailedPreconditionError(
              "Non-directory in middle of path.");
        }
        return false;
    }
  }

  return ResourceExhaustedError(
      absl::StrCat("Ran into max iteration count ", max_iteration_count));
}

StatusOr<bool> FileEventIsUserControlled(
    const ProcessInformation &proc_info, const FileEvent &event) {
  PATHAUDITOR_ASSIGN_OR_RETURN(std::string path, event.PathArg(0));

  absl::optional<uint64_t> fd_arg;
  bool skip_last_element = false;

  switch (event.syscall_nr) {
    case SYS_chmod:
    case SYS_chown:
    case SYS_chdir:
    case SYS_rmdir:
    case SYS_uselib:
    case SYS_swapon:
    case SYS_chroot:
    case SYS_creat:  // creat == open(O_CREAT|O_WRONLY|O_TRUNC)
    case SYS_truncate:
      break;
    case SYS_unlink:
    case SYS_mknod:
    case SYS_mkdir:
    case SYS_lchown:
      // These syscalls don't follow symlinks
      skip_last_element = true;
      break;
    case SYS_unlinkat:
    case SYS_mknodat:
    case SYS_mkdirat: {
      PATHAUDITOR_ASSIGN_OR_RETURN(fd_arg, event.Arg(0));
      skip_last_element = true;
      break;
    }
    case SYS_open: {
      PATHAUDITOR_ASSIGN_OR_RETURN(int flags, event.Arg(1));
      if (flags & O_NOFOLLOW || flags & O_EXCL) {
        skip_last_element = true;
      }
      break;
    }
    case SYS_openat: {
      PATHAUDITOR_ASSIGN_OR_RETURN(fd_arg, event.Arg(0));
      PATHAUDITOR_ASSIGN_OR_RETURN(int flags, event.Arg(2));
      if (flags & O_NOFOLLOW || flags & O_EXCL) {
        skip_last_element = true;
      }
      break;
    }
    case SYS_fchmodat: {
      PATHAUDITOR_ASSIGN_OR_RETURN(fd_arg, event.Arg(0));
      // fchmodat has a no follow flag, but it's not used
      break;
    }
    case SYS_fchownat: {
      PATHAUDITOR_ASSIGN_OR_RETURN(fd_arg, event.Arg(0));
      PATHAUDITOR_ASSIGN_OR_RETURN(int flags, event.Arg(4));
      if (flags & AT_EMPTY_PATH && path.empty()) {
        return false;
      }
      if (flags & AT_SYMLINK_NOFOLLOW) {
        skip_last_element = true;
      }
      break;
    }
#ifdef SYS_execveat
    case SYS_execveat: {
      PATHAUDITOR_ASSIGN_OR_RETURN(fd_arg, event.Arg(0));
      PATHAUDITOR_ASSIGN_OR_RETURN(int flags, event.Arg(4));
      if (flags & AT_EMPTY_PATH && path.empty()) {
        return false;
      }
      StatusOr<bool> result = FileIsUserWritable(proc_info, path, fd_arg);
      if (result.ok() && *result) {
        return true;
      }
      if (flags & AT_SYMLINK_NOFOLLOW) {
        skip_last_element = true;
      }
      break;
    }
#endif
    case SYS_execve: {
      StatusOr<bool> result = FileIsUserWritable(proc_info, path);
      if (result.ok() && *result) {
        return true;
      }
      break;
    }
    case SYS_umount2: {
      PATHAUDITOR_ASSIGN_OR_RETURN(int flags, event.Arg(1));
      if (flags & UMOUNT_NOFOLLOW) {
        skip_last_element = true;
      }
      break;
    }
    case SYS_name_to_handle_at: {
      PATHAUDITOR_ASSIGN_OR_RETURN(int flags, event.Arg(4));
      if (flags & AT_EMPTY_PATH && path.empty()) {
        return false;
      }
      if (!(flags & AT_SYMLINK_FOLLOW)) {
        skip_last_element = true;
      }
      break;
    }
    case SYS_rename: {
      skip_last_element = true;
      PATHAUDITOR_ASSIGN_OR_RETURN(std::string other_path, event.PathArg(1));
      StatusOr<bool> result =
          PathIsUserControlled(proc_info, Dirname(other_path));
      if (result.ok() && *result) {
        return true;
      }
      break;
    }
    case SYS_renameat:
    case SYS_renameat2: {
      skip_last_element = true;
      PATHAUDITOR_ASSIGN_OR_RETURN(fd_arg, event.Arg(0));
      PATHAUDITOR_ASSIGN_OR_RETURN(int new_fd, event.Arg(2));
      PATHAUDITOR_ASSIGN_OR_RETURN(std::string new_path, event.PathArg(1));
      StatusOr<bool> result =
          PathIsUserControlled(proc_info, Dirname(new_path), new_fd);
      if (result.ok() && *result) {
        return true;
      }

      break;
    }
    case SYS_link: {
      PATHAUDITOR_ASSIGN_OR_RETURN(std::string newpath, event.PathArg(1));
      StatusOr<bool> result =
          PathIsUserControlled(proc_info, Dirname(newpath));
      if (result.ok() && *result) {
        return true;
      }
      break;
    }
    case SYS_symlink: {
      PATHAUDITOR_ASSIGN_OR_RETURN(std::string newpath, event.PathArg(1));
      StatusOr<bool> result =
          PathIsUserControlled(proc_info, Dirname(newpath));
      if (result.ok() && *result) {
        return true;
      }
      // no checks on link target
      return false;
    }
    case SYS_linkat: {
      PATHAUDITOR_ASSIGN_OR_RETURN(fd_arg, event.Arg(0));
      PATHAUDITOR_ASSIGN_OR_RETURN(std::string newpath, event.PathArg(1));
      PATHAUDITOR_ASSIGN_OR_RETURN(int newdirfd, event.Arg(2));
      PATHAUDITOR_ASSIGN_OR_RETURN(int flags, event.Arg(4));

      StatusOr<bool> result =
          PathIsUserControlled(proc_info, Dirname(newpath), newdirfd);
      if (result.ok() && *result) {
        return true;
      }

      if (flags & AT_EMPTY_PATH && path.empty()) {
        return false;
      }

      if (!(flags & AT_SYMLINK_FOLLOW)) {
        skip_last_element = true;
      }

      break;
    }
    case SYS_symlinkat: {
      PATHAUDITOR_ASSIGN_OR_RETURN(std::string newpath, event.PathArg(1));
      PATHAUDITOR_ASSIGN_OR_RETURN(int newdirfd, event.Arg(1));
      StatusOr<bool> result =
          PathIsUserControlled(proc_info, Dirname(newpath), newdirfd);
      if (result.ok() && *result) {
        return true;
      }
      // no checks on link target
      return false;
    }
    case SYS_mount: {
      std::string source = path;
      PATHAUDITOR_ASSIGN_OR_RETURN(std::string target, event.PathArg(1));
      PATHAUDITOR_ASSIGN_OR_RETURN(int flags, event.Arg(3));

      StatusOr<bool> result = PathIsUserControlled(proc_info, target);
      if (result.ok() && *result) {
        return true;
      }

      if (!(flags & (MS_BIND | MS_MOVE))) {
        // only check the source if MS_BIND or MS_MOVE is set
        return false;
      }

      break;
    }
    default:
      LOG(ERROR) << "Unexpected syscall nr: " << event.syscall_nr;
      return UnimplementedError(
          absl::StrCat("No support for syscall ", event.syscall_nr));
  }

  if (skip_last_element) {
    path = std::string(Dirname(path));
  }

  return PathIsUserControlled(proc_info, path, fd_arg);
}

}  // namespace pathauditor
