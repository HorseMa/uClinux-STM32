/* include/net-snmp/net-snmp-config.h.in.  Generated from configure.in by autoheader.  */
/* modified by hand with care. */

#ifndef NET_SNMP_CONFIG_H
#define NET_SNMP_CONFIG_H

/* Define HAVE_WIN32_PLATFORM_SDK if you have:
 * Microsoft Visual Studio MSVC 6.0 and the Platform SDK (PSDK)
 * Microsoft Visual Studio.New 2002
 * Microsoft Visual Studio.New 2003
 * Cygwin
 * MinGW 
 */
/* #undef HAVE_WIN32_PLATFORM_SDK */

#define INSTALL_BASE "c:/usr"

/* config.h:  a general config file */

/* Default (SNMP) version number for the tools to use */
#define DEFAULT_SNMP_VERSION 3

/* don't change these values! */
#define SNMPV1      0xAAAA       /* readable by anyone */
#define SNMPV2ANY   0xA000       /* V2 Any type (includes NoAuth) */
#define SNMPV2AUTH  0x8000       /* V2 Authenticated requests only */

/* default list of mibs to load */

#define DEFAULT_MIBS "IP-MIB;IF-MIB;TCP-MIB;UDP-MIB;HOST-RESOURCES-MIB;SNMPv2-MIB;RFC1213-MIB;NOTIFICATION-LOG-MIB;UCD-SNMP-MIB;UCD-DEMO-MIB;SNMP-TARGET-MIB;NET-SNMP-AGENT-MIB;DISMAN-EVENT-MIB;SNMP-VIEW-BASED-ACM-MIB;SNMP-COMMUNITY-MIB;UCD-DLMOD-MIB;SNMP-FRAMEWORK-MIB;SNMP-MPD-MIB;SNMP-USER-BASED-SM-MIB;SNMP-NOTIFICATION-MIB;SNMPv2-TM"

/* default location to look for mibs to load using the above tokens
   and/or those in the MIBS envrionment variable*/
#define DEFAULT_MIBDIRS INSTALL_BASE ## "/share/snmp/mibs"

/* default mib files to load, specified by path. */
/* #undef DEFAULT_MIBFILES */

/* should we compile to use special opaque types: float, double,
   counter64, i64, ui64, union? */
#define OPAQUE_SPECIAL_TYPES 1

/* comment the next line if you are compiling with libsnmp.h
   and are not using the UC-Davis SNMP library. */
#define UCD_SNMP_LIBRARY 1

/* define if you want to compile support for both authentication and
   privacy support. */
#define SCAPI_AUTHPRIV 1

/* define if you are using the MD5 code ...*/
/* #undef USE_INTERNAL_MD5 */

/* define if you are using the codeS11 library ...*/
/* #undef USE_PKCS */

/* add in recent CMU library extensions (not complete) */
/* #undef CMU_COMPATIBLE */

/* add in recent resource lock functions (not complete) */
/* #undef _REENTRANT */

/* debugging stuff */
/* if defined, we optimize the code to exclude all debugging calls. */
/* #undef SNMP_NO_DEBUGGING */
/* ignore the -D flag and always print debugging information */
#define SNMP_ALWAYS_DEBUG 0

/* reverse encoding BER packets is both faster and more efficient in space. */
#define USE_REVERSE_ASNENCODING       1
#define DEFAULT_ASNENCODING_DIRECTION 1 /* 1 = reverse, 0 = forwards */

/* PERSISTENT_DIRECTORY: If defined, the library is capabile of saving
   persisant information to this directory in the form of configuration
   lines: PERSISTENT_DIRECTORY/NAME.persistent.conf */
#define PERSISTENT_DIRECTORY INSTALL_BASE ## "/snmp/persist"

/* PERSISTENT_MASK: the umask permissions to set up persistent files with */
/* #undef PERSISTENT_MASK -- no win32 umask */

/* AGENT_DIRECTORY_MODE: the mode the agents should use to create
   directories with. Since the data stored here is probably sensitive, it
   probably should be read-only by root/administrator. */
#define AGENT_DIRECTORY_MODE 0700

/* MAX_PERSISTENT_BACKUPS:
 *   The maximum number of persistent backups the library will try to
 *   read from the persistent cache directory.  If an application fails to
 *   close down successfully more than this number of times, data will be lost.
 */
#define MAX_PERSISTENT_BACKUPS 10


/* define if you are embedding perl in the main agent */
/* #undef NETSNMP_EMBEDDED_PERL */

#if notused
/* define the system type include file here */
#define SYSTEM_INCLUDE_FILE <net-snmp/system/generic.h>

/* define the machine (cpu) type include file here */
#define MACHINE_INCLUDE_FILE <net-snmp/machine/generic.h>
#endif

/* SNMPLIBDIR contains important files */

#define SNMPLIBPATH INSTALL_BASE ## "/lib"
#define SNMPSHAREPATH INSTALL_BASE ## "/share/snmp"
#define SNMPCONFPATH INSTALL_BASE ## "/etc/snmp"
#define SNMPDLMODPATH INSTALL_BASE ## "/lib/dlmod"

/* LOGFILE:  If defined it closes stdout/err/in and opens this in out/err's
   place.  (stdin is closed so that sh scripts won't wait for it) */
/* #undef LOGFILE */

/* default system contact */
#define SYS_CONTACT "unknown"

/* system location */
#define SYS_LOC "unknown"

/* Use libwrap to handle allow/deny hosts? */
/* #undef USE_LIBWRAP */

/* Use dmalloc to do malloc debugging? */
/* #undef HAVE_DMALLOC_H */

/* location of UNIX kernel */
#define KERNEL_LOC "unknown"

/* location of mount table list */
#define ETC_MNTTAB "unknown"

/* location of swap device (ok if not found) */
/* #undef DMEM_LOC */

/* Command to generate ps output, the final column must be the process
   name withOUT arguments */
#define PSCMD "/bin/ps"

/* Where is the uname command */
#define UNAMEPROG "/bin/uname"

/* pattern for temporary file names */
#define NETSNMP_TEMP_FILE_PATTERN INSTALL_BASE ## "/temp/snmpdXXXXXX"

/* testing code sections. */
/* #undef SNMP_TESTING_CODE */

/* If you don't have root access don't exit upon kmem errors */
/* #undef NO_ROOT_ACCESS */

/* If we don't want to use kmem. */
/* #undef NO_KMEM_USAGE */

/* If you don't want the agent to report on variables it doesn't have data for */
#define NO_DUMMY_VALUES 1

/* Define if statfs takes 2 args and the second argument has
   type struct fs_data. [Ultrix] */
/* #undef STAT_STATFS_FS_DATA */

/* Define if the TCP timer constants in <netinet/tcp_timer.h>
   depend on the integer variable `hz'.  [FreeBSD 4.x] */
/* #undef TCPTV_NEEDS_HZ */


/* Define to one of `_getb67', `GETB67', `getb67' for Cray-2 and Cray-YMP
   systems. This function is required for `alloca.c' support on those systems.
   */
/* #undef CRAY_STACKSEG_END */

/* Define to 1 if using `alloca.c'. */
/* #undef C_ALLOCA */

/* Define if DES encryption should not be supported */
/* #undef DISABLE_DES */

/* Define if MD5 authentication should not be supported */
/* #undef DISABLE_MD5 */

/* Define if mib loading and parsing code should not be included */
/* #undef DISABLE_MIB_LOADING */

/* Define if SNMPv1 code should not be included */
/* #undef DISABLE_SNMPV1 */

/* Define if SNMPv2c code should not be included */
/* #undef DISABLE_SNMPV2C */

/* Define to 1 if you have the `AES_cfb128_encrypt' function. */
/* #undef HAVE_AES_CFB128_ENCRYPT */

/* Define to 1 if you have `alloca', as a function or macro. */
/* #undef HAVE_ALLOCA */

/* Define to 1 if you have <alloca.h> and it should be used (not on Ultrix).
   */
/* #undef HAVE_ALLOCA_H */

/* Define to 1 if you have the <arpa/inet.h> header file. */
/* #undef HAVE_ARPA_INET_H */

/* Define to 1 if you have the <asm/page.h> header file. */
/* #undef HAVE_ASM_PAGE_H */

/* Define to 1 if you have the `bcopy' function. */
/* #undef HAVE_BCOPY */

/* Define to 1 if you have the `cgetnext' function. */
/* #undef HAVE_CGETNEXT */

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_DIRENT_H */

/* Define to 1 if you have the <dlfcn.h> header file. */
/* #undef HAVE_DLFCN_H */

/* Define to 1 if you have the `dlopen' function. */
/* #undef HAVE_DLOPEN */

/* Define to 1 if you have the <err.h> header file. */
/* #undef HAVE_ERR_H */

/* Define to 1 if you have the `eval_pv' function. */
/* #undef HAVE_EVAL_PV */

/* Define to 1 if you have the `execv' function. */
/* #undef HAVE_EXECV */

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `fork' function. */
/* #undef HAVE_FORK */

/* Define to 1 if you have the <fstab.h> header file. */
/* #undef HAVE_FSTAB_H */

/* Define to 1 if you have the `getaddrinfo' function. */
/* #undef HAVE_GETADDRINFO */

/* Define to 1 if you have the `getdtablesize' function. */
/* #undef HAVE_GETDTABLESIZE */

/* Define to 1 if you have the `getfsstat' function. */
/* #undef HAVE_GETFSSTAT */

/* Define to 1 if you have the `getgrnam' function. */
/* #undef HAVE_GETGRNAM */

/* Define to 1 if you have the `gethostname' function. */
#define HAVE_GETHOSTNAME 1

/* Define to 1 if you have the `getipnodebyname' function. */
/* #undef HAVE_GETIPNODEBYNAME */

/* Define to 1 if you have the `getloadavg' function. */
/* #undef HAVE_GETLOADAVG */

/* Define to 1 if you have the `getmntent' function. */
/* #undef HAVE_GETMNTENT */

/* Define to 1 if you have the <getopt.h> header file. */
/* #undef HAVE_GETOPT_H */

/* Define to 1 if you have the `getpagesize' function. */
/* #undef HAVE_GETPAGESIZE */

/* Define to 1 if you have the `getpid' function. */
#define HAVE_GETPID 1

/* Define to 1 if you have the `getpwnam' function. */
/* #undef HAVE_GETPWNAM */

/* Define to 1 if you have the `gettimeofday' function. */
/* #undef HAVE_GETTIMEOFDAY */

/* Define to 1 if you have the <grp.h> header file. */
/* #undef HAVE_GRP_H */

/* Define to 1 if you have the `if_freenameindex' function. */
/* #undef HAVE_IF_FREENAMEINDEX */

/* Define to 1 if you have the `if_nameindex' function. */
/* #undef HAVE_IF_NAMEINDEX */

/* Define to 1 if you have the `index' function. */
/* #undef HAVE_INDEX */

/* Define to 1 if you have the <inet/mib2.h> header file. */
/* #undef HAVE_INET_MIB2_H */

/* Define to 1 if the system has the type `int32_t'. */
#define HAVE_INT32_T 1

/* define if you have type uint32_t */
#define HAVE_UINT32_T 1

/* define if you have type u_int32_t */
#undef HAVE_U_INT32_T

/* define if you have type int64_t */
#define HAVE_INT64_T 1

/* define if you have type uint64_t */
#define HAVE_UINT64_T 1

/* define if you have type u_int64_t */
#undef HAVE_U_INT64_T

/* Define to 1 if you have the <inttypes.h> header file. */
/* #undef HAVE_INTTYPES_H */

/* Define to 1 if you have the <ioctls.h> header file. */
/* #undef HAVE_IOCTLS_H */

/* Define to 1 if you have the <io.h> header file. */
#define HAVE_IO_H 1

/* Define to 1 if you have the `knlist' function. */
/* #undef HAVE_KNLIST */

/* Define to 1 if you have the <kstat.h> header file. */
/* #undef HAVE_KSTAT_H */

/* Define to 1 if you have the `kvm_getprocs' function. */
/* #undef HAVE_KVM_GETPROCS */

/* Define to 1 if you have the <kvm.h> header file. */
/* #undef HAVE_KVM_H */

/* Define to 1 if you have the `kvm_openfiles' function. */
/* #undef HAVE_KVM_OPENFILES */

/* Define to 1 if you have the `crypto' library (-lcrypto). */
/* #undef HAVE_LIBCRYPTO */

/* Define to 1 if you have the `dl' library (-ldl). */
/* #undef HAVE_LIBDL */

/* Define to 1 if you have the `efence' library (-lefence). */
/* #undef HAVE_LIBEFENCE */

/* Define to 1 if you have the `elf' library (-lelf). */
/* #undef HAVE_LIBELF */

/* Define to 1 if you have the `kstat' library (-lkstat). */
/* #undef HAVE_LIBKSTAT */

/* Define to 1 if you have the `kvm' library (-lkvm). */
/* #undef HAVE_LIBKVM */

/* Define to 1 if you have the `m' library (-lm). */
/* #undef HAVE_LIBM */

/* Define to 1 if you have the `mld' library (-lmld). */
/* #undef HAVE_LIBMLD */

/* Define to 1 if you have the `nsl' library (-lnsl). */
/* #undef HAVE_LIBNSL */

/* Define to 1 if you have the <libperfstat.h> header file. */
/* #undef HAVE_LIBPERFSTAT_H */

/* Define to 1 if you have the `pkcs11' library (-lpkcs11). */
/* #undef HAVE_LIBPKCS11 */

/* Define to 1 if you have the `RSAglue' library (-lRSAglue). */
/* #undef HAVE_LIBRSAGLUE */

/* Define to 1 if you have the `rsaref' library (-lrsaref). */
/* #undef HAVE_LIBRSAREF */

/* Define to 1 if you have the `sensors' library (-lsensors). */
/* #undef HAVE_LIBSENSORS */

/* Define to 1 if you have the `z' library (-lz). */
/* #undef HAVE_LIBZ */

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the <linux/hdreg.h> header file. */
/* #undef HAVE_LINUX_HDREG_H */

/* Define to 1 if you have the <linux/tasks.h> header file. */
/* #undef HAVE_LINUX_TASKS_H */

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define to 1 if you have the `lrand48' function. */
/* #undef HAVE_LRAND48 */

/* Define to 1 if you have the <machine/param.h> header file. */
/* #undef HAVE_MACHINE_PARAM_H */

/* Define to 1 if you have the <machine/pte.h> header file. */
/* #undef HAVE_MACHINE_PTE_H */

/* Define to 1 if you have the <machine/types.h> header file. */
/* #undef HAVE_MACHINE_TYPES_H */

/* Define to 1 if you have the <malloc.h> header file. */
#define HAVE_MALLOC_H 1

/* Define to 1 if you have the `memcpy' function. */
#define HAVE_MEMCPY 1

/* Define to 1 if you have the `memmove' function. */
#define HAVE_MEMMOVE 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `mkstemp' function. */
/* #undef HAVE_MKSTEMP */

/* Define to 1 if you have the `mktime' function. */
/* #undef HAVE_MKTIME */

/* Define to 1 if you have the <mntent.h> header file. */
/* #undef HAVE_MNTENT_H */

/* Define to 1 if you have the <mtab.h> header file. */
/* #undef HAVE_MTAB_H */

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if you have the <netdb.h> header file. */
/* #undef HAVE_NETDB_H */

/* Define to 1 if you have the <netinet6/in6_pcb.h> header file. */
/* #undef HAVE_NETINET6_IN6_PCB_H */

/* Define to 1 if you have the <netinet6/in6_var.h> header file. */
/* #undef HAVE_NETINET6_IN6_VAR_H */

/* Define to 1 if you have the <netinet6/ip6_var.h> header file. */
/* #undef HAVE_NETINET6_IP6_VAR_H */

/* Define to 1 if you have the <netinet6/nd6.h> header file. */
/* #undef HAVE_NETINET6_ND6_H */

/* Define to 1 if you have the <netinet6/tcp6_fsm.h> header file. */
/* #undef HAVE_NETINET6_TCP6_FSM_H */

/* Define to 1 if you have the <netinet6/tcp6.h> header file. */
/* #undef HAVE_NETINET6_TCP6_H */

/* Define to 1 if you have the <netinet6/tcp6_timer.h> header file. */
/* #undef HAVE_NETINET6_TCP6_TIMER_H */

/* Define to 1 if you have the <netinet6/tcp6_var.h> header file. */
/* #undef HAVE_NETINET6_TCP6_VAR_H */

/* Define to 1 if you have the <netinet/icmp_var.h> header file. */
/* #undef HAVE_NETINET_ICMP_VAR_H */

/* Define to 1 if you have the <netinet/if_ether.h> header file. */
/* #undef HAVE_NETINET_IF_ETHER_H */

/* Define to 1 if you have the <netinet/in.h> header file. */
/* #undef HAVE_NETINET_IN_H */

/* Define to 1 if you have the <netinet/in_systm.h> header file. */
/* #undef HAVE_NETINET_IN_SYSTM_H */

/* Define to 1 if you have the <netinet/in_var.h> header file. */
/* #undef HAVE_NETINET_IN_VAR_H */

/* Define to 1 if you have the <netinet/ip6.h> header file. */
/* #undef HAVE_NETINET_IP6_H */

/* Define to 1 if you have the <netinet/ip.h> header file. */
/* #undef HAVE_NETINET_IP_H */

/* Define to 1 if you have the <netinet/ip_icmp.h> header file. */
/* #undef HAVE_NETINET_IP_ICMP_H */

/* Define to 1 if you have the <netinet/ip_var.h> header file. */
/* #undef HAVE_NETINET_IP_VAR_H */

/* Define to 1 if you have the <netinet/tcpip.h> header file. */
/* #undef HAVE_NETINET_TCPIP_H */

/* Define to 1 if you have the <netinet/tcp_fsm.h> header file. */
/* #undef HAVE_NETINET_TCP_FSM_H */

/* Define to 1 if you have the <netinet/tcp.h> header file. */
/* #undef HAVE_NETINET_TCP_H */

/* Define to 1 if you have the <netinet/tcp_timer.h> header file. */
/* #undef HAVE_NETINET_TCP_TIMER_H */

/* Define to 1 if you have the <netinet/tcp_var.h> header file. */
/* #undef HAVE_NETINET_TCP_VAR_H */

/* Define to 1 if you have the <netinet/udp.h> header file. */
/* #undef HAVE_NETINET_UDP_H */

/* Define to 1 if you have the <netinet/udp_var.h> header file. */
/* #undef HAVE_NETINET_UDP_VAR_H */

/* Define to 1 if you have the <netipx/ipx.h> header file. */
/* #undef HAVE_NETIPX_IPX_H */

/* Define to 1 if you have the <net/if_dl.h> header file. */
/* #undef HAVE_NET_IF_DL_H */

/* Define to 1 if you have the <net/if.h> header file. */
/* #undef HAVE_NET_IF_H */

/* Define to 1 if you have the <net/if_mib.h> header file. */
/* #undef HAVE_NET_IF_MIB_H */

/* Define to 1 if you have the <net/if_types.h> header file. */
/* #undef HAVE_NET_IF_TYPES_H */

/* Define to 1 if you have the <net/if_var.h> header file. */
/* #undef HAVE_NET_IF_VAR_H */

/* Define to 1 if you have the <net/route.h> header file. */
/* #undef HAVE_NET_ROUTE_H */

/* Define to 1 if you have the `nlist' function. */
/* #undef HAVE_NLIST */

/* Define to 1 if you have the <nlist.h> header file. */
/* #undef HAVE_NLIST_H */

/* Define to 1 if you have the <openssl/aes.h> header file. */
/* #undef HAVE_OPENSSL_AES_H */

/* Define to 1 if you have the <openssl/des.h> header file. */
/* #undef HAVE_OPENSSL_DES_H */

/* Define to 1 if you have the <openssl/dh.h> header file. */
/* #undef HAVE_OPENSSL_DH_H */

/* Define to 1 if you have the <openssl/evp.h> header file. */
/* #undef HAVE_OPENSSL_EVP_H */

/* Define to 1 if you have the <openssl/hmac.h> header file. */
/* #undef HAVE_OPENSSL_HMAC_H */

/* Define to 1 if you have the <osreldate.h> header file. */
/* #undef HAVE_OSRELDATE_H */

/* Define to 1 if you have the `perl_eval_pv' function. */
/* #undef HAVE_PERL_EVAL_PV */

/* Define to 1 if you have the <pkginfo.h> header file. */
/* #undef HAVE_PKGINFO_H */

/* Define to 1 if you have the <pkglocs.h> header file. */
/* #undef HAVE_PKGLOCS_H */

/* Define if you have <process.h> header file. (Win32-getpid) */
#define HAVE_PROCESS_H 1

/* Define to 1 if you have the <pthread.h> header file. */
/* #undef HAVE_PTHREAD_H */

/* Define to 1 if you have the <pwd.h> header file. */
/* #undef HAVE_PWD_H */

/* Define to 1 if you have the `rand' function. */
#define HAVE_RAND 1

/* Define to 1 if you have the `random' function. */
#define HAVE_RAND 1

/* Define to 1 if you have the `regcomp' function. */
/* #undef HAVE_REGCOMP */

/* Define to 1 if you have the <regex.h> header file. */
/* #undef HAVE_REGEX_H */

/* Define to 1 if you have the `rpmGetPath' function. */
/* #undef HAVE_RPMGETPATH */

/* Define to 1 if you have the <rpmio.h> header file. */
/* #undef HAVE_RPMIO_H */

/* Define to 1 if you have the <rpm/rpmio.h> header file. */
/* #undef HAVE_RPM_RPMIO_H */

/* Define to 1 if you have the <search.h> header file. */
#define HAVE_SEARCH_H 1

/* Define to 1 if you have the <security/cryptoki.h> header file. */
/* #undef HAVE_SECURITY_CRYPTOKI_H */

/* Define to 1 if you have the `select' function. */
/* #undef HAVE_SELECT */

/* Define to 1 if you have the `setenv' function. */
/* #undef HAVE_SETENV */

/* Define to 1 if you have the `setgid' function. */
/* #undef HAVE_SETGID */

/* Define to 1 if you have the `setgroups' function. */
/* #undef HAVE_SETGROUPS */

/* Define to 1 if you have the `setitimer' function. */
/* #undef HAVE_SETITIMER */

/* Define to 1 if you have the `setlocale' function. */
#define HAVE_SETLOCALE 1

/* Define to 1 if you have the `setmntent' function. */
/* #undef HAVE_SETMNTENT */

/* Define to 1 if you have the `setsid' function. */
/* #undef HAVE_SETSID */

/* Define to 1 if you have the `setuid' function. */
/* #undef HAVE_SETUID */

/* Define to 1 if you have the <sgtty.h> header file. */
/* #undef HAVE_SGTTY_H */

/* Define to 1 if you have the `sigaction' function. */
/* #undef HAVE_SIGACTION */

/* Define to 1 if you have the `sigalrm' function. */
/* #undef HAVE_SIGALRM */

/* Define to 1 if you have the `sigblock' function. */
/* #undef HAVE_SIGBLOCK */

/* Define to 1 if you have the `sighold' function. */
/* #undef HAVE_SIGHOLD */

/* Define to 1 if you have the `signal' function. */
/* #undef HAVE_SIGNAL */

/* Define to 1 if you have the `sigset' function. */
/* #undef HAVE_SIGSET */

/* Define to 1 if you have the `snprintf' function. */
#define HAVE_SNPRINTF 1

/* Define to 1 if you have the `socket' function. */
#define HAVE_SOCKET 1

/* Define to 1 if you have the `statfs' function. */
/* #undef HAVE_STATFS */

/* Define to 1 if you have the `statvfs' function. */
/* #undef HAVE_STATVFS */

/* Define to 1 if you have the <stdarg.h> header file. */
#define HAVE_STDARG_H 1

/* Define to 1 if you have the <stdint.h> header file. */
/* #undef HAVE_STDINT_H */

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `stime' function. */
/* #undef HAVE_STIME */

/* Define to 1 if you have the `strcasestr' function. */
/* #undef HAVE_STRCASESTR */

/* Define to 1 if you have the `strchr' function. */
#define HAVE_STRCHR 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the <strings.h> header file. */
/* #undef HAVE_STRINGS_H */

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strlcpy' function. */
/* #undef HAVE_STRLCPY */

/* Define to 1 if you have the `strncasecmp' function. */
/* #undef HAVE_STRNCASECMP */

/* Define to 1 if you have the `strtol' function. */
#define HAVE_STRTOL 1

/* Define to 1 if you have the `strtoul' function. */
#define HAVE_STRTOUL 1

/* Define to 1 if you have the <syslog.h> header file. */
/* #undef HAVE_SYSLOG_H */

/* Define to 1 if you have the `system' function. */
#define HAVE_SYSTEM 1

/* Define to 1 if you have the <sys/cdefs.h> header file. */
/* #undef HAVE_SYS_CDEFS_H */

/* Define to 1 if you have the <sys/conf.h> header file. */
/* #undef HAVE_SYS_CONF_H */

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/diskio.h> header file. */
/* #undef HAVE_SYS_DISKIO_H */

/* Define to 1 if you have the <sys/dkio.h> header file. */
/* #undef HAVE_SYS_DKIO_H */

/* Define to 1 if you have the <sys/dmap.h> header file. */
/* #undef HAVE_SYS_DMAP_H */

/* Define to 1 if you have the <sys/file.h> header file. */
/* #undef HAVE_SYS_FILE_H */

/* Define to 1 if you have the <sys/filio.h> header file. */
/* #undef HAVE_SYS_FILIO_H */

/* Define to 1 if you have the <sys/fixpoint.h> header file. */
/* #undef HAVE_SYS_FIXPOINT_H */

/* Define to 1 if you have the <sys/fs.h> header file. */
/* #undef HAVE_SYS_FS_H */

/* Define to 1 if you have the <sys/hashing.h> header file. */
/* #undef HAVE_SYS_HASHING_H */

/* Define to 1 if you have the <sys/ioctl.h> header file. */
/* #undef HAVE_SYS_IOCTL_H */

/* Define to 1 if you have the <sys/loadavg.h> header file. */
/* #undef HAVE_SYS_LOADAVG_H */

/* Define to 1 if you have the <sys/mbuf.h> header file. */
/* #undef HAVE_SYS_MBUF_H */

/* Define to 1 if you have the <sys/mntent.h> header file. */
/* #undef HAVE_SYS_MNTENT_H */

/* Define to 1 if you have the <sys/mnttab.h> header file. */
/* #undef HAVE_SYS_MNTTAB_H */

/* Define to 1 if you have the <sys/mount.h> header file. */
/* #undef HAVE_SYS_MOUNT_H */

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/param.h> header file. */
/* #undef HAVE_SYS_PARAM_H */

/* Define to 1 if you have the <sys/pool.h> header file. */
/* #undef HAVE_SYS_POOL_H */

/* Define to 1 if you have the <sys/proc.h> header file. */
/* #undef HAVE_SYS_PROC_H */

/* Define to 1 if you have the <sys/protosw.h> header file. */
/* #undef HAVE_SYS_PROTOSW_H */

/* Define to 1 if you have the <sys/pstat.h> header file. */
/* #undef HAVE_SYS_PSTAT_H */

/* Define to 1 if you have the <sys/queue.h> header file. */
/* #undef HAVE_SYS_QUEUE_H */

/* Define to 1 if you have the <sys/select.h> header file. */
/* #undef HAVE_SYS_SELECT_H */

/* Define to 1 if you have the <sys/socketvar.h> header file. */
/* #undef HAVE_SYS_SOCKETVAR_H */

/* Define to 1 if you have the <sys/socket.h> header file. */
/* #undef HAVE_SYS_SOCKET_H */

/* Define to 1 if you have the <sys/sockio.h> header file. */
/* #undef HAVE_SYS_SOCKIO_H */

/* Define to 1 if you have the <sys/statfs.h> header file. */
/* #undef HAVE_SYS_STATFS_H */

/* Define to 1 if you have the <sys/statvfs.h> header file. */
/* #undef HAVE_SYS_STATVFS_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/stream.h> header file. */
/* #undef HAVE_SYS_STREAM_H */

/* Define to 1 if you have the <sys/swap.h> header file. */
/* #undef HAVE_SYS_SWAP_H */

/* Define to 1 if you have the <sys/sysctl.h> header file. */
/* #undef HAVE_SYS_SYSCTL_H */

/* Define to 1 if you have the <sys/sysmp.h> header file. */
/* #undef HAVE_SYS_SYSMP_H */

/* Define to 1 if you have the <sys/tcpipstats.h> header file. */
/* #undef HAVE_SYS_TCPIPSTATS_H */

/* Define to 1 if you have the <sys/time.h> header file. */
/* #undef HAVE_SYS_TIME_H */

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/un.h> header file. */
/* #undef HAVE_SYS_UN_H */

/* Define to 1 if you have the <sys/user.h> header file. */
/* #undef HAVE_SYS_USER_H */

/* Define to 1 if you have the <sys/utsname.h> header file. */
/* #undef HAVE_SYS_UTSNAME_H */

/* Define to 1 if you have the <sys/vfs.h> header file. */
/* #undef HAVE_SYS_VFS_H */

/* Define to 1 if you have the <sys/vmmac.h> header file. */
/* #undef HAVE_SYS_VMMAC_H */

/* Define to 1 if you have the <sys/vmmeter.h> header file. */
/* #undef HAVE_SYS_VMMETER_H */

/* Define to 1 if you have the <sys/vmparam.h> header file. */
/* #undef HAVE_SYS_VMPARAM_H */

/* Define to 1 if you have the <sys/vmsystm.h> header file. */
/* #undef HAVE_SYS_VMSYSTM_H */

/* Define to 1 if you have the <sys/vm.h> header file. */
/* #undef HAVE_SYS_VM_H */

/* Define to 1 if you have the <sys/vnode.h> header file. */
/* #undef HAVE_SYS_VNODE_H */

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
/* #undef HAVE_SYS_WAIT_H */

/* Define to 1 if you have the `tcgetattr' function. */
/* #undef HAVE_TCGETATTR */

/* Define to 1 if you have the <ufs/ffs/fs.h> header file. */
/* #undef HAVE_UFS_FFS_FS_H */

/* Define to 1 if you have the <ufs/fs.h> header file. */
/* #undef HAVE_UFS_FS_H */

/* Define to 1 if you have the <ufs/ufs/dinode.h> header file. */
/* #undef HAVE_UFS_UFS_DINODE_H */

/* Define to 1 if you have the <ufs/ufs/inode.h> header file. */
/* #undef HAVE_UFS_UFS_INODE_H */

/* Define to 1 if you have the <ufs/ufs/quota.h> header file. */
/* #undef HAVE_UFS_UFS_QUOTA_H */

/* Define to 1 if you have the `uname' function. */
/* #undef HAVE_UNAME */

/* Define to 1 if you have the <unistd.h> header file. */
/* #undef HAVE_UNISTD_H */

/* Define to 1 if you have the `usleep' function. */
/* #undef HAVE_USLEEP */

/* Define to 1 if you have the <utmpx.h> header file. */
/* #undef HAVE_UTMPX_H */

/* Define to 1 if you have the <utsname.h> header file. */
/* #undef HAVE_UTSNAME_H */

/* Define to 1 if you have the <uvm/uvm_extern.h> header file. */
/* #undef HAVE_UVM_UVM_EXTERN_H */

/* Define to 1 if you have the <uvm/uvm_param.h> header file. */
/* #undef HAVE_UVM_UVM_PARAM_H */

/* Define to 1 if you have the <vm/swap_pager.h> header file. */
/* #undef HAVE_VM_SWAP_PAGER_H */

/* Define to 1 if you have the <vm/vm_extern.h> header file. */
/* #undef HAVE_VM_VM_EXTERN_H */

/* Define to 1 if you have the <vm/vm.h> header file. */
/* #undef HAVE_VM_VM_H */

/* Define to 1 if you have the <vm/vm_param.h> header file. */
/* #undef HAVE_VM_VM_PARAM_H */

/* Define to 1 if you have the `vsnprintf' function. */
#define HAVE_VSNPRINTF 1

/* Define to 1 if you have the <winsock.h> header file. */
#define HAVE_WINSOCK_H 1

/* Define to 1 if you have the <xti.h> header file. */
/* #undef HAVE_XTI_H */

/* Define to the address where bug reports for this package should be sent. */
/* #undef PACKAGE_BUGREPORT */

/* Define to the full name of this package. */
#define PACKAGE_NAME "Net-SNMP"

/* Define to the full name and version of this package. */
/* #undef PACKAGE_STRING */

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "net-snmp"

/* Define to the version of this package. */
/* #undef PACKAGE_VERSION */

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* The size of a `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of a `long', as computed by sizeof. */
#define SIZEOF_LONG 4

/* The size of a `long long', as computed by sizeof. */
#define SIZEOF_LONG_LONG 8

/* The size of a `short', as computed by sizeof. */
#define SIZEOF_SHORT 2

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at run-time.
        STACK_DIRECTION > 0 => grows toward higher addresses
        STACK_DIRECTION < 0 => grows toward lower addresses
        STACK_DIRECTION = 0 => direction of growth unknown */
/* #undef STACK_DIRECTION */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
/* #undef WORDS_BIGENDIAN */

/* Define to 1 if on AIX 3.
   System headers sometimes define this.
   We just want to avoid a redefinition error message.  */
#ifndef _ALL_SOURCE
/* # undef _ALL_SOURCE */
#endif

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define as `__inline' if that's what the C compiler calls it, or to nothing
   if it is not supported. */
#define inline __inline

/* Define to `long' if <sys/types.h> does not define. */
/* #undef off_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* define if you have getdevs() */
/* #undef HAVE_GETDEVS */

/* define if you have devstat_getdevs() */
/* #undef HAVE_DEVSTAT_GETDEVS */

/* define if you have <netinet/in_pcb.h> */
/* #undef HAVE_NETINET_IN_PCB_H */

/* define if you have <sys/disklabel.h> */
/* #undef HAVE_SYS_DISKLABEL_H */

/* define if you are using linux and /proc/net/dev has the compressed
   field, which exists in linux kernels 2.2 and greater. */
/* #undef PROC_NET_DEV_HAS_COMPRESSED */

/* define rtentry to ortentry on SYSV machines (alphas) */
#define RTENTRY rtentry;

/* Use BSD 4.4 routing table entries? */
/* #undef RTENTRY_4_4 */

/* Does struct sigaction have a sa_sigaction field? */
/* #undef STRUCT_SIGACTION_HAS_SA_SIGACTION */

/* Does struct sockaddr have a sa_len field? */
/* #undef STRUCT_SOCKADDR_HAS_SA_LEN */

/* Does struct sockaddr have a sa_family2 field? */
/* #undef STRUCT_SOCKADDR_HAS_SA_UNION_SA_GENERIC_SA_FAMILY2 */

/* Does struct in6_addr have a s6_un.sa6_ladd field? */
/* #undef STRUCT_IN6_ADDR_HAS_S6_UN_SA6_LADDR */

/* rtentry structure tests */
/* #undef RTENTRY_RT_NEXT */
/* #undef STRUCT_RTENTRY_HAS_RT_DST */
/* #undef STRUCT_RTENTRY_HAS_RT_UNIT */
/* #undef STRUCT_RTENTRY_HAS_RT_USE */
/* #undef STRUCT_RTENTRY_HAS_RT_REFCNT */
/* #undef STRUCT_RTENTRY_HAS_RT_HASH */

/* ifnet structure tests */
/* #undef STRUCT_IFNET_HAS_IF_BAUDRATE */
/* #undef STRUCT_IFNET_HAS_IF_BAUDRATE_IFS_VALUE */
/* #undef STRUCT_IFNET_HAS_IF_SPEED */
/* #undef STRUCT_IFNET_HAS_IF_TYPE */
/* #undef STRUCT_IFNET_HAS_IF_IMCASTS */
/* #undef STRUCT_IFNET_HAS_IF_IQDROPS */
/* #undef STRUCT_IFNET_HAS_IF_LASTCHANGE_TV_SEC */
/* #undef STRUCT_IFNET_HAS_IF_NOPROTO */
/* #undef STRUCT_IFNET_HAS_IF_OMCASTS */
/* #undef STRUCT_IFNET_HAS_IF_XNAME */
/* #undef STRUCT_IFNET_HAS_IF_OBYTES */
/* #undef STRUCT_IFNET_HAS_IF_IBYTES */
/* #undef STRUCT_IFNET_HAS_IF_ADDRLIST */

/* tcpstat.tcps_rcvmemdrop */
/* #undef STRUCT_TCPSTAT_HAS_TCPS_RCVMEMDROP */

