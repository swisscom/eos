CFLAGS = -Wall -O2 -g -Werror -Wall -Wextra -pedantic -std=c99 -D_GNU_SOURCE
CFLAGS_LOCAL = -Wall -O2 -Werror -Wall -Wextra -pedantic -std=c99 -D_GNU_SOURCE -g -I. -I../..
LDFLAGS = -lrt
OBJ = dvb_print_si dvb_gen_si dvb_ecmg dvb_ecmg_test mpeg_print_pcr rtp_check_seqnum mpeg_restamp header_compile_test

ifeq "$(shell uname -s)" "Darwin"
LDFLAGS += -liconv
# Prevent generation of .dSYM debug directories
CFLAGS += -gstabs
CFLAGS_LOCAL += -gstabs
endif

all: $(OBJ)

%: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

local: CFLAGS = $(CFLAGS_LOCAL)
local: all

clean:
	rm -rf $(OBJ)
