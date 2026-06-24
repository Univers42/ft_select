/* ctest.c - runner for the tiny C unit-test framework. See ctest.h.
 * TEST CODE (unrestricted libc). Reporting is raw write(2) only: unbuffered
 * and fork-safe (the runner forks per test).
 */
#include "ctest.h"
#include "libft.h"
#include <unistd.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>

/* ANSI colour (disabled when stdout is not a tty). */
#define C_GRN "\033[32m"
#define C_RED "\033[31m"
#define C_YEL "\033[33m"
#define C_DIM "\033[2m"
#define C_RST "\033[0m"

/* ------------------------------------------------------------------ state */
static t_vec		g_reg;
static int			g_reg_ready;
static t_ctest_ctx	g_ctx;
static int			g_color;
static volatile pid_t	g_child;

t_ctest_ctx	*ctest_cur(void)
{
	return (&g_ctx);
}

/* ----------------------------------------------------------- write output */
void	cw_str(const char *s)
{
	if (s)
		write(1, s, ft_strlen(s));
}

static void	cw_col(const char *code)
{
	if (g_color)
		cw_str(code);
}

/* write a signed long in base 10 without printf (libft printf lacks %ld). */
void	cw_long(long v)
{
	char			buf[24];
	int				i;
	unsigned long	u;

	i = (int)sizeof(buf);
	if (v < 0)
		u = (unsigned long)(-(v + 1)) + 1UL;
	else
		u = (unsigned long)v;
	buf[--i] = (char)('0' + (u % 10));
	u /= 10;
	while (u > 0 && i > 0)
	{
		buf[--i] = (char)('0' + (u % 10));
		u /= 10;
	}
	if (v < 0 && i > 0)
		buf[--i] = '-';
	write(1, buf + i, sizeof(buf) - (size_t)i);
}

void	cw_quoted(const char *s)
{
	if (!s)
	{
		cw_str("(null)");
		return ;
	}
	cw_str("\"");
	cw_str(s);
	cw_str("\"");
}

/* ------------------------------------------------------------- comparisons */
int	ctest_streq(const char *a, const char *b)
{
	if (a == b)
		return (1);
	if (!a || !b)
		return (0);
	return (ft_strcmp(a, b) == 0);
}

int	ctest_memeq(const void *a, const void *b, size_t n)
{
	if (a == b)
		return (1);
	if (!a || !b)
		return (0);
	return (memcmp(a, b, n) == 0);
}

/* --------------------------------------------------------------- registry */
static void	reg_ensure(void)
{
	if (g_reg_ready)
		return ;
	vec_init(&g_reg);
	g_reg.elem_size = sizeof(t_ctest);
	g_reg_ready = 1;
}

void	ctest_register(const char *suite, const char *name, t_ctest_fn fn)
{
	t_ctest	rec;

	reg_ensure();
	rec.suite = suite;
	rec.name = name;
	rec.fn = fn;
	vec_push(&g_reg, &rec);
}

/* --------------------------------------------------- per-check accounting */
void	ctest_record_check(void)
{
	g_ctx.checks++;
}

/* first failing check in a test prints the [FAILED] banner detail header. */
void	ctest_fail_begin(const char *file, int line)
{
	g_ctx.failed = 1;
	cw_col(C_RED);
	cw_str(file);
	cw_str(":");
	cw_long((long)line);
	cw_str(": failure");
	cw_col(C_RST);
	cw_str("\n");
}

void	ctest_abort_test(void)
{
	longjmp(g_ctx.jump, 1);
}

/* SUBCASE: count a distinct generated case and (dimly) label it. */
void	ctest_subcase(const char *fmt, ...)
{
	va_list	ap;
	char	buf[256];
	int		n;

	g_ctx.cases++;
	g_ctx.has_subcase = 1;
	va_start(ap, fmt);
	n = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (n < 0)
		return ;
	cw_col(C_DIM);
	cw_str("    - case: ");
	cw_str(buf);
	cw_col(C_RST);
	cw_str("\n");
}

/* ------------------------------------------------------------- glob match */
/* minimal '*' and '?' glob (suite.name vs pattern). */
static int	glob_match(const char *pat, const char *s)
{
	if (*pat == '\0')
		return (*s == '\0');
	if (*pat == '*')
		return (glob_match(pat + 1, s) || (*s && glob_match(pat, s + 1)));
	if (*pat == '?')
		return (*s && glob_match(pat + 1, s + 1));
	if (*pat == *s)
		return (glob_match(pat + 1, s + 1));
	return (0);
}

