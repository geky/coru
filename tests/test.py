#!/usr/bin/env python

import re
import sys
import subprocess
import os

def fail(message, info=None):
    sys.stdout.write('\033[31m%s\033[0m\n' % message)
    if info:
        sys.stdout.write('\n%s' % info)
    sys.exit(2)

def generate(test):
    with open("tests/template.fmt") as file:
        template = file.read()

    lines = []
    for line in re.split('(?<=(?:.;| [{}]))\n', test.read()):
        match = re.match('(?: *\n)*( *)(.*)=>(.*);', line, re.DOTALL | re.MULTILINE)
        if match:
            tab, test, expect = match.groups()
            lines.append(tab+'test_res = {test};'.format(test=test.strip()))
            lines.append(tab+'test_asserteqm(test_res, {expect}, "{test}");'.format(
                    test = test.strip().replace('\n', '\\n'),
                    expect = expect.strip()))
        else:
            lines.append(line)

    # Create test file
    with open('test.c', 'w') as file:
        file.write(template.format(test='\n'.join(lines)))

    # Remove build artifacts to force rebuild
    try:
        os.remove('test.o')
        os.remove('lfs')
    except OSError:
        pass

def compile():
    err = subprocess.call([
            os.environ.get('MAKE', 'make'),
            '--no-print-directory', '-s'])
    if err:
        fail("Could not compile")

def execute(expect_error=False):
    if 'EXEC' in os.environ:
        cmd = [os.environ['EXEC'], "./coru"]
    else:
        cmd = ["./coru"]

    # create pipe to listen to expect stream (fd 3)
    os.pipe()
    os.dup2(3, 5)
    os.dup2(4, 3)
    os.dup2(5, 4)
    os.close(5)
    expect = os.fdopen(4, "r")

    proc = subprocess.Popen(cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)

    # yes this is bad, but none of our tests output enough data to
    # need to buffer, may need to fix this in the future
    proc.wait()
    os.close(3)

    stdout = proc.stdout.read()
    stderr = proc.stderr.read()
    expect = expect.read()

    if (proc.returncode == 0) == expect_error:
        fail("Expected %s but test exited with %d" % (
                "error" if expect_error else "no error", proc.returncode),
            "stderr:\n%s\n" % stderr)

    if stdout != expect:
        fail("Output does not match expected output",
            "expected:\n%s\n" % expect +
            "actual:\n%s\n" % stdout)

def main(*args):
    test = [a for a in args if not a.startswith('-')]
    if test:
        with open(test[0]) as file:
            generate(file)
    else:
        generate(sys.stdin)

    compile()

    if '-s' in args:
        sys.exit(1)

    execute(expect_error=('-e' in args))

if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:]))
