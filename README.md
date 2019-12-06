# PathAuditor

The PathAuditor is a tool meant to find file access related vulnerabilities by
auditing libc functions.

The idea is roughly as follows:

*   Audit every call to filesystem related libc functions performed by the
    binary.
*   Check if the path used in the syscall is user-writable. In this case an
    unprivileged user could have replaced a directory or file with a symlink.
*   Log all violations as potential vulnerabilities.

We're using LD\_PRELOAD to hook all filesystem related library calls and log
any encountered violations to syslog.

This is not an officially supported Google product.

## Example Vulnerability

Let's look at an example of the kind of vulnerability that this tool can detect.
[CVE-2019-3461](https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2019-3461)
was a bug in tmpreaper, a tool that traverses /tmp and deletes old files. It's
usually run as a cron job as root. Since it doesn't want to delete files
outside of tmp, it was using the following code to check if a directory is a
mount point:

```c
if (S_ISDIR (sb.st_mode)) {
    char *dst;

    if ((dst = malloc(strlen(ent->d_name) + 3)) == NULL)
        message (LOG_FATAL, "malloc failed.\n");
    strcpy(dst, ent->d_name);
    strcat(dst, "/X");
    rename(ent->d_name, dst);
    if (errno == EXDEV) {
        free(dst);
        message (LOG_VERBOSE,
                 "File on different device skipped: `%s/%s'\n",
                 dirname, ent->d_name);
        continue;
    }
    // [...]
```

In short, this code calls `rename("/tmp/foo", "/tmp/foo/x")` which will return
`EXDEV` if `"/tmp/foo"` is a mount point.
PathAuditor would flag this call as a potential vulnerability if `"/tmp/foo"`
is owned by any user except root.
To understand why, we have to think about what happens in the kernel when the
rename syscall is executed (simplified):

1. The kernel traverses the path `"/tmp/foo"` for the first argument.
2. The kernel traverses the path `"/tmp/foo/x"` for the second argument.
3. If the source and target are on different filesystems, return EXDEV.
4. Otherwise, move the file from the first to the second directory.

There's a race condition here since `"/tmp/foo"` will be resolved twice. If it's
user-controlled, the user can replace it with a different file at any point in
time.
In particular, we want `"/tmp/foo"` to be a directory at first to pass the
`if(S_ISDIR)` check in the tmpreaper code. We then replace it with a file just
before the code enters the syscall. When the kernel resolves the first argument,
it will see a file with user-controlled content.
Now we replace it again, this time with a symlink to an arbitrary directory on
the same filesystem. The kernel will resolve the path a second time, follow the
symlink and move the controlled file to a chosen directory.

The same filesystem restriction is because rename does not work between
filesystems. But on some Linux distributions /tmp is just a folder on the rootfs
by default and you could use this bug to move a file to /etc/cron, which will
get executed as root.

## How to run

To try it out, you need to build libpath\_auditor.so with [bazel](https://bazel.build)
and load it into a binary using LD\_PRELOAD. Any violations will be logged to
syslog, so make sure that you have it running.

```sh
bazel build //pathauditor/libc:libpath_auditor.so
LD_PRELOAD=/path/to/bazel-bin/pathauditor/libc/libpath_auditor.so cat /tmp/foo/bar
tail /var/log/syslog
```

It's also possible to run this on all processes on the system by adding it to
/etc/ld.so.preload. Though be warned that this is only recommended on test
systems as it can lead to instability.

As a quickstart, you can try out the docker container shipped with this project:

```sh
docker build -t pathauditor-example .
docker run -it pathauditor-example
# LD_PRELOAD=/pathauditor/bazel-bin/pathauditor/libc/libpath_auditor.so cat /tmp/foo/bar
# cat /var/log/syslog
```
