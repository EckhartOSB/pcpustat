#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/resource.h>
#include <sys/sysctl.h>

#ifndef CPUSTATES
#define CPUSTATES	5	/* OSX doesn't define this */
#endif

static const char* what_string="@(#)pcpustat 1.5";

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
static size_t getsysctllen(const char *name);

struct opthelp {
    char *argname;
    char *description;
};

int main(int ac, char **av)
{
    int c, option_index, stats=0, count=-1, wait=0, cpu, ncpu, quiet=0, not=0;
    char *delim=NULL;
    size_t state_size;
    long cpus=0;
    long *cpu_prev, *cpu_curr;
    struct option options[] = {
      {"all", 0, 0, 'a'},
      {"count", 1, 0, 'c'},
      {"delim", 1, 0, 'd'},
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
      {"delimiter","separate columns with delimiter instead of justifying with spaces"},
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

    while ((c = getopt_long(ac, av, "ac:d:hinp:qstuw:x", options, &option_index)) >= 0)
    {
        switch (c)
        {
	    case 'a':
	        stats = STAT_ALL;
		break;

	    case 'c':
		count = strtol(optarg, NULL, 10);
	        break;

	    case 'd':
	    	delim = malloc(strlen(optarg)+1);
		strcpy(delim, optarg);
		break;

            case 'h':
	    {
	        int n, nopts = (sizeof(options) / sizeof(struct option));
		printf("%s\nusage: pcpustat [-", what_string+4);
		for (n = 0; n < nopts; n++)
		  if (!(help[n].argname))
		    printf("%c", options[n].val);
		fputs("]", stdout);
		for (n = 0; n < nopts; n++)
		  if (help[n].argname)
		    printf(" [-%c %s]", options[n].val, help[n].argname);
		fputs("\n", stdout);
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
	char *head, fmt[5];
	int i;

	if (delim)
	{
	    head = malloc((strlen(delim)+3)*5+1);
	    char *p = head;
	    int cnt = 0;
	    #define ADD_HEAD(s) { \
				    if (not) *p++ = '!';\
				    sprintf(p,"%s%s",s,delim);\
				    p+=strlen(p);\
				    cnt += 1;\
				  }
	    if (stats & STAT_USER)
	      ADD_HEAD("us");
	    if (stats & STAT_NICE)
	      ADD_HEAD("ni");
	    if (stats & STAT_SYSTEM)
	      ADD_HEAD("sy");
	    if (stats & STAT_INT)
	      ADD_HEAD("in");
	    if (stats & STAT_IDLE)
	      ADD_HEAD("id");
	    #undef ADD_HEAD
	    *(p-=strlen(delim)) = 0;		/* Overwrite last delimiter */
	    cnt--;
	    for (cpu = 0; cpu < ncpu; cpu++)
	    {
	      int dc;
	      if (cpu > 0)
	        fputs(delim, stdout);
	      printf("cpu %d", cpu);
	      for (dc=0; dc < cnt; dc++)
	        fputs(delim, stdout);
	    }
	    fputs("\n", stdout);
	}
	else
	{
	    head = malloc(21);
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
	    fputs("\n", stdout);
	}
	for (cpu = 0; cpu < ncpu; cpu++)
	  if (cpus & (1 << cpu))
	  {
	    if (delim && (cpu > 0))
	      fputs(delim, stdout);
	    fputs(head, stdout);
	  }
	fputs("\n", stdout);
	free(head);
    }

    state_size = getsysctllen("kern.cp_times");
    cpu_prev = malloc(state_size);
    cpu_curr = malloc(state_size);
    getsysctl("kern.cp_times", cpu_prev, state_size);

    for (;count; count--)	/* Negative = forever */
    {
	long *prev, *curr;
	int first = 1;

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
		if (delim)
		{
		    if (first)
		       first = 0;
		    else
		       fputs(delim, stdout);
		    printf("%d", pct);
		}
		else
		{
	            printf(" %3d", pct);
		}
	      }
	  }
	fputs("\n", stdout);
	fflush(stdout);

	memmove(cpu_prev, cpu_curr, state_size);
    }

    free(cpu_prev);
    free(cpu_curr);
    if (delim)
      free(delim);
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
}

static size_t
getsysctllen(const char *name)
{
	size_t len = 0;
	if (sysctlbyname(name, NULL, &len, NULL, 0) == -1) {
		fprintf(stderr, "pcpustat: sysctl(%s...) failed: %s\n", name,
		    sys_errlist[errno]);
		exit(23);
	}
	return len;
}