/* udpstat.udps_discard */
/* #undef STRUCT_UDPSTAT_HAS_UDPS_DISCARD */

/* udpstat.udps_discard */
/* #undef STRUCT_UDPSTAT_HAS_UDPS_NOPORT */

/* udpstat.udps_discard */
/* #undef STRUCT_UDPSTAT_HAS_UDPS_NOPORTBCAST */

/* udpstat.udps_discard */
/* #undef STRUCT_UDPSTAT_HAS_UDPS_FULLSOCK */

/* arphd.at_next */
/* #undef STRUCT_ARPHD_HAS_AT_NEXT */

/* ifaddr.ifa_next */
/* #undef STRUCT_IFADDR_HAS_IFA_NEXT */

/* ifnet.if_mtu */
/* #undef STRUCT_IFNET_HAS_IF_MTU */

/* swdevt.sw_nblksenabled */
/* #undef STRUCT_SWDEVT_HAS_SW_NBLKSENABLED */

/* nlist.n_value */
/* #undef STRUCT_NLIST_HAS_N_VALUE */

/* ipstat structure tests */
/* #undef STRUCT_IPSTAT_HAS_IPS_CANTFORWARD */
/* #undef STRUCT_IPSTAT_HAS_IPS_CANTFRAG */
/* #undef STRUCT_IPSTAT_HAS_IPS_DELIVERED */
/* #undef STRUCT_IPSTAT_HAS_IPS_FRAGDROPPED */
/* #undef STRUCT_IPSTAT_HAS_IPS_FRAGTIMEOUT */
/* #undef STRUCT_IPSTAT_HAS_IPS_LOCALOUT */
/* #undef STRUCT_IPSTAT_HAS_IPS_NOPROTO */
/* #undef STRUCT_IPSTAT_HAS_IPS_NOROUTE */
/* #undef STRUCT_IPSTAT_HAS_IPS_ODROPPED */
/* #undef STRUCT_IPSTAT_HAS_IPS_OFRAGMENTS */
/* #undef STRUCT_IPSTAT_HAS_IPS_REASSEMBLED */

/* vfsstat.f_frsize */
/* #undef STRUCT_STATVFS_HAS_F_FRSIZE */

/* vfsstat.f_files */
/* #undef STRUCT_STATVFS_HAS_F_FILES */

/* statfs inode structure tests*/
/* #undef STRUCT_STATFS_HAS_F_FILES */
/* #undef STRUCT_STATFS_HAS_F_FFREE */
/* #undef STRUCT_STATFS_HAS_F_FAVAIL */

/* des_ks_struct.weak_key */
/* #undef STRUCT_DES_KS_STRUCT_HAS_WEAK_KEY */

/* ifnet needs to have _KERNEL defined */
/* #undef IFNET_NEEDS_KERNEL */

/* sysctl works to get boottime, etc... */
/* #undef CAN_USE_SYSCTL */

/* type check for in_addr_t */
/* #undef in_addr_t */

/* define if SIOCGIFADDR exists in sys/ioctl.h */
/* #undef SYS_IOCTL_H_HAS_SIOCGIFADDR */

/* define if your compiler (processor) defines __FUNCTION__ for you */
/* #undef HAVE_CPP_UNDERBAR_FUNCTION_DEFINED */

/* Mib-2 tree Info */
/* These are the system information variables. */

#define VERS_DESC   "unknown"             /* overridden at run time */
#define SYS_NAME    "unknown"             /* overridden at run time */

/* comment out the second define to turn off functionality for any of
   these: (See README for details) */

/*   proc PROCESSNAME [MAX] [MIN] */
#define PROCMIBNUM 2

/*   exec/shell NAME COMMAND      */
#define SHELLMIBNUM 8

/*   swap MIN                     */
#define MEMMIBNUM 4

/*   disk DISK MINSIZE            */
#define DISKMIBNUM 9

/*   load 1 5 15                  */
#define LOADAVEMIBNUM 10

/* which version are you using? This mibloc will tell you */
#define VERSIONMIBNUM 100

/* Reports errors the agent runs into */
/* (typically its "can't fork, no mem" problems) */
#define ERRORMIBNUM 101

/* The sub id of EXTENSIBLEMIB returned to queries of
   .iso.org.dod.internet.mgmt.mib-2.system.sysObjectID.0 */
#define AGENTID 250

/* This ID is returned after the AGENTID above.  IE, the resulting
   value returned by a query to sysObjectID is
   EXTENSIBLEMIB.AGENTID.???, where ??? is defined below by OSTYPE */

#define HPUX9ID 1
#define SUNOS4ID 2
#define SOLARISID 3
#define OSFID 4
#define ULTRIXID 5
#define HPUX10ID 6
#define NETBSD1ID 7
#define FREEBSDID 8
#define IRIXID 9
#define LINUXID 10
#define BSDIID 11
#define OPENBSDID 12
#define WIN32ID 13
#define HPUX11ID 14
#define UNKNOWNID 255

#ifdef hpux9
#define OSTYPE HPUX9ID
#endif
#ifdef hpux10
#define OSTYPE HPUX10ID
#endif
#ifdef hpux11
#define OSTYPE HPUX11ID
#endif
#ifdef sunos4
#define OSTYPE SUNOS4ID
#endif
#ifdef solaris2
#define OSTYPE SOLARISID
#endif
#if defined(osf3) || defined(osf4) || defined(osf5)
#define OSTYPE OSFID
#endif
#ifdef ultrix4
#define OSTYPE ULTRIXID
#endif
#ifdef netbsd1
#define OSTYPE NETBSD1ID
#endif
#if defined(__FreeBSD__)
#define OSTYPE FREEBSDID
#endif
#if defined(irix6) || defined(irix5)
#define OSTYPE IRIXID
#endif
#ifdef linux
#define OSTYPE LINUXID
#endif
#if defined(bsdi2) || defined(bsdi3) || defined(bsdi4)
#define OSTYPE BSDIID
#endif
#ifdef openbsd2
#define OSTYPE OPENBSDID
#endif
#ifdef WIN32
#define OSTYPE WIN32ID
#endif
/* unknown */
#ifndef OSTYPE
#define OSTYPE UNKNOWNID
#endif

/* The enterprise number has been assigned by the IANA group.   */
/* Optionally, this may point to the location in the tree your  */
/* company/organization has been allocated.                     */
/* The assigned enterprise number for the NET_SNMP MIB modules. */
#define ENTERPRISE_OID			8072
#define ENTERPRISE_MIB			1,3,6,1,4,1,8072
#define ENTERPRISE_DOT_MIB		1.3.6.1.4.1.8072
#define ENTERPRISE_DOT_MIB_LENGTH	7

/* The assigned enterprise number for sysObjectID. */
#define SYSTEM_MIB		1,3,6,1,4,1,8072,3,2,OSTYPE
#define SYSTEM_DOT_MIB		1.3.6.1.4.1.8072.3.2.OSTYPE
#define SYSTEM_DOT_MIB_LENGTH	10

/* The assigned enterprise number for notifications. */
#define NOTIFICATION_MIB		1,3,6,1,4,1,8072,4
#define NOTIFICATION_DOT_MIB		1.3.6.1.4.1.8072.4
#define NOTIFICATION_DOT_MIB_LENGTH	8

/* this is the location of the ucdavis mib tree.  It shouldn't be
   changed, as the places it is used are expected to be constant
   values or are directly tied to the UCD-SNMP-MIB. */
