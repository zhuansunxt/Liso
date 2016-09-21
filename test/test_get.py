#!/usr/bin/env python

import datetime
import requests
import os
import time
from grader import grader, tester
import hashlib
import random
import lowlevelhttptests
from subprocess import Popen, PIPE, STDOUT
import os.path
import socket
import sys
from grader1cp2 import project1cp2tester, project1cp2grader

  
BIN = "./lisod"

MIME = {
    '.html' : 'text/html',
    '.css'  : 'text/css',
    '.png'  : 'image/png',
    '.jpg'  : 'image/jpeg',
    '.gif'  : 'image/gif',
    ''      : 'application/octet-stream'
}

if __name__ == '__main__':
    if len(sys.argv) < 3:
        sys.stderr.write('Usage: ./test_get.py <ip> <port>')
        sys.exit(1)
    p1cp2grader = project1cp2grader("checkpoint-2")
    p1cp2grader.addTestSuite('test_GET')
    p1cp2grader.setUp(sys.argv)
    results = p1cp2grader.runTests()
    p1cp2grader.reportScores()