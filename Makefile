CC ?= cc

all:	pcpustat.c
	$(CC) -o pcpustat pcpustat.c
