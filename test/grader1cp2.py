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

BIN = "./lisod"

MIME = {
    '.html' : 'text/html',
    '.css'  : 'text/css',
    '.png'  : 'image/png',
    '.jpg'  : 'image/jpeg',
    '.gif'  : 'image/gif',
    ''      : 'application/octet-stream'
}


class project1cp2tester(tester):

    def __init__(self, test_name, testsuit):
        super(project1cp2tester, self).__init__(test_name, testsuit)

    def check_headers(self, response_type, headers, length_content, ext):
        self.pAssertEqual(headers['Server'].lower(), 'liso/1.0')
        try:
            datetime.datetime.strptime(headers['Date'],\
             '%a, %d %b %Y %H:%M:%S %Z')
        except KeyError:
            print 'Bad Date header'
        except Exception:
            print 'Bad Date header: %s' % (headers['Date'])
        
        self.pAssertEqual(int(headers['Content-Length']), length_content)
        #self.pAssertEqual(headers['Connection'].lower(), 'close')

        if response_type == 'GET' or response_type == 'HEAD':
            header_set = set(['connection', 'content-length',
                              'date', 'last-modified',
                              'server', 'content-type'])
            self.pAssertEqual(set(), header_set - set(headers.keys()))
            if headers['Content-Type'].lower() != MIME[ext]:
                print 'MIME got %s expected %s'\
                 % (headers['Content-Type'].lower(), MIME[ext])
            self.pAssertTrue(headers['Content-Type'].lower() == MIME[ext]\
                 or headers['Content-Type'].lower() == MIME['.html'])

            try:
                datetime.datetime.strptime(headers['Last-Modified'],\
                '%a, %d %b %Y %H:%M:%S %Z')
            except Exception:
                print 'Bad Last-Modified header: %s' \
                % (headers['Last-Modified'])
        elif response_type == 'POST':
            header_set = set(['connection', 'content-length',
                              'date', 'server'])
            self.pAssertEqual(set(), header_set - set(headers.keys()))
        else:
            self.fail('Unsupported Response Type...')

    def test_HEAD_headers(self):
        print '----- Testing Headers -----'
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        time.sleep(1)
        for test in self.testsuite.tests:
            dummy_root, ext = os.path.splitext(test)
            response = requests.head(test % (self.testsuite.ip, self.testsuite.port), timeout=10.0)
            self.check_headers(response.request.method,
                               response.headers,
                               self.testsuite.tests[test][1],
                               ext)
        self.testsuite.scores['test_HEAD_headers'] = 1

    def test_HEAD(self):
        print '----- Testing HEAD -----'
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        time.sleep(1)
        for test in self.testsuite.tests:
            response = requests.head(test % (self.testsuite.ip, self.testsuite.port), timeout=10.0)
            self.pAssertEqual(200, response.status_code)
        self.testsuite.scores['test_HEAD'] = 1
        if not lowlevelhttptests.check_correct_HEAD(self.testsuite.ip,\
                                                    self.testsuite.port):
            self.testsuite.scores['test_HEAD'] -= 0.5
            print "HEAD must not return Content!"

    def test_GET(self):
        print '----- Testing GET -----'
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        time.sleep(1)
        for test in self.testsuite.tests:
            response = requests.get(test % (self.testsuite.ip, self.testsuite.port), timeout=10.0)
            contenthash = hashlib.sha256(response.content).hexdigest()
            self.pAssertEqual(200, response.status_code)
            self.pAssertEqual(contenthash, self.testsuite.tests[test][0])
        self.testsuite.scores['test_GET'] = 1
        err, res = lowlevelhttptests.check_correct_GET(self.testsuite.ip,\
                        self.testsuite.port)
        if res > 1:
            self.testsuite.scores['test_GET'] -= 0.5
            print "Received %d responses, should get only 1" % res
            if err:
                print "And, some of them reported error" % res

    def test_POST(self):
        print '----- Testing POST -----'
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        time.sleep(1)
        for test in self.testsuite.tests:
            # for checkpoint 2, this should time out; we told them to 
            # swallow the data and ignore
            try:
                response = requests.post(test % (self.testsuite.ip, self.testsuite.port),\
                    data='dummy data', timeout=3.0)
            #except requests.exceptions.Timeout:
            except requests.exceptions.RequestException:
                #print 'timeout'
                continue
            except socket.timeout:
                #print 'socket.timeout'
                continue
            # if they do return something, make sure it's OK
            self.pAssertEqual(200, response.status_code)
        self.testsuite.scores['test_POST'] = 1

    def test_bad(self):
        print '----- Testing Bad Requests-----'
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        time.sleep(1)
        for test in self.testsuite.bad_tests:
            response = requests.head(test % (self.testsuite.ip, self.testsuite.port), timeout=3.0)
            self.pAssertEqual(404, response.status_code)
        self.testsuite.scores['test_bad'] = 1

class project1cp2grader(grader):

    def __init__(self, checkpoint):
        super(project1cp2grader, self).__init__()
        self.process = 'nasty-hack'
        self.checkpoint = checkpoint
        self.tests = {
            'http://%s:%d/index.html' : 
            ('f5cacdcb48b7d85ff48da4653f8bf8a7c94fb8fb43407a8e82322302ab13becd', 802),
            'http://%s:%d/images/liso_header.png' :
            ('abf1a740b8951ae46212eb0b61a20c403c92b45ed447fe1143264c637c2e0786', 17431),
            'http://%s:%d/style.css' :
            ('575150c0258a3016223dd99bd46e203a820eef4f6f5486f7789eb7076e46736a', 301),
        }

        self.bad_tests = [
            'http://%s:%d/bad.html'
        ]

    def prepareTestSuite(self):
        super(project1cp2grader, self)
        self.suite.addTest(project1cp2tester('test_HEAD_headers', self))
        self.suite.addTest(project1cp2tester('test_HEAD', self))
        self.suite.addTest(project1cp2tester('test_GET', self))
        self.suite.addTest(project1cp2tester('test_POST', self))
        self.suite.addTest(project1cp2tester('test_bad', self))
        self.scores['test_HEAD_headers'] = 0
        self.scores['test_HEAD'] = 0
        self.scores['test_GET'] = 0
        self.scores['test_POST'] = 0
        self.scores['test_bad'] = 0

    def addTestSuite(self, testName):
        super(project1cp2grader, self)
        self.suite.addTest(project1cp2tester(testName, self))
        self.scores['test_GET'] = 0

    def setUp(self, argv):
        self.ip = argv[1]
        self.port = int(argv[2])
        #self.port = 9999
        self.tmp_dir = "../"
        self.priv_key = os.path.join(self.tmp_dir, 'grader.key')
        self.cert = os.path.join(self.tmp_dir, 'grader.crt')
        self.www = os.path.join(self.tmp_dir, 'www/')
        self.cgi = os.path.join(self.tmp_dir, 'cgi/cgi_script.py')
        print '\nUsing ports: %d' % (self.port)


if __name__ == '__main__':
    if len(sys.argv) < 3:
        sys.stderr.write('Usage: ./grader1cp2.py <ip> <port>')
        sys.exit(1)
    p1cp2grader = project1cp2grader("checkpoint-2")
    p1cp2grader.prepareTestSuite()
    p1cp2grader.setUp(sys.argv)
    results = p1cp2grader.runTests()
    p1cp2grader.reportScores()
