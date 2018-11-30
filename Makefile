src = $(wildcard src/*.c)
obj = $(src:.c=.o)
dep = $(obj:.o=.d)
bin = erebus

CFLAGS = -pedantic -Wall -O3 -g -ffast-math -pthread
LDFLAGS = -lGL -lglut -lm -lpthread

$(bin): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

-include $(dep)

%.d: %.c
	@echo "gen depfile $< -> $@"
	@$(CPP) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@ 

.PHONY: clean
clean:
	rm -f $(obj) $(bin)

.PHONY: cleandep
cleandep:
	rm -f $(dep)
