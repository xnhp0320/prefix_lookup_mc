CC=gcc
#CPPFLAGS=-Wall -g  -DTEST_CHECK -msse4.2
CPPFLAGS=-Wall -g  -O3 -msse4.2

SOURCE=$(wildcard *.c)
SOURCE_DEP=$(subst .c,.o,$(SOURCE))
SOURCE_DEP_D=$(subst .c,.d,$(SOURCE))

LDFLAGS= -lrt
OBJ=libfib.a

%.d: %.c
	$(CC) -M $(CPPFLAGS) $< > $@.$$$$;               \
	sed 's,\($*\)\.o[ :]*,\1.o $@: ,g' $@.$$$$ > $@; \
	rm -f $@.$$$$

all:
-include $(SOURCE_DEP_D)

.PHONY:all

all:$(OBJ) 


$(OBJ):$(SOURCE_DEP)
	$(AR) rvs $@ $^  
	#$(CC) $^ -o $@ $(LDFLAGS)


.PHONY: clean install uninstall 
clean:
	rm -f *.d
	rm -f *.o 
	rm -f $(OBJ) 


	
