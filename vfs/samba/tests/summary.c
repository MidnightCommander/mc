#include <stdio.h>

main()
{
#if !(defined(HAVE_NETMASK_IFCONF) || defined(HAVE_NETMASK_IFREQ) || defined(HAVE_NETMASK_AIX))
	printf("WARNING: No automated netmask determination - use an interfaces line\n");
#endif

#if !((defined(HAVE_RANDOM) || defined(HAVE_RAND)) && (defined(HAVE_SRANDOM) || defined(HAVE_SRAND)))
    printf("ERROR: No random or srandom routine!\n");
    exit(1);
#endif

	exit(0);
}
