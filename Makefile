################################################################################
# Makefile                                                                     #
#                                                                              #
# Description: This file contains the make rules for Recitation 1.             #
#                                                                              #
# Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                          #
#          Wolf Richter <wolf@cs.cmu.edu>                                      #
#                                                                              #
################################################################################

default: lisod liso_client

lisod:
	@gcc liso_server.c -o lisod -Wall -Werror

liso_client:
	@gcc liso_client.c -o liso_client -Wall -Werror

clean:
	@rm lisod liso_client
