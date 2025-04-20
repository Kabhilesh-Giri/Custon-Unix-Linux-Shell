#Define Compiler
CC = gcc

#Define Flags
CFlags = -fdiagnostics-color=always -Wall -Wpedantic -Wextra -g

File = Shell.c

clean:
	rm -rf output
	rm -rf output.txt

run : output
	./output

output: ${File}
	clear
	rm -rf output
	@echo "Compiling ${File}"
	${CC} ${CFlags} ${File} -o output
