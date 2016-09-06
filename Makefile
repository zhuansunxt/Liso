################################################################################
# Makefile                                                                     #
#                                                                              #
# Description: This file contains the make rules for Liso HTTP server          #
#                                                                              #
# Author: Xiaotong Sun (xiaotons@andrew.cmu.edu)                               #
#                                                                              #
################################################################################

define build-cmd
$(CC) $(CFLAGS) $< -o $@
endef

CC=gcc
CFLAGS=-Wall  -Werror -O2
SOURCE=src
VPATH=$(SOURCE)
OBJECTS = liso.o
OBJECTS += server.o
OBJECTS += io.o
OBJECTS += utilities.o

default: lisod

lisod: $(OBJECTS)
	$(CC) $(CFLAGS) -o lisod $(OBJECTS)

$(SOURCE)/%.o: %.c
	$(build-cmd)

client: liso_client.c
	$(CC) -o  liso_client liso_client.c

clean:
	rm *.o
	rm lisod
