NAME      := ft_select
CC        := cc
CFLAGS    := -Wall -Wextra -Werror
LIBFT_DIR := vendor/libft
LIBFT     := $(LIBFT_DIR)/build/lib/libft.a
CPPFLAGS  := -Iinclude -I$(LIBFT_DIR)

# SAFE=1 builds libft with the libc allocator so valgrind can actually see and
# verify every allocation (real no-leak proof). Override with `make SAFE=` to
# use libft's in-house ft_malloc (mmap arena, invisible to Memcheck).
SAFE      ?= 1

SRCDIR    := src
OBJDIR    := build
SRCS      := $(wildcard $(SRCDIR)/*.c)
OBJS      := $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
DEPS      := $(OBJS:.o=.d)

# pure-logic objects shared with the unit-test binary (the test seam)
PURE      := args layout navigate select_ops output search color
PURE_OBJ  := $(addprefix $(OBJDIR)/,$(addsuffix .o,$(PURE)))

TESTDIR   := tests
FW         := $(TESTDIR)/framework/ctest.c $(TESTDIR)/framework/pty.c
TESTFLAGS := $(CPPFLAGS) -I$(TESTDIR)/framework

all: $(NAME)

$(LIBFT):
	$(MAKE) -C $(LIBFT_DIR) SAFE=$(SAFE)

$(NAME): $(OBJS) $(LIBFT)
	$(CC) $(CFLAGS) $(OBJS) $(LIBFT) -o $(NAME)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

# unit suite: ~1000 pure-logic cases under the µtest framework
test: $(LIBFT) $(PURE_OBJ)
	$(CC) $(CFLAGS) $(TESTFLAGS) $(TESTDIR)/unit_logic.c $(FW) $(PURE_OBJ) \
		$(LIBFT) -o $(OBJDIR)/run_tests
	$(OBJDIR)/run_tests

# framework self-test: proves the µtest framework itself (pass/fail/crash/to)
fwtest: $(LIBFT) | $(OBJDIR)
	$(CC) $(CFLAGS) $(TESTFLAGS) $(TESTDIR)/framework/selftest.c $(FW) \
		$(LIBFT) -o $(OBJDIR)/run_fwtest
	$(OBJDIR)/run_fwtest

# interactive end-to-end (forkpty) against the real binary
e2e: $(NAME)
	$(CC) $(CFLAGS) $(TESTFLAGS) $(TESTDIR)/e2e.c $(FW) $(LIBFT) -o $(OBJDIR)/run_e2e
	$(OBJDIR)/run_e2e

# valgrind leak matrix across contexts
leaks: $(NAME)
	bash $(TESTDIR)/leaks.sh

norm:
	norminette include src tests

clean:
	rm -rf $(OBJDIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

-include $(DEPS)

.PHONY: all test fwtest e2e leaks norm clean fclean re
