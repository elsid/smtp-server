#!/usr/bin/env python3
#conding: utf-8

import sys

from time import sleep
from smtplib import SMTP

HOST = 'localhost'
PORT = 25252

if __name__ == '__main__':
    message = sys.stdin.read()
    with SMTP() as smtp:
        smtp.connect(HOST, PORT)
        smtp.ehlo()
        for n in range(int(sys.argv[1])):
            smtp.sendmail('from@domain', ['to@domain'], message)
        smtp.quit()
