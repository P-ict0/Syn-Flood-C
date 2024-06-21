LINK_TARGET = syn-flood


OBJS = \
	syn-flood.o


REBUILDABLES = $(OBJS) $(LINK_TARGET)


all : $(LINK_TARGET)


clean: 
	rm -f $(REBUILDABLES)


$(LINK_TARGET) : $(OBJS)


%.o : %.c
	gcc -g   -Wall -o $@ -c $< 