#define UCDAVIS_OID		2021
#define UCDAVIS_MIB		1,3,6,1,4,1,2021
#define UCDAVIS_DOT_MIB		1.3.6.1.4.1.2021
#define UCDAVIS_DOT_MIB_LENGTH	7

/* this is the location of the net-snmp mib tree.  It shouldn't be
   changed, as the places it is used are expected to be constant
   values or are directly tied to the UCD-SNMP-MIB. */
#define NETSNMP_OID		8072
#define NETSNMP_MIB		1,3,6,1,4,1,8072
#define NETSNMP_DOT_MIB		1.3.6.1.4.1.8072
#define NETSNMP_DOT_MIB_LENGTH	7
      
/* how long to wait (seconds) for error querys before reseting the error trap.*/
#define ERRORTIMELENGTH 600

/* Exec command to fix PROC problems */
/* %s will be replaced by the process name in error */

/* #define PROCFIXCMD "/usr/bin/perl /local/scripts/fixproc %s" */

/* Exec command to fix EXEC problems */
/* %s will be replaced by the exec/script name in error */

/* #define EXECFIXCMD "/usr/bin/perl /local/scripts/fixproc %s" */

/* Should exec output Cashing be used (speeds up things greatly), and
   if so, After how many seconds should the cache re-newed?  Note:
   Don't define CASHETIME to disable cashing completely */

#define EXCACHETIME 30
#define CACHEFILE ".snmp-exec-cache"
#define MAXCACHESIZE (200*80)   /* roughly 200 lines max */

#define MAXDISKS 50                      /* can't scan more than this number */

/* misc defaults */

/* default of 100 meg minimum if the minimum size is not specified in
   the config file */
#define DEFDISKMINIMUMSPACE 100000

#define DEFMAXLOADAVE 12.0      /* default maximum load average before error */

/* Because of sleep(1)s, this will also be time to wait (in seconds) for exec
   to finish */
#define MAXREADCOUNT 100   /* max times to loop reading output from execs. */

/* The original CMU code had this hardcoded as = 1 */
#define SNMPBLOCK 1       /* Set if snmpgets should block and never timeout */

/* How long to wait before restarting the agent after a snmpset to
   EXTENSIBLEMIB.VERSIONMIBNUM.VERRESTARTAGENT.  This is
   necessary to finish the snmpset reply before restarting. */
#define RESTARTSLEEP 5

/* Number of community strings to store */
#define NUM_COMMUNITIES	5

/* UNdefine to allow specifying zero-length community string */
/* #define NO_ZEROLENGTH_COMMUNITY 1 */

/* #define EXIT_ON_BAD_KLREAD  */
/* define to exit the agent on a bad kernel read */

#define LASTFIELD -1      /* internal define */

/* configure options specified */
#define CONFIGURE_OPTIONS ""

/* got socklen_t? */
#ifdef HAVE_WIN32_PLATFORM_SDK
#define HAVE_SOCKLEN_T 1
#endif

/* got in_addr_t? */
/* #undef HAVE_IN_ADDR_T */

/* got ssize_t? */
/* #undef HAVE_SSIZE_T */

#ifndef HAVE_STRCHR
#ifdef HAVE_INDEX
# define strchr(a,b) index(a,b)
# define strrchr(a,b) rindex(a,b)
#endif
#endif

#ifndef HAVE_INDEX
#ifdef HAVE_STRCHR
# define index(a,b) strchr(a,b)
# define rindex(a,b) strrchr(a,b)
#endif
#endif

#ifndef HAVE_MEMCPY
#ifdef HAVE_BCOPY
# define memcpy(d, s, n) bcopy ((s), (d), (n))
# define memmove(d, s, n) bcopy ((s), (d), (n))
# define memcmp bcmp
#endif
#endif

#ifndef HAVE_MEMMOVE
#ifdef HAVE_MEMCPY
# define memmove memcpy
#endif
#endif

#if notused /* dont step on other defns of bcopy,bzero, and bcmp */
#ifndef HAVE_BCOPY
#ifdef HAVE_MEMCPY
# define bcopy(s, d, n) memcpy ((d), (s), (n))
# define bzero(p,n) memset((p),(0),(n))
# define bcmp memcmp
#endif
#endif
#endif

/* If you have openssl 0.9.7 or above, you likely have AES support. */
/* #undef USE_OPENSSL */

#ifdef USE_OPENSSL

/* Define to 1 if you have the <openssl/dh.h> header file. */
#define HAVE_OPENSSL_DH_H 1

/* Define to 1 if you have the `AES_cfb128_encrypt' function. */
#define HAVE_AES_CFB128_ENCRYPT 1

#else /* ! USE_OPENSSL */

/* define if you are using the MD5 code ...*/
#define USE_INTERNAL_MD5 1

#endif /* ! USE_OPENSSL */

#if defined(USE_OPENSSL) && defined(HAVE_OPENSSL_AES_H) && defined(HAVE_AES_CFB128_ENCRYPT)
#define HAVE_AES 1
#endif

/* define random functions */

#ifndef HAVE_RANDOM
#ifdef HAVE_LRAND48
#define random lrand48
#define srandom(s) srand48(s)
#else
#ifdef HAVE_RAND
#define random rand
#define srandom(s) srand(s)
#endif
#endif
#endif

/* define signal if DNE */

#ifndef HAVE_SIGNAL
#ifdef HAVE_SIGSET
#define signal(a,b) sigset(a,b)
#endif
#endif

/* define if you have librpm and libdb */
/* #undef HAVE_LIBDB */
/* #undef HAVE_LIBRPM */

/* define if you have pkginfo */
/* #undef HAVE_PKGINFO */

/* define if you have gethostbyname */
#define HAVE_GETHOSTBYNAME 1

/* define if you have getservbyname */
#define HAVE_GETSERVBYNAME 1

/* printing system */
/* #undef HAVE_LPSTAT */
/* #undef LPSTAT_PATH */
/* #undef HAVE_PRINTCAP */

/*  Pluggable transports.  */

/*  This is defined if support for the UDP/IP transport domain is
    available.   */
#define SNMP_TRANSPORT_UDP_DOMAIN 1

/*  This is defined if support for the "callback" transport domain is
    available.   */
#define SNMP_TRANSPORT_CALLBACK_DOMAIN 1

/*  This is defined if support for the TCP/IP transport domain is
    available.  */
#define SNMP_TRANSPORT_TCP_DOMAIN 1

/*  This is defined if support for the Unix transport domain
    (a.k.a. "local IPC") is available.  */
/* #undef SNMP_TRANSPORT_UNIX_DOMAIN */

/*  This is defined if support for the AAL5 PVC transport domain is
    available.  */
/* #undef SNMP_TRANSPORT_AAL5PVC_DOMAIN */

/*  This is defined if support for the IPX transport domain is
    available.  */
/* #undef SNMP_TRANSPORT_IPX_DOMAIN */

/* XXX do not modify. change the INET6 define instead */
/*  This is defined if support for the UDP/IPv6 transport domain is
    available.  */
/* #undef SNMP_TRANSPORT_UDPIPV6_DOMAIN */

/* XXX do not modify. change the INET6 define instead */
/*  This is defined if support for the TCP/IPv6 transport domain is
    available.  */
/* #undef SNMP_TRANSPORT_TCPIPV6_DOMAIN */

/* define this if the USM security module is available */
#define SNMP_SECMOD_USM 1

/* define this if the KSM (kerberos based snmp) security module is available */
/* #undef SNMP_SECMOD_KSM */

/* define this if we're using the new MIT crypto API */
/* #undef MIT_NEW_CRYPTO */

