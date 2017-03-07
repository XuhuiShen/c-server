INCLDIRS = ./
CC = gcc
SRC += $(shell ls *.c)
OBJ += $(SRC:.c=.o)
CFLAGS += -O2 -Wall -g -c
LDFLAGS +=
CPPFLAGS += -I$(INCLDIRS)
OUTPUT = $(shell pwd)/a.out

$(OUTPUT): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

-include $(SRC:%.c=%.P)

%.o : %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MD -o $@ $<
	@cp $*.d $*.P
	@sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	     -e '/^$$/ d' -e 's/$$/ :/' < $*.d >> $*.P
	@rm -f $*.d

.PHONY: clean
clean:
	rm -rf $(OUTPUT) $(OBJ) *.P
