.PHONY: all unittest clean

all : clean lua_compile unittest


unittest : unittest.o
	${CC} $^ -o $@  -llua5.3 -lsqlite3
	@echo "=== Test Result ==="
	./$@

unittest.o : unittest.c
	${CC} -c $< -o $@ -Wall -I/usr/local/include/lua5.3

lua_compile : filter.lua 
	luac  -o $<.out $<
	xxd -i $<.out $<.h

clean : 
	-rm -f *.o *.out *.lua.h unittest code.db

