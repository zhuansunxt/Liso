#!/usr/bin/env python
import unittest2
import traceback
import json
from plcommon import check_output, check_both

class tester(unittest2.TestCase):

    def __init__(self, test_name, testsuite):
        super(tester, self).__init__(test_name)
        self.testsuite = testsuite

    def pAssertEqual(self, arg1, arg2):
        try:
            self.assertEqual(arg1, arg2)
        except Exception as e:
            print traceback.format_stack()[-2]
            raise e

    def pAssertTrue(self, test):
        try:
            self.assertTrue(test)
        except Exception as e:
            print traceback.format_stack()[-2]
            raise e


class grader(object):

    def __init__(self):
        super(grader, self).__init__()
        self.suite = unittest2.TestSuite()
        self.scores = {}

    def runTests(self):
        print '\n\n----- Testing: -----'
        return unittest2.TextTestRunner(verbosity=2).run(self.suite) 

    def reportScores(self):
        print json.dumps({'scores':self.scores})





