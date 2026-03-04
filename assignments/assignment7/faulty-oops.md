# lsmod
Module                  Size  Used by    Tainted: G  
hello                  16384  0 
faulty                 16384  0 
scull                  24576  0 
# ls -l /dev/scull*
crw-r--r--    1 root     root      248,   0 Mar  2 03:39 /dev/scull0
crw-r--r--    1 root     root      248,   1 Mar  2 03:39 /dev/scull1
crw-r--r--    1 root     root      248,   2 Mar  2 03:39 /dev/scull2
crw-r--r--    1 root     root      248,   3 Mar  2 03:39 /dev/scull3
# echo hello_world > /dev/faulty
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x0000000096000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=0000000041bcf000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 0000000096000045 [#1] SMP
Modules linked in: hello(O) faulty(O) scull(O)
CPU: 0 PID: 119 Comm: sh Tainted: G           O       6.1.44 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x10/0x20 [faulty]
lr : vfs_write+0xc8/0x390
sp : ffffffc008dbbd20
x29: ffffffc008dbbd80 x28: ffffff8001b9ea00 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 000000000000000c x22: 000000000000000c x21: ffffffc008dbbdc0
x20: 000000558348fa30 x19: ffffff8001bcaa00 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc000787000 x3 : ffffffc008dbbdc0
x2 : 000000000000000c x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
 faulty_write+0x10/0x20 [faulty]
 ksys_write+0x74/0x110
 __arm64_sys_write+0x1c/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x2c/0xc0
 el0_svc+0x2c/0x90
 el0t_64_sync_handler+0xf4/0x120
 el0t_64_sync+0x18c/0x190
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace 0000000000000000 ]---

Analysis:

The call trace shows the sequence of function calls leading to the crash:

faulty_write — Driver write function where crash occurred

ksys_write — Kernel write system call handler

System call entry path

A NULL pointer dereference occurred because the faulty_write function attempted to write data to memory address 0x0.

When you see faulty_write+0x10/0x20, it tells you exactly the function the CPU was at.

0x20: The total size of the function in bytes (32 bytes)

0x10: The crash happened at the 16th byte. 
