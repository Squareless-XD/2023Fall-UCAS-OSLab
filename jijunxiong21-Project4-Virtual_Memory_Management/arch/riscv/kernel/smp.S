#include <asm.h>
#include <csr.h>

# ENTRY(get_current_cpu_id)
#   csrr a0, mhartid
#   jr ra
# ENDPROC(get_current_cpu_id)
