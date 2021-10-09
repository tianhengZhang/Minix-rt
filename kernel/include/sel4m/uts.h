/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __MINIX_RT_UTS_H_
#define __MINIX_RT_UTS_H_

/*
 * Defines for what uname() should return 
 */
#ifndef UTS_SYSNAME
#define UTS_SYSNAME "Minix-rt"
#endif

#ifndef UTS_NODENAME
#define UTS_NODENAME CONFIG_DEFAULT_HOSTNAME /* set by sethostname() */
#endif

#ifndef UTS_DOMAINNAME
#define UTS_DOMAINNAME "(none)"	/* set by setdomainname() */
#endif

#endif /* !__MINIX_RT_UTS_H_ */
