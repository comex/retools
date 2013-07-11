BINS := fu
override CFLAGS += -std=c99
ifneq "$(wildcard /opt/local/include)" ""
	override CFLAGS += -I/opt/local/include -L/opt/local/lib
	override LDFLAGS += -lpcre
endif
all: $(BINS)
%: %.c *.h Makefile
	$(CC) -O3 -o $@ $< $(CFLAGS) $(LDFLAGS)
install:
	cp -a $(BINS) /usr/local/bin/
