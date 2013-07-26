BINS := fu
override CFLAGS += -std=c99
override LDFLAGS += -lpcre
ifneq "$(wildcard /opt/local/include)" ""
	override CFLAGS += -I/opt/local/include -L/opt/local/lib
endif
all: $(BINS)
%: %.c *.h Makefile
	$(CC) -O3 -o $@ $< $(CFLAGS) $(LDFLAGS)
install:
	cp -a $(BINS) /usr/local/bin/
