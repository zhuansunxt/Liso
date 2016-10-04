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

    def test_kill(self):
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        print "kill it"
        self.testsuite.process.terminate()
        return

    def test_using_select(self):
        print "Simple checker to tell if you are using select(). \
        We will check it manually later."
        p = Popen("grep -rq 'select' ./ ", \
            shell=True, stdout=PIPE, stderr=STDOUT)
        rc = p.wait()
        if rc == 0:
            self.testsuite.scores['use_select'] = 1
            return
        else:
            raise Exception("You probably did not use select.")

    def start_server(self):
        if self.testsuite.scores['test_make'] <= 0:
            self.skipTest("Failed to make. Skip this test")
        if self.testsuite.scores['use_select'] <= 0:
            self.skipTest("Select() is not used. Skip this test")
        if not os.path.isfile("lisod"):
            if os.path.isfile("echo_server"):
                print "Your makefile should make a binary called lisod, \
                        not echo_server!"
            self.skipTest("lisod not found. Skip this test")

        print "Try to start server!"
        cmd = '%s %d %d ./lisod.log %slisod.lock %s %s %s %s' % \
                (BIN, self.testsuite.port, self.testsuite.tls_port, \
                self.testsuite.tmp_dir, \
                self.testsuite.www[:-1], self.testsuite.cgi, \
                self.testsuite.priv_key, self.testsuite.cert)
        print cmd
        fp = open(os.devnull, 'w')
        p = Popen(cmd.split(' '), stdout=fp, stderr=fp)
        print "Wait 2 seconds."
        time.sleep(2)
        if p.poll() is None:
            print "Server is running"
            self.testsuite.process = p
            self.testsuite.scores['server_start'] = 1
        else:
            raise Exception("server dies within 2 seconds!")

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
            response = requests.head(test % self.testsuite.port, timeout=10.0)
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
            response = requests.head(test % self.testsuite.port, timeout=10.0)
            self.pAssertEqual(200, response.status_code)
        self.testsuite.scores['test_HEAD'] = 1
        if not lowlevelhttptests.check_correct_HEAD('127.0.0.1',\
                                                    self.testsuite.port):
            self.testsuite.scores['test_HEAD'] -= 0.5
            print "HEAD must not return Content!"

    def test_GET(self):
        print '----- Testing GET -----'
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        time.sleep(1)
        for test in self.testsuite.tests:
            response = requests.get(test % self.testsuite.port, timeout=10.0)
            contenthash = hashlib.sha256(response.content).hexdigest()
            self.pAssertEqual(200, response.status_code)
            self.pAssertEqual(contenthash, self.testsuite.tests[test][0])
        self.testsuite.scores['test_GET'] = 1
        err, res = lowlevelhttptests.check_correct_GET('127.0.0.1',\
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
                response = requests.post(test % self.testsuite.port,\
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
            response = requests.head(test % self.testsuite.port, timeout=3.0)
            self.pAssertEqual(404, response.status_code)
        self.testsuite.scores['test_bad'] = 1

    def test_big(self):
        print '----- Testing Big file -----'
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        time.sleep(1)
        for test in self.testsuite.big_tests:
            response = requests.get(test % self.testsuite.port, timeout=5.0)
            contenthash = hashlib.sha256(response.content).hexdigest()
            self.pAssertEqual(200, response.status_code)
            self.pAssertEqual(contenthash, self.testsuite.big_tests[test])
        self.testsuite.scores['test_big'] = 1

    def test_apache_bench(self):
        print '----- Testing Apache Bench with pipelining-----'
        #print 'Make sure your lisod accepts http/1.0 requests'
        if self.testsuite.process is None:
            self.skipTest("server failed to start. skip this test")
        time.sleep(3)
        cmd = '../apachebench/ab -kc 100 -n 40000\
        http://127.0.0.1:%d/index.html > ../tmp/ab.log'% self.testsuite.port
        self.pAssertEqual(0, os.system(cmd))
        cmd_error_rate = "cat ../tmp/ab.log | grep Failed | awk '{print $3}' "
        p = Popen(cmd_error_rate, shell=True, stdout=PIPE, stderr=STDOUT)
        out, dummy_err = p.communicate()
        try:
            errors = int(out)
            if errors > 40000:
                errors = 40000
        except ValueError:
            errors = 0
        print "ab test errors:%d" % errors
        self.testsuite.scores['test_apache_bench'] =\
            round((40000-errors)/40000.0, 2)

class project1cp2grader(grader):

    def __init__(self, checkpoint):
        super(project1cp2grader, self).__init__()
        self.process = None
        self.checkpoint = checkpoint
        self.tests = {
            'http://127.0.0.1:%d/index.html' : 
            ('f5cacdcb48b7d85ff48da4653f8bf8a7c94fb8fb43407a8e82322302ab13becd', 802),
            'http://127.0.0.1:%d/images/liso_header.png' :
            ('abf1a740b8951ae46212eb0b61a20c403c92b45ed447fe1143264c637c2e0786', 17431),
            'http://127.0.0.1:%d/style.css' :
            ('575150c0258a3016223dd99bd46e203a820eef4f6f5486f7789eb7076e46736a', 301)
        }

        self.bad_tests = [
            'http://127.0.0.1:%d/bad.html'
        ]

        self.big_tests = {
            'http://127.0.0.1:%d/big.html':
            'fa066c7d40f0f201ac4144e652aa62430e58a6b3805ec70650f678da5804e87b'
        }

    def prepareTestSuite(self):
        super(project1cp2grader, self).prepareTestSuite()
        self.suite.addTest(project1cp2tester('test_using_select', self))
        self.suite.addTest(project1cp2tester('start_server', self))
        self.suite.addTest(project1cp2tester('test_HEAD_headers', self))
        self.suite.addTest(project1cp2tester('test_HEAD', self))
        self.suite.addTest(project1cp2tester('test_GET', self))
        self.suite.addTest(project1cp2tester('test_POST', self))
        self.suite.addTest(project1cp2tester('test_bad', self))
        self.suite.addTest(project1cp2tester('test_big', self))
        #self.suite.addTest(project1cp2tester('test_apache_bench', self))
        self.suite.addTest(project1cp2tester('test_kill', self))
        self.scores['use_select'] = 0
        self.scores['server_start'] = 0
        self.scores['test_HEAD_headers'] = 0
        self.scores['test_HEAD'] = 0
        self.scores['test_GET'] = 0
        self.scores['test_POST'] = 0
        self.scores['test_bad'] = 0
        self.scores['test_big'] = 0
        #self.scores['test_apache_bench'] = 0

    def setUp(self):
        self.port = random.randint(1025, 9999)
        #self.port = 9999
        self.tls_port = random.randint(1025, 9999)
        self.tmp_dir = "../tmp/"
        self.priv_key = os.path.join(self.tmp_dir, 'grader.key')
        self.cert = os.path.join(self.tmp_dir, 'grader.crt')
        self.www = os.path.join(self.tmp_dir, 'www/')
        self.cgi = os.path.join(self.tmp_dir, 'cgi/cgi_script.py')
        print '\nUsing ports: %d,%d' % (self.port, self.tls_port)


if __name__ == '__main__':
    p1cp2grader = project1cp2grader("checkpoint-2")
    p1cp2grader.prepareTestSuite()
    p1cp2grader.setUp()
    results = p1cp2grader.runTests()
    p1cp2grader.reportScores()
