# coding=utf-8
from time import sleep
import sys
import subprocess

def spamer(num):
    sleep(1)
    return subprocess.Popen(["./build/client", host, port, "spamer" + str(num)], stdin=subprocess.PIPE)

def finisher(spam):
    spam.communicate("exit")
    sleep(1)

def createSpamers(num):
    sleep(10)
    return map(spamer, xrange(num, num + 10))


if __name__ == "__main__":
    host = sys.argv[1]
    port = sys.argv[2]
    num = 10 * (int(sys.argv[3]) / 10) + 10
    spamers = sum(map(createSpamers, xrange(0, num, 10)),[])
    sleep(20)
    map(finisher, spamers)
