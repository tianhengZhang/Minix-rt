#include <base/linkage.h>
#include <base/compiler.h>
#include <base/common.h>

#include <libminix_rt/syscalls.h>
#include <libminix_rt/object/ipc.h>
#include <libminix_rt/object/notifier.h>

asmlinkage __visible __weak int printf(const char *fmt, ...)
{
	va_list args;
	char *text = ipc_get_debug_buffer();
	size_t text_len;
	int r = 0;

	va_start(args, fmt);
   	text_len = vscnprintf(text, IPC_DEBUG_PRINTF_BUFFER_MAX, fmt, args);
   	if (text_len) {
    	r = __syscall(__NR_debug_printf, text, text_len);
	}
	va_end(args);

    return r;
}

__weak void hang(const char *fmt, ...)
{
	static char buf[IPC_DEBUG_PRINTF_BUFFER_MAX];
	s64 len;
	va_list args;

	va_start(args, fmt);
	len = vscnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (len && buf[len - 1] == '\n')
		buf[len - 1] = '\0';

   printf("task hang %s\n", buf);
   notifier_send_child_exit(-1);
   for (;;);
}