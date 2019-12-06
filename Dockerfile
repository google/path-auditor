FROM ubuntu:19.04

# We log to syslog, install it
RUN apt-get update && apt-get -y install syslog-ng

# Install bazel
RUN apt-get -y install pkg-config zip g++ zlib1g-dev unzip python3 wget
RUN wget https://github.com/bazelbuild/bazel/releases/download/1.1.0/bazel-1.1.0-installer-linux-x86_64.sh && chmod u+x bazel-1.1.0-installer-linux-x86_64.sh && ./bazel-1.1.0-installer-linux-x86_64.sh

COPY . /pathauditor/

RUN cd /pathauditor && bazel build //pathauditor/libc:libpath_auditor.so

CMD service syslog-ng start && /bin/bash
