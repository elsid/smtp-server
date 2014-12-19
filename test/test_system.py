#!/usr/bin/env python3
#conding: utf-8

from time import sleep
from smtplib import SMTP, SMTPResponseException, SMTPServerDisconnected
from hamcrest import assert_that, raises, calling, equal_to
from unittest import TestCase, main

HOST = 'localhost'
PORT = 25253
TIMEOUT = 0.2
COUNT = 3

class HeloTest(TestCase):
    def test_one_should_succeed(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            assert_that(smtp.helo(), equal_to((250, b'Ok')))
            assert_that(smtp.quit(), equal_to((221, b'Service closing transmission channel')))

class EhloTest(TestCase):
    def test_one_should_succeed(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            assert_that(smtp.ehlo(), equal_to((250, b'Ok')))
            assert_that(smtp.quit(), equal_to((221, b'Service closing transmission channel')))

    def test_one_with_domain_should_succeed(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            assert_that(smtp.ehlo('domain'), equal_to((250, b'Ok')))
            assert_that(smtp.quit(), equal_to((221, b'Service closing transmission channel')))

    def test_many_should_succeed(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            for _ in range(COUNT):
                assert_that(smtp.ehlo(), equal_to((250, b'Ok')))
            assert_that(smtp.quit(), equal_to((221, b'Service closing transmission channel')))

class NoopTest(TestCase):
    def test_after_ehlo_should_succeed(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            assert_that(smtp.ehlo(), equal_to((250, b'Ok')))
            assert_that(smtp.noop(), equal_to((250, b'Ok')))
            assert_that(smtp.quit(), equal_to((221, b'Service closing transmission channel')))

    def test_after_mail_should_succeed(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            assert_that(smtp.ehlo(), equal_to((250, b'Ok')))
            assert_that(smtp.mail('from@domain'), equal_to((250, b'Ok')))
            assert_that(smtp.noop(), equal_to((250, b'Ok')))
            assert_that(smtp.quit(), equal_to((221, b'Service closing transmission channel')))

    def test_after_rcpt_should_succeed(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            assert_that(smtp.ehlo(), equal_to((250, b'Ok')))
            assert_that(smtp.mail('from@domain'), equal_to((250, b'Ok')))
            assert_that(smtp.rcpt(['to@domain']), equal_to((250, b'Ok')))
            assert_that(smtp.noop(), equal_to((250, b'Ok')))
            assert_that(smtp.quit(), equal_to((221, b'Service closing transmission channel')))

    def test_after_data_should_succeed(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            assert_that(smtp.ehlo(), equal_to((250, b'Ok')))
            assert_that(smtp.mail('from@domain'), equal_to((250, b'Ok')))
            assert_that(smtp.rcpt(['to@domain']), equal_to((250, b'Ok')))
            assert_that(smtp.data('message'), equal_to((250, b'Ok')))
            assert_that(smtp.noop(), equal_to((250, b'Ok')))
            assert_that(smtp.quit(), equal_to((221, b'Service closing transmission channel')))

class MailTest(TestCase):
    def test_one_should_succeed(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            assert_that(smtp.ehlo(), equal_to((250, b'Ok')))
            assert_that(smtp.mail('from@domain'), equal_to((250, b'Ok')))
            assert_that(smtp.rcpt(['to@domain']), equal_to((250, b'Ok')))
            assert_that(smtp.data('message'), equal_to((250, b'Ok')))
            assert_that(smtp.quit(), equal_to((221, b'Service closing transmission channel')))

    def test_many_should_succeed(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            assert_that(smtp.ehlo(), equal_to((250, b'Ok')))
            for _ in range(COUNT):
                assert_that(smtp.mail('from@domain'), equal_to((250, b'Ok')))
                assert_that(smtp.rcpt(['to@domain']), equal_to((250, b'Ok')))
                assert_that(smtp.data('message'), equal_to((250, b'Ok')))
            assert_that(smtp.quit(), equal_to((221, b'Service closing transmission channel')))

    def test_no_reverse_path_should_return_error(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            assert_that(smtp.ehlo(), equal_to((250, b'Ok')))
            smtp.putcmd('mail to:')
            assert_that(smtp.getreply(), equal_to((555, b'Syntax error in reverse-path or not present')))

    def test_after_connect_should_return_error(self):
        def run():
            with SMTP() as smtp:
                assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
                assert_that(smtp.mail('from@domain'), equal_to((503, b'Bad sequence of commands')))

        assert_that(calling(run), raises(SMTPResponseException))

    def test_after_mail_should_return_error(self):
        def run():
            with SMTP() as smtp:
                assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
                assert_that(smtp.mail('from@domain'), equal_to((250, b'Ok')))
                assert_that(smtp.mail('from@domain'), equal_to((503, b'Bad sequence of commands')))
                assert_that(smtp.quit(), equal_to((221, b'Service closing transmission channel')))

        assert_that(calling(run), raises(SMTPResponseException))

class RcptTest(TestCase):
    def test_many_should_succeed(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            assert_that(smtp.ehlo(), equal_to((250, b'Ok')))
            assert_that(smtp.mail('from@domain'), equal_to((250, b'Ok')))
            assert_that(smtp.rcpt(['to%d@domain' % n for n in range(COUNT)]),
                equal_to((250, b'Ok')))
            assert_that(smtp.data('message'), equal_to((250, b'Ok')))
            assert_that(smtp.quit(), equal_to((221, b'Service closing transmission channel')))

    def test_no_forward_path_should_return_error(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            assert_that(smtp.ehlo(), equal_to((250, b'Ok')))
            assert_that(smtp.mail('from@domain'), equal_to((250, b'Ok')))
            smtp.putcmd('rcpt to:')
            assert_that(smtp.getreply(), equal_to((555, b'Syntax error in forward-path or not present')))
            assert_that(smtp.quit(), equal_to((221, b'Service closing transmission channel')))

class RsetTest(TestCase):
    def test_after_connect_should_succeed(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            assert_that(smtp.rset(), equal_to((250, b'Ok')))
            assert_that(smtp.quit(), equal_to((221, b'Service closing transmission channel')))

    def test_after_ehlo_should_succeed(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            assert_that(smtp.ehlo(), equal_to((250, b'Ok')))
            assert_that(smtp.rset(), equal_to((250, b'Ok')))
            assert_that(smtp.quit(), equal_to((221, b'Service closing transmission channel')))

    def test_after_mail_should_succeed(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            assert_that(smtp.ehlo(), equal_to((250, b'Ok')))
            assert_that(smtp.mail('from@domain'), equal_to((250, b'Ok')))
            assert_that(smtp.rset(), equal_to((250, b'Ok')))
            assert_that(smtp.quit(), equal_to((221, b'Service closing transmission channel')))

    def test_after_rcpt_should_succeed(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            assert_that(smtp.ehlo(), equal_to((250, b'Ok')))
            assert_that(smtp.mail('from@domain'), equal_to((250, b'Ok')))
            assert_that(smtp.rcpt(['to@domain']), equal_to((250, b'Ok')))
            assert_that(smtp.rset(), equal_to((250, b'Ok')))
            assert_that(smtp.quit(), equal_to((221, b'Service closing transmission channel')))

class VrfyTest(TestCase):
    def test_should_return_error(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            assert_that(smtp.ehlo(), equal_to((250, b'Ok')))
            assert_that(smtp.vrfy('some@domain'), equal_to((502, b'Command not implemented')))
            assert_that(smtp.quit(), equal_to((221, b'Service closing transmission channel')))

class IncorrectCommandTest(TestCase):
    def test_empty_should_return_error(self):
        def run():
            with SMTP() as smtp:
                assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
                smtp.putcmd('')
                assert_that(smtp.getreply(), equal_to((500, b'Syntax error, command unrecognized')))

        assert_that(calling(run), raises(SMTPResponseException))

    def test_not_empty_should_return_error(self):
        def run():
            with SMTP() as smtp:
                assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
                smtp.putcmd('command')
                assert_that(smtp.getreply(), equal_to((500, b'Syntax error, command unrecognized')))

        assert_that(calling(run), raises(SMTPResponseException))

class TimeoutTest(TestCase):
    def test_connect_than_sleep_should_return_error(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            sleep(TIMEOUT)
            assert_that(calling(smtp.ehlo), raises(SMTPServerDisconnected))

class SpecificCommandTest(TestCase):
    def test_random_case(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            smtp.putcmd('EhLo')
            assert_that(smtp.getreply(), equal_to((250, b'Ok')))
            assert_that(smtp.quit(), equal_to((221, b'Service closing transmission channel')))

    def test_leading_spaces(self):
        with SMTP() as smtp:
            assert_that(smtp.connect(HOST, PORT), equal_to((220, b'Service ready')))
            smtp.putcmd(' \t\n \rehlo')
            assert_that(smtp.getreply(), equal_to((250, b'Ok')))
            assert_that(smtp.quit(), equal_to((221, b'Service closing transmission channel')))

if __name__ == '__main__':
    main()
