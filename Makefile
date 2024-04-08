LINK_TARGET = syn_flood


OBJS = \
	syn_flood.o


REBUILDABLES = $(OBJS) $(LINK_TARGET)


all : $(LINK_TARGET)


clean: 
	rm -f $(REBUILDABLES)


$(LINK_TARGET) : $(OBJS)


%.o : %.c
	gcc -g   -Wall -o $@ -c $< 
