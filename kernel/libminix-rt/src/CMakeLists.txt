
target_sources(
	${libminix_rt_VAR}libminix_rt
	PRIVATE
	printf.c
	notifier.c ipc.c brk.c malloc.c string.c mmap.c
	exec.c random.c pid.c
)
