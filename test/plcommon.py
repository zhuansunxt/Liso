#!/usr/bin/python

import sys
from subprocess import Popen, PIPE, STDOUT

def check_output(args, shouldPrint=True):
    return check_both(args, shouldPrint)[0]

def check_both(args, shouldPrint=True, check=True):
    out = ""
    p = Popen(args, shell=True, stdout=PIPE, stderr=STDOUT)
    while True:
        line = p.stdout.readline()
        if not line:
            break
        if shouldPrint: 
            sys.stdout.write(line)
        out += line
    rc = p.wait()
    out = (out, "")
    out = (out, rc)
    if check and rc is not 0:
        #print "Error processes output: %s" % (out,)
        raise Exception("subprocess.CalledProcessError: Command '%s'" \
                            "returned non-zero exit status %s" % (args, rc))
    return out