static int	test_selected(const char *filter, t_ctest *t)
{
	char	full[256];

	if (!filter)
		return (1);
	(void)snprintf(full, sizeof(full), "%s.%s", t->suite, t->name);
	return (glob_match(filter, full));
}

/* ---------------------------------------------------------- run one body */
/* runs the test fn under setjmp; returns 1 = pass, 0 = fail. resets ctx. */
static int	run_body(t_ctest *t)
{
	g_ctx.suite = t->suite;
	g_ctx.name = t->name;
	g_ctx.failed = 0;
	g_ctx.checks = 0;
	g_ctx.cases = 0;
	g_ctx.has_subcase = 0;
	if (setjmp(g_ctx.jump) == 0)
		t->fn();
	return (g_ctx.failed == 0);
}

/* ---- fork mode: child reports {failed, cases} back over a pipe --------- */
typedef struct s_kid
{
	int	failed;
	long	cases;
}	t_kid;

static volatile sig_atomic_t	g_timed_out;

static void	on_alarm(int sig)
{
	(void)sig;
	g_timed_out = 1;
	if (g_child > 0)
		kill(g_child, SIGKILL);
}

static void	child_run(t_ctest *t, int wfd)
{
	t_kid	k;

	memset(&k, 0, sizeof(k));
	run_body(t);
	k.failed = g_ctx.failed;
	k.cases = g_ctx.cases;
	(void)!write(wfd, &k, sizeof(k));
	close(wfd);
	_exit(g_ctx.failed ? 1 : 0);
}

/* classify a finished/killed child. returns: 0 pass, 1 fail, 2 crash,
 * 3 timeout. fills *cases from the pipe payload (0 if none arrived). */
static int	classify(int status, int got, t_kid *k, int timed_out)
{
	if (timed_out)
		return (3);
	if (WIFSIGNALED(status))
		return (2);
	if (got == (int)sizeof(*k) && k->failed == 0
		&& WIFEXITED(status) && WEXITSTATUS(status) == 0)
		return (0);
	return (1);
}

/* parent: wait the child with an alarm watchdog; report timeout via the
 * SIGALRM handler that SIGKILLs it. returns the raw wait status, sets
 * *timed_out. (3s cap per test - generous for unit logic, fatal for hangs.) */
static int	parent_wait(pid_t pid, int *timed_out)
{
	int	status;
	int	r;

	g_timed_out = 0;
	g_child = pid;
	signal(SIGALRM, on_alarm);
	alarm(3);
	r = waitpid(pid, &status, 0);
	while (r < 0)
		r = waitpid(pid, &status, 0);
	alarm(0);
	signal(SIGALRM, SIG_DFL);
	g_child = 0;
	*timed_out = (g_timed_out != 0);
	return (status);
}

/* run one test in a forked child; return 0 pass / 1 fail / 2 crash / 3 to. */
static int	run_forked(t_ctest *t, long *cases_out)
{
	int		fds[2];
	pid_t	pid;
	t_kid	k;
	int		st[2];
	int		to;

	k.failed = 1;
	k.cases = 0;
	if (pipe(fds) < 0)
		return (2);
	pid = fork();
	if (pid < 0)
		return (close(fds[0]), close(fds[1]), 2);
	if (pid == 0)
	{
		close(fds[0]);
		child_run(t, fds[1]);
	}
	close(fds[1]);
	st[0] = parent_wait(pid, &to);
	st[1] = (read(fds[0], &k, sizeof(k)) == (ssize_t)sizeof(k));
	close(fds[0]);
	*cases_out = k.cases;
	return (classify(st[0], st[1] ? (int)sizeof(k) : 0, &k, to));
}

/* ----------------------------------------------------------- result print */
static const char	*verdict_str(int v)
{
	if (v == 0)
		return ("OK");
	if (v == 2)
		return ("CRASH");
	if (v == 3)
		return ("TIMEOUT");
	return ("FAILED");
}

