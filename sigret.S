#include "syscall.h"
#include "traps.h"


.globl begin_sigret
.globl end_sigret

begin_sigret:
movl $SYS_sigret,%eax
int $T_SYSCALL

end_sigret:
