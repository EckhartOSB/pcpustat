#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/resource.h>
#include <sys/sysctl.h>

static const char* what_string="@(#)pcpustat 1.1";

/* Bit flags for what stats to include: */

#define STAT_USER	0x01
#define STAT_NICE	0x02
#define STAT_SYSTEM	0x04
#define STAT_INT	0x08
#define STAT_IDLE	0x10
#define STAT_ALL	0x1F


/* Copied from /usr/src/usr.bin/top/machine.c */
#define GETSYSCTL(name, var) getsysctl(name, &(var), sizeof(var))
static void getsysctl(const char *name, void *ptr, size_t len);

struct opthelp {
    char *argname;
    char *description;
};

int main(int ac, char **av)
{
    int c, option_index, stats=0, count=-1, wait=0, cpu, maxcpu, ncpu, quiet=0, not=0;
    size_t state_size;
    long cpus=0;
    long *cpu_prev, *cpu_curr;
    struct option options[] = {
      {"all", 0, 0, 'a'},
      {"count", 1, 0, 'c'},
      {"help", 0, 0, 'h'},
      {"idle", 0, 0, 'i'},
      {"nice", 0, 0, 'n'},
      {"cpu", 1, 0, 'p'},
      {"quiet", 0, 0, 'q'},
      {"system", 0, 0, 's'},
      {"interrupt", 0, 0, 't'},
      {"user", 0, 0, 'u'},
      {"wait", 1, 0, 'w'},
      {"not", 0, 0, 'x'}
    };
    struct opthelp help[] = {
      {0,"include all usage statistics (-instu)"},
      {"count","repeat count times (default = forever if wait is specified)"},
      {0,"print this list and exit"},
      {0,"include idle time"},
      {0,"include nice time"},
      {"cpu","select processor cpu [0-n] (default = all CPUs)"},
      {0,"print no header"},
      {0,"include system time"},
      {0,"include interrupt time"},
      {0,"include user time"},
      {"wait","pause wait seconds between updates (default = 1)"},
      {0,"report percentage not in each state"}
    };

    GETSYSCTL("hw.ncpu", ncpu);

    if (ncpu > sizeof(long)*8)
      ncpu = sizeof(long)*8;	/* We're using a bit per CPU */

    while ((c = getopt_long(ac, av, "ac:hinp:qsuw:x", options, &option_index)) >= 0)
    {
        switch (c)
        {
	    case 'a':
	        stats = STAT_ALL;
		break;

	    case 'c':
		count = strtol(optarg, NULL, 10);
	        break;

            case 'h':
	    {
	        int n, nopts = (sizeof(options) / sizeof(struct option));
		printf("usage: pcpustat [-");
		for (n = 0; n < nopts; n++)
		  if (!(help[n].argname))
		    printf("%c", options[n].val);
		printf("]");
		for (n = 0; n < nopts; n++)
		  if (help[n].argname)
		    printf(" [-%c %s]", options[n].val, help[n].argname);
		printf("\n");
		for (n = 0; n < nopts; n++)
		    printf("  -%c, --%-10s\t%s\n", options[n].val, options[n].name,
		    	help[n].description);
	    	exit(1);
	    }
	    break;

	    case 'i':
	        stats |= STAT_IDLE;
		break;

	    case 'n':
		stats |= STAT_NICE;
	    	break;

	    case 'p':
	    {
	        cpu = strtol(optarg, NULL, 10);
		if ((cpu < 0) || (cpu > (ncpu-1)))
		{
		    fprintf(stderr, "error: -p specifies invalid CPU number, range 0-%d\n", ncpu-1);
		    exit(1);
		}
		cpus |= 1 << cpu;
	    }
	    break;

	    case 'q':
	        quiet = 1;
		break;

	    case 's':
	    	stats |= STAT_SYSTEM;
		break;

	    case 't':
	        stats |= STAT_INT;
		break;

	    case 'u':
		stats |= STAT_USER;
	    	break;

	    case 'w':
		wait = strtol(optarg, NULL, 10);
	    	break;

	    case 'x':
	        not = 1;
		break;
        }
    }

    if (!stats)
        stats = STAT_ALL;

    if (!cpus)
        cpus = -1;	/* All CPUs */

    if (wait < 1)
    {
	if (count < 1)
            count = 1;
	wait = 1;
    }

    if (!quiet)
    {
	char head[21], fmt[5];
	int i;

	sprintf(head, "%s%s%s%s%s",
	    (stats & STAT_USER) ? "  us" : "",
	    (stats & STAT_NICE) ? "  ni" : "",
	    (stats & STAT_SYSTEM) ? "  sy" : "",
	    (stats & STAT_INT) ? "  in" : "",
	    (stats & STAT_IDLE) ? "  id" : "");
	if (not)
	  for (i=1; i<strlen(head); i+=4)
	    head[i] = '!';
        sprintf(fmt, "%%%ds", (int)strlen(head));

	for (cpu = 0; cpu < ncpu; cpu++)
	  if (cpus & (1 << cpu))
	  {
	    char str[8];
	    if (strlen(head) > 5)
	      sprintf(str, "cpu %d", cpu);
	    else
	      sprintf(str, "%d", cpu);
	    printf(fmt, str);
	  }
	printf("\n");
	for (cpu = 0; cpu < ncpu; cpu++)
	  if (cpus & (1 << cpu))
	  {
	    printf(head);
	  }
	printf("\n");
    }

    GETSYSCTL("kern.smp.maxcpus", maxcpu);
    state_size = CPUSTATES * maxcpu * sizeof(long);
    cpu_prev = malloc(state_size);
    cpu_curr = malloc(state_size);
    getsysctl("kern.cp_times", cpu_prev, state_size);

    for (;count; count--)	/* Negative = forever */
    {
	long *prev, *curr;

	if (wait > 0)
            sleep(wait);
	getsysctl("kern.cp_times", cpu_curr, state_size);

	for (cpu=0, prev=cpu_prev, curr=cpu_curr;
	     cpu < ncpu;
	     cpu++, prev += CPUSTATES, curr += CPUSTATES)
	  if (cpus & (1 << cpu))
	  {
	    long diff[CPUSTATES], n, h, total=0;

	    for (n=0; n < CPUSTATES; n++)
	    {
	        diff[n] = curr[n] - prev[n];
		total += diff[n];
	    }
	    h = total / 2;
	    for (n=0; n < CPUSTATES; n++)
	      if (stats & (1 << n))
	      {
	        int pct = (int)((diff[n] * 100 + h) / total);
		if (not)
		  pct = 100 - pct;
	        printf(" %3d", pct);
	      }
	  }
	printf("\n");
	fflush(stdout);

	memmove(cpu_prev, cpu_curr, state_size);
    }

    free(cpu_prev);
    free(cpu_curr);
    return(0);
}

/* Copied from /usr/src/usr.bin/top/machine.c */

static void
getsysctl(const char *name, void *ptr, size_t len)
{
	size_t nlen = len;

	if (sysctlbyname(name, ptr, &nlen, NULL, 0) == -1) {
		fprintf(stderr, "pcpustat: sysctl(%s...) failed: %s\n", name,
		    sys_errlist[errno]);
		exit(23);
	}
	if (nlen != len) {
		fprintf(stderr, "pcpustat: sysctl(%s...) expected %lu, got %lu\n",
		    name, (unsigned long)len, (unsigned long)nlen);
		exit(23);
	}
}
