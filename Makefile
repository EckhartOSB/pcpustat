CC ?= cc

all:	pcpustat
pcpustat: pcpustat.c
	$(CC) -o pcpustat pcpustat.c
