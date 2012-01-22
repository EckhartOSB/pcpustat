CC ?= cc
CFLAGS?= -Wall -Wextra -std=c99 -pedantic

NAME=pcpustat

all:	${NAME}
pcpustat: ${NAME}.c
	${CC} ${CFLAGS} -o ${NAME} ${NAME}.c

clean:
	rm -f ${NAME}
