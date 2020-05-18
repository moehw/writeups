#!/usr/bin/env python3

import gdb

bkps = []
bkps.append("*0x07fbd8a4") # start of shellcode

bkps = "".join(["\nbreak %s" % (x,) for x in bkps])

arg = '''
define hook-stop
info register eax
info register ecx
info register edx
info register ebx

info register esp
info register ebp
info register esi
info register edi

info register eip
info register eflags

info register cs
info register ss
info register ds
info register fs
info register gs

info register cr0

printf "----------------------------------------------------\\n"

x/10i $eip
end

target remote :1234

{}
'''.format(bkps)

r = gdb.execute(arg)
