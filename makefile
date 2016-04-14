OBJS = shell.cpp builtins.cpp
NAME = /home/ocket8888/bin/myshell

all: myshell

myshell: $(OBJS)
	g++ $(OBJS) -l readline -o $(NAME) -Wall

clean:
	rm -rf $(NAME)