/* define if you want to build with reentrant/threaded code (incomplete)*/
/* #undef NS_REENTRANT */

/* on aix, if you have perfstat */
/* #undef HAVE_PERFSTAT */

/* Not-to-be-compiled macros for use by configure only */
#define config_require(x)
#define config_exclude(x)
#define config_arch_require(x,y)
#define config_parse_dot_conf(w,x,y,z)
#define config_add_mib(x)
#define config_belongs_in(x)

#if defined (WIN32) || defined (mingw32) || defined (cygwin)
#define ENV_SEPARATOR ";"
#define ENV_SEPARATOR_CHAR ';'
#else
#define ENV_SEPARATOR ":"
#define ENV_SEPARATOR_CHAR ':'
#endif

/*
 * this must be before the system/machine includes, to allow them to
 * override and turn off inlining. To do so, they should do the
 * following:
 *
 *    #undef NETSNMP_ENABLE_INLINE
 *    #define NETSNMP_ENABLE_INLINE 0
 *
 * A user having problems with their compiler can also turn off
 * the use of inline by defining NETSNMP_NO_INLINE via their cflags:
 *
 *    -DNETSNMP_NO_INLINE
 *
 * Header and source files should only test against NETSNMP_USE_INLINE:
 *
 *   #ifdef NETSNMP_USE_INLINE
 *   NETSNMP_INLINE function(int parm) { return parm -1; }
 *   #endif
 *
 * Functions which should be static, regardless of whether or not inline
 * is available or enabled should use the NETSNMP_STATIC_INLINE macro,
 * like so:
 *
 *    NETSNMP_STATIC_INLINE function(int parm) { return parm -1; }
 *
 * NOT like this:
 *
 *    static NETSNMP_INLINE function(int parm) { return parm -1; }
 *
 */
/*
 * Win32 needs extern for inline function declarations in headers.
 * See MS tech note Q123768:
 *   http://support.microsoft.com/default.aspx?scid=kb;EN-US;123768
 */
#define NETSNMP_INLINE extern inline
#define NETSNMP_STATIC_INLINE static inline
#define NETSNMP_ENABLE_INLINE 1

#if notused
#include SYSTEM_INCLUDE_FILE
#include MACHINE_INCLUDE_FILE
#endif

#if NETSNMP_ENABLE_INLINE && !defined(NETSNMP_NO_INLINE)
#   define NETSNMP_USE_INLINE 1
#else
#   undef  NETSNMP_INLINE
#   define NETSNMP_INLINE 
#   undef  NETSNMP_STATIC_INLINE
#   define NETSNMP_STATIC_INLINE static
#endif

#ifdef WIN32

typedef unsigned short mode_t;
typedef unsigned __int32 uint32_t;
typedef long int32_t;
typedef unsigned __int64 uint64_t;
typedef __int64 int64_t;

/* Define if you have the closesocket function.  */
#define HAVE_CLOSESOCKET 1

/* Define if you have raise() instead of alarm() */
#define HAVE_RAISE 1

/* define to 1 if you do not want to set global snmp_errno */
#define DONT_SHARE_ERROR_WITH_OTHER_THREADS 1

#define vsnprintf _vsnprintf
#define snprintf  _snprintf

#define EADDRINUSE	WSAEADDRINUSE

/* Define NETSNMP_USE_DLL when building or using netsnmp.DLL */
/* #undef NETSNMP_USE_DLL */

#ifdef NETSNMP_USE_DLL
  #ifdef NETSNMP_DLL
    #if defined(_MSC_VER)
      #define NETSNMP_IMPORT __declspec(dllexport)
    #endif
  #else
    #if defined(_MSC_VER)
      #define NETSNMP_IMPORT __declspec(dllimport)
    #endif
  #endif   /* NETSNMP_DLL */
#endif     /* NETSNMP_USE_DLL */

/*
 * DLL decoration, if used at all, must be consistent.
 * This is why NETSNMP_IMPORT is really an export decoration
 * when it is encountered in a header file that is included
 * during the compilation of a library source file.
 * NETSNMP_DLL is set by the MSVC libsnmp_dll project
 *  in order to signal that the library sources are being compiled.
 * Not defining NETSNMP_USE_DLL ignores the preceding, and renders
 *  the NETSNMP_IMPORT definitions harmless.
 */


  #ifdef NETSNMP_USE_DLL
    #ifndef NETSNMP_TOOLS_C

  /* wrap alloc functions to use DLL's memory heap */
  /* This is not done in tools.c, where these wrappers are defined */

      #define strdup    netsnmp_strdup
      #define calloc    netsnmp_calloc
      #define malloc    netsnmp_malloc
      #define realloc   netsnmp_realloc
      #define free      netsnmp_free
    #endif
  #endif

#endif       /* WIN32 */

#ifndef NETSNMP_IMPORT
#  define NETSNMP_IMPORT extern
#endif

#if defined(HAVE_NLIST) && defined(STRUCT_NLIST_HAS_N_VALUE) && !defined(DONT_USE_NLIST) && !defined(NO_KMEM_USAGE)
#define CAN_USE_NLIST
#endif

#if HAVE_DMALLOC_H
#define DMALLOC_FUNC_CHECK
#endif

/* #undef INET6 */
/* #undef LOCAL_SMUX */

/* define if agentx transport is to use domain sockets only */
/* #undef AGENTX_DOM_SOCK_ONLY */

#ifndef LOG_DAEMON
#define       LOG_DAEMON      (3<<3)  /* system daemons */
#endif

#if UCD_COMPATIBLE
/* old and in the way */
#define EXTENSIBLEMIB UCDAVIS_MIB
#endif

#if defined(_MSC_VER)
/* this bit of magic enables or disables IPv6 transports */
  #if INET6
    /* "dont have" implies "compile here" for snmplib/inet_?to?.c */
    #undef HAVE_INET_NTOP
    #undef HAVE_INET_PTON
    #define SNMP_TRANSPORT_TCPIPV6_DOMAIN 1
    #define SNMP_TRANSPORT_UDPIPV6_DOMAIN 1
  #else
    /* "have" implies "dont compile here" for snmplib/inet_?to?.c */
    #define HAVE_INET_NTOP 1
    #define HAVE_INET_PTON 1
    #undef SNMP_TRANSPORT_TCPIPV6_DOMAIN
    #undef SNMP_TRANSPORT_UDPIPV6_DOMAIN
  #endif
#endif

/*
 * Module configuration and control starts here.
 *
 * Some of the defines herein are used to control
 * groups of modules.  The ones that have "CFG"
 * are used especially to control the include files
 * seen in {agent,mib}_module_includes.h, and the init entries
 * which are invoked in {agent,mib}_module_inits.h.
 *
 * To disable a group, uncomment the associated define.
 */
 
/* CFG Define if compiling with the ucd_snmp module files.  */
#define USING_UCD_SNMP_MODULE 1
 
/* CFG Define if compiling with the agentx module files.  */
#define USING_AGENTX_MODULE 1
 
/* CFG Define if compiling with the host module files.  */
/* #undef USING_HOST_MODULE */
 
/* CFG Define if compiling with the Rmon module files.  */
/* #undef USING_RMON_MODULE */

/* CFG Define if compiling with the disman/event-mib module files.  */
#define USING_DISMAN_EVENT_MIB_MODULE 1

/* CFG Define if compiling with the smux module files.  */
/* #undef USING_SMUX_MODULE */

/*
 * Module configuration and control ends here.
 */

#endif /* NET_SNMP_CONFIG_H */

