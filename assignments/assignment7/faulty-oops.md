# Analysis of kernel oops

## faulty.ko
A kernel module which generates an oops on executing the command `echo “hello_world” > /dev/faulty`.

### file command output of faulty.ko
```
faulty.ko: ELF 64-bit LSB relocatable, ARM aarch64, version 1 (SYSV), BuildID[sha1]=ecab6d86aea436b2c85b9040bf77e67f513850bf, not stripped
```

### Stack dump for faulty kernel module

```
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x96000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=0000000042050000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 96000045 [#1] SMP
Modules linked in: faulty(O) scull(O) hello(O)
CPU: 0 PID: 161 Comm: sh Tainted: G           O      5.15.18 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x14/0x20 [faulty]
lr : vfs_write+0xa8/0x2b0
sp : ffffffc008d0bd80
x29: ffffffc008d0bd80 x28: ffffff80020d8000 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 0000000040001000 x22: 0000000000000012 x21: 00000055828b2670
x20: 00000055828b2670 x19: ffffff80020a5500 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc0006fc000 x3 : ffffffc008d0bdf0
x2 : 0000000000000012 x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
 faulty_write+0x14/0x20 [faulty]
 ksys_write+0x68/0x100
 __arm64_sys_write+0x20/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x40/0xa0
 el0_svc+0x20/0x60
 el0t_64_sync_handler+0xe8/0xf0
 el0t_64_sync+0x1a0/0x1a4
Code: d2800001 d2800000 d503233f d50323bf (b900003f)
---[ end trace 74d48d1d94f94927 ]---
```
- `Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000`: The reason for the kernel failure is due to a NULL pointer dereference.
- `ESR = 0x96000045`: WnR, bit [6] of Exception Syndrome Register set to 1 indicates an abort caused by an instruction writing to a memory location.
- `Internal error: Oops: 96000045 [#1] SMP`: Oops error occured 1 time.
- `Modules linked in: faulty(O) scull(O) hello(O)`: externally-built (“out-of-tree”) module(s) loaded.
- `CPU: 0 PID: 161 Comm: sh Tainted: G O 5.15.18 #1`
 - `CPU` denotes on which the CPU the error occurred
 - `Tainted: G O 5.15.18 #1` - Tainted kernel tells that module have a GPL or compatible license and externally built (“out-of-tree”) was loaded.
- `pc : faulty_write+0x14/0x20 [faulty]` - The program counter at the time of failure is on function faulty_write executing the instruction at address 0x14 (offset from the function address) which is 0x20 bytes long.

From the call trace, we know that kernel module `faulty` has caused the failure for null pointer dereference while executing an instruction at address `0x14` into a function called `faulty_write`. For further analysis, we can look at the objdump of fauly kernel module and discern what instruction causing the kernel failure.

## objdump of faulty.ko

From the objdump (shown below), we see that the instruction at address 0x14 is str wzr, [x1], which stores contents of wzr (zero register) to the address pointed by x1, which is a NULL from instruction at address 0x04 (mov x1, #0x0).

```
0000000000000000 <faulty_write>:
   0:   d503245f        bti     c
   4:   d2800001        mov     x1, #0x0                        // #0
   8:   d2800000        mov     x0, #0x0                        // #0
   c:   d503233f        paciasp
  10:   d50323bf        autiasp
  14:   b900003f        str     wzr, [x1]
  18:   d65f03c0        ret
  1c:   d503201f        nop
```

The function defintion of faulty_write is shown below where the code is dereferencing an address pointed to 0 (NULL) and write a 0.
```
ssize_t faulty_write (struct file *filp, const char __user *buf, size_t count,
		loff_t *pos)
{
	/* make a simple fault by dereferencing a NULL pointer */
	*(int *)0 = 0;
	return 0;
}

```

## References
- https://www.kernel.org/doc/html/latest/admin-guide/bug-hunting.html