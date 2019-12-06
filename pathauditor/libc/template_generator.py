# Copyright 2019 Google LLC. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

#!/usr/bin/python3
"""A utility script that generates libc function stubs.

It automates the process of writing a wrapper for libc functions that are
relevant for PathAuditor.

run with: python3 template_generator.py path_auditor_libc.cc

IMPORTANT: the script depends on a file from a third-party library that's not
imported into Google's third_party: https://github.com/zachriggle/functions
"""

import argparse
from functions import functions


def argname(arg):
  """Format illegal argument names (like 'new')."""
  name = arg.name
  if arg.name == 'new' or arg.name == 'old':
    name += 'path'

  return name


def args(func):
  """Return a list of function's arguments with types."""
  arg_strings = []
  for arg in func.args:
    name = argname(arg)

    if name == 'vararg':
      arg_strings.append('...')
    elif arg.type == 'char' and arg.derefcnt == 1:
      arg_strings.append(f'const char *{name}')
    elif arg.type == 'char' and arg.derefcnt == 2:
      arg_strings.append(f'char *const {name}[]')
    else:
      arg_strings.append(f'{arg.type} {"*"*arg.derefcnt}{name}')

  return arg_strings


def typedef(func):
  """Produce a typedef string for a function."""
  return (f'typedef {func.type}{"*"*func.derefcnt} (*orig_{func.name}_type)'
          f'({", ".join(args(func))});')


def dlsym_call(func):
  """Produce a string with a dlsym call and the original function call."""
  fun_args = ', '.join(argname(arg) for arg in func.args)
  return (f'  orig_{func.name}_type orig_{func.name};\n'
          f'  orig_{func.name} = reinterpret_cast<orig_{func.name}_type>'
          f'(dlsym(RTLD_NEXT, \"{func.name}\"));\n'
          f'  return orig_{func.name}({fun_args});')


def signature(func):
  """Produce a string with the function's signature and body."""
  return (f'{func.type}{"*"*func.derefcnt} {func.name}({", ".join(args(func))})'
          ' {\n'
          f'{dlsym_call(func)}\n'
          '}')


def main():
  parser = argparse.ArgumentParser(description='Generate libc function stubs')
  parser.add_argument('file', type=str, help='Output file')

  cmdline_args = parser.parse_args()

  func_names = [
      'open',
      'open64',
      'openat',
      'openat64',
      'creat',
      'creat64',
      'chdir',
      'chmod',
      'chown',
      'execl',
      'execve',
      'execle',
      'execvp',
      'execlp',
      'fopen',
      'fopen64',
      'freopen',
      'freopen64',
      'truncate',
      'truncate64',
      'mkdir',
      'link',
      'linkat',
      'unlink',
      'unlinkat',
      'chroot',
      'lchown',
      'remove',
      'rmdir',
      'mount',
      'umount',
      'umount2',
      'rename',
      'renameat',
      'symlink',
      'symlinkat',
      'mkdirat',
      'fchmodat',
      'fchownat',
  ]

  func_info = [functions[name] for name in func_names]
  assert len(func_names) == len(func_info)

  with open(cmdline_args.file, 'w') as f:
    f.write('#define _GNU_SOURCE\n'
            '#include <sys/types.h>\n'
            '#include <stdio.h>\n'
            '#include <dlfcn.h>\n\n')

    for func in func_info:
      f.write(typedef(func) + '\n')

    f.write('\nextern \"C\" {\n')
    for func in func_info:
      f.write(signature(func) + '\n\n')
    f.write('}')


if __name__ == '__main__':
  main()
