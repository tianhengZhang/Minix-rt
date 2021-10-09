#ifndef __MINIX_RT_OBJECT_TCB_H_
#define __MINIX_RT_OBJECT_TCB_H_

#include <minix_rt/object/pid.h>

#include <uapi/minix_rt/magic.h>

struct task_struct;

extern struct task_struct *tcb_create_task(unsigned int flags);
extern void tcb_destroy_task(struct task_struct *tsk);

extern void tcb_set_task_stack_end_magic(struct task_struct *tsk);

#define tcb_stack_end_corrupted(tsk)	\
		(*(end_of_stack(tsk)) != STACK_END_MAGIC)

extern void tcb_do_exit(struct task_struct *tsk, int flags);

#endif /* !__MINIX_RT_OBJECT_TCB_H_ */