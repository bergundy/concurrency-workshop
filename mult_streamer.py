#!/usr/bin/python
import sys
import os
sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)

for line in iter(sys.stdin.readline, ''):
    num = int(line)
    print(num * 7)
