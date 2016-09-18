################################################################################
# Makefile                                                                     #
#                                                                              #
# Description: This file contains the make rules for Liso HTTP server          #
#                                                                              #
# Author: Xiaotong Sun (xiaotons@andrew.cmu.edu)                               #
#                                                                              #
################################################################################

define build-cmd
$(CC) $(CFLAGS) -c $< -o $@
endef

CC=gcc
CFLAGS=-Wall  -Werror -O2 -Wno-unused-function

OBJ_DIR=objs
SOURCE_DIR=src
PARSE_DIR=$(SOURCE_DIR)/parser
UTILITY_DIR=$(SOURCE_DIR)/utilities
CORE_DIR=$(SOURCE_DIR)/core
PARSER_DEPS = $(PARSE_DIR)/parse.h $(PARSE_DIR)/y.tab.h

OBJ=$(OBJ_DIR)/io.o
OBJ+=$(OBJ_DIR)/commons.o
OBJ+=$(OBJ_DIR)/handle_clients.o
OBJ+=$(OBJ_DIR)/handle_request.o
OBJ+=$(OBJ_DIR)/y.tab.o
OBJ+=$(OBJ_DIR)/lex.yy.o
OBJ+=$(OBJ_DIR)/parse.o
OBJ+=$(OBJ_DIR)/liso.o

all: pre lisod

lisod: $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

pre:
	mkdir -p $(OBJ_DIR)

$(PARSE_DIR)/lex.yy.c: $(PARSE_DIR)/lexer.l
	flex -o $@ $^

$(PARSE_DIR)/y.tab.c: $(PARSE_DIR)/parser.y
	yacc -d $^
	mv y.tab.* $(PARSE_DIR)/

$(OBJ_DIR)/commons.o: $(UTILITY_DIR)/commons.c $(UTILITY_DIR)/commons.h
	$(build-cmd)

$(OBJ_DIR)/io.o: $(UTILITY_DIR)/io.c $(UTILITY_DIR)/io.h
	$(build-cmd)

$(OBJ_DIR)/handle_clients.o: $(CORE_DIR)/handle_clients.c $(CORE_DIR)/handle_clients.h
	$(build-cmd)

$(OBJ_DIR)/handle_request.o: $(CORE_DIR)/handle_request.c $(CORE_DIR)/handle_request.h
	$(build-cmd)

$(OBJ_DIR)/%.o: $(PARSE_DIR)/%.c $(PARSER_DEPS)
	$(build-cmd)

$(OBJ_DIR)/lex.yy.o: $(PARSE_DIR)/lex.yy.c $(PARSER_DEPS)
	$(build-cmd)

$(OBJ_DIR)/y.tab.o: $(PARSE_DIR)/y.tab.c $(PARSER_DEPS)
	$(build-cmd)

$(OBJ_DIR)/liso.o: $(SOURCE_DIR)/liso.c $(SOURCE_DIR)/liso.h
	$(build-cmd)

clean:
	rm $(PARSE_DIR)/y.tab.* $(PARSE_DIR)/lex.yy.c
	rm $(OBJ_DIR)/*.o
	rm lisod