static void	print_run(t_ctest *t)
{
	cw_col(C_GRN);
	cw_str("[ RUN    ] ");
	cw_col(C_RST);
	cw_str(t->suite);
	cw_str(".");
	cw_str(t->name);
	cw_str("\n");
}

static void	print_verdict(t_ctest *t, int v)
{
	if (v == 0)
		cw_col(C_GRN);
	else
		cw_col(C_RED);
	if (v == 0)
		cw_str("[     OK ] ");
	else
		cw_str("[ FAILED ] ");
	cw_col(C_RST);
	cw_str(t->suite);
	cw_str(".");
	cw_str(t->name);
	if (v != 0 && v != 1)
	{
		cw_str(" (");
		cw_str(verdict_str(v));
		cw_str(")");
	}
	cw_str("\n");
}

/* --------------------------------------------------------------- options */
typedef struct s_opt
{
	int			no_fork;
	int			list;
	const char	*filter;
	long		seed;
}	t_opt;

static void	parse_opts(int argc, char **argv, t_opt *o)
{
	int	i;

	o->no_fork = 0;
	o->list = 0;
	o->filter = NULL;
	o->seed = 0;
	i = 1;
	while (i < argc)
	{
		if (!strcmp(argv[i], "--no-fork"))
			o->no_fork = 1;
		else if (!strcmp(argv[i], "--list"))
			o->list = 1;
		else if (!strncmp(argv[i], "--filter=", 9))
			o->filter = argv[i] + 9;
		else if (!strncmp(argv[i], "--seed=", 7))
			o->seed = ft_atoi(argv[i] + 7);
		i++;
	}
}

/* --list: print each selected test's full name + total distinct count.
 * (a TEST with no SUBCASEs counts as 1; SUBCASE counts are dynamic, so the
 * static list reports the test count as a lower bound.) */
static int	do_list(t_opt *o)
{
	size_t	i;
	t_ctest	*t;
	long	n;

	n = 0;
	i = 0;
	while (i < g_reg.len)
	{
		t = (t_ctest *)vec_idx(&g_reg, i);
		if (test_selected(o->filter, t))
		{
			cw_str(t->suite);
			cw_str(".");
			cw_str(t->name);
			cw_str("\n");
			n++;
		}
		i++;
	}
	cw_str("tests: ");
	cw_long(n);
	cw_str("\n");
	return (0);
}

/* running tally across the whole suite. */
typedef struct s_tally
{
	long	run;
	long	pass;
	long	fail;
	long	cases;
}	t_tally;

static void	tally_one(t_tally *ta, t_ctest *t, t_opt *o)
{
	int		v;
	long	cases;

	print_run(t);
	cases = 0;
	if (o->no_fork)
	{
		v = run_body(t) ? 0 : 1;
		cases = g_ctx.cases;
	}
	else
		v = run_forked(t, &cases);
	print_verdict(t, v);
	ta->run++;
	if (v == 0)
		ta->pass++;
	else
		ta->fail++;
	if (cases > 0)
		ta->cases += cases;
	else
		ta->cases += 1;
}

static void	print_summary(t_tally *ta)
{
	cw_str("\n");
	cw_col(C_GRN);
	cw_str("[========] ");
	cw_col(C_RST);
	cw_long(ta->run);
	cw_str(" tests run, ");
	cw_long(ta->cases);
	cw_str(" distinct cases.\n");
	cw_col(C_GRN);
	cw_str("[ PASSED ] ");
	cw_col(C_RST);
	cw_long(ta->pass);
	cw_str("\n");
	if (ta->fail > 0)
	{
		cw_col(C_RED);
		cw_str("[ FAILED ] ");
		cw_col(C_RST);
		cw_long(ta->fail);
		cw_str("\n");
	}
}

int	ctest_main(int argc, char **argv)
{
	t_opt	o;
	t_tally	ta;
	size_t	i;
	t_ctest	*t;

	reg_ensure();
	g_color = isatty(1);
	parse_opts(argc, argv, &o);
	(void)o.seed;
	if (o.list)
		return (do_list(&o));
	ta = (t_tally){0, 0, 0, 0};
	i = 0;
	while (i < g_reg.len)
	{
		t = (t_ctest *)vec_idx(&g_reg, i);
		if (test_selected(o.filter, t))
			tally_one(&ta, t, &o);
		i++;
	}
	print_summary(&ta);
	return (ta.fail > 0);
}
