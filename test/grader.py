#!/usr/bin/env python
import unittest2
import traceback
import json
from plcommon import check_output, check_both
GIT_TAG = "git tag"
GIT_SWITCH_TAG = "git checkout %s"
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


    def test_git(self):
        check_both(GIT_TAG)
        check_both(GIT_SWITCH_TAG % self.testsuite.checkpoint)
        self.testsuite.scores['test_git'] = 1
        return

    def test_make(self):
        if self.testsuite.scores['test_git'] <= 0:
            self.skipTest("Not a git repo, skip rest of the test.")
        check_both('make clean', False, False)
        check_output('make')
        self.testsuite.scores['test_make'] = 1

class grader(object):

    def __init__(self):
        super(grader, self).__init__()
        self.suite = unittest2.TestSuite()
        self.scores = {}

    def prepareTestSuite(self):
        self.suite.addTest(tester('test_git', self))
        self.suite.addTest(tester('test_make', self))
        self.scores['test_git'] = 0
        self.scores['test_make'] = 0

    def runTests(self):
        print '\n\n----- Testing: -----'
        return unittest2.TextTestRunner(verbosity=2).run(self.suite) 

    def reportScores(self):
        print json.dumps({'scores':self.scores})





