# SPDX-License-Identifier: Apache-2.0

kernel_library()

kernel_library_sources(
	head.S
	entry.S smccc-call.S
	setup.c smp.c process.c cpu_ops.c psci.c cpuinfo.c
	irq.c ioremap.c time.c cpufeature.c
	traps.c fpsimd.c signal.c syscall.c ptrace.c insn.c
)

set_property(GLOBAL PROPERTY LINKER_SCRIPT_S "${CMAKE_CURRENT_LIST_DIR}/linker.lds.S")
