/* ctest.h - tiny gtest-style C unit-test framework ("ctest"/"utest").
 *
 * NOTE: this is TEST CODE, not the graded ft_select program. It may use any
 * libc it likes. The registry uses libft's t_vec (so the framework stays
 * dependency-light and reuses what already exists). All reporting goes through
 * raw write(2) via cw_* helpers - never libft printf - because (a) libft's
 * ft_fdprintf lacks %ld/%zu and buffers, and (b) the runner forks per test, so
 * any buffered stdout would be duplicated by the child. write(2) is unbuffered
 * and fork-safe.
 *
 * Authoring:
 *   TEST(suite, name) { ASSERT_EQ(2, 1 + 1); EXPECT_TRUE(cond); }
 *   - fatal  ASSERT_* : on failure, longjmp out of the test body (teardown).
 *   - soft   EXPECT_* : on failure, record + keep running.
 *   - SUBCASE(fmt,...): label one generated iteration; bumps the DISTINCT-case
 *                       counter so a loop of N iterations counts as N cases.
 *
 * Entry:  int ctest_main(int argc, char **argv);
 *   flags: --no-fork  --filter=GLOB  --list  --seed=N
 */
#ifndef CTEST_H
# define CTEST_H

# include <setjmp.h>
# include <stddef.h>

/* ---- registered test record -------------------------------------------- */
typedef void	(*t_ctest_fn)(void);

typedef struct s_ctest
{
	const char	*suite;
	const char	*name;
	t_ctest_fn	fn;
}	t_ctest;

/* ---- per-test live state (file-scope singleton inside ctest.c) ---------- */
typedef struct s_ctest_ctx
{
	jmp_buf		jump;
	const char	*suite;
	const char	*name;
	int			failed;
	long		checks;
	long		cases;
	int			has_subcase;
}	t_ctest_ctx;

/* the framework owns one of these; macros below reach it through accessors. */
t_ctest_ctx	*ctest_cur(void);
void		ctest_register(const char *suite, const char *name, t_ctest_fn fn);
int			ctest_main(int argc, char **argv);

/* low-level reporting + failure plumbing used by the macros */
void	ctest_fail_begin(const char *file, int line);
void	cw_str(const char *s);
void	cw_long(long v);
void	cw_quoted(const char *s);
void	ctest_record_check(void);
void	ctest_subcase(const char *fmt, ...);
void	ctest_abort_test(void);

/* ---- TEST(): define + auto-register via a constructor ------------------- */
# define CT_REG(s, n) ct_reg_##s##_##n
# define CT_FN(s, n)  ct_fn_##s##_##n

# define TEST(s, n) \
	static void CT_FN(s, n)(void); \
	__attribute__((constructor)) static void CT_REG(s, n)(void) \
	{ \
		ctest_register(#s, #n, &CT_FN(s, n)); \
	} \
	static void CT_FN(s, n)(void)

/* SUBCASE: label a generated iteration; counts as one distinct case. */
# define SUBCASE(...) ctest_subcase(__VA_ARGS__)

/* ---- failure emission helpers ------------------------------------------ */
/* soft failure: record + continue. */
# define CT_SOFT(file, line) \
	do { \
		ctest_fail_begin((file), (line)); \
	} while (0)

/* fatal failure: record then longjmp out of the test body. */
# define CT_FATAL(file, line) \
	do { \
		ctest_fail_begin((file), (line)); \
		ctest_abort_test(); \
	} while (0)

/* ---- generic predicate driver ------------------------------------------ */
/* EMIT is CT_SOFT (EXPECT) or CT_FATAL (ASSERT). */
# define CT_CHECK(EMIT, ok, msg) \
	do { \
		ctest_record_check(); \
		if (!(ok)) \
		{ \
			EMIT(__FILE__, __LINE__); \
			cw_str("  " msg "\n"); \
		} \
	} while (0)

/* boolean */
# define CT_BOOL(EMIT, expr, want) \
	do { \
		ctest_record_check(); \
		if (((expr) ? 1 : 0) != (want)) \
		{ \
			EMIT(__FILE__, __LINE__); \
			cw_str("  " #expr " expected "); \
			cw_str((want) ? "true" : "false"); \
			cw_str("\n"); \
		} \
	} while (0)

/* long comparison: OP is ==, !=, <, <=, >, >= */
# define CT_CMP(EMIT, a, b, OP) \
	do { \
		long ct_a_ = (long)(a); \
		long ct_b_ = (long)(b); \
		ctest_record_check(); \
		if (!(ct_a_ OP ct_b_)) \
		{ \
			EMIT(__FILE__, __LINE__); \
			cw_str("  " #a " " #OP " " #b " : got "); \
			cw_long(ct_a_); \
			cw_str(" vs "); \
			cw_long(ct_b_); \
			cw_str("\n"); \
		} \
	} while (0)

/* string compare via wrapper (handles NULL). want=1 equal, want=0 differ. */
int		ctest_streq(const char *a, const char *b);

# define CT_STR(EMIT, a, b, want) \
	do { \
		const char *ct_sa_ = (a); \
		const char *ct_sb_ = (b); \
		ctest_record_check(); \
		if (ctest_streq(ct_sa_, ct_sb_) != (want)) \
		{ \
			EMIT(__FILE__, __LINE__); \
			cw_str("  strings: "); \
			cw_quoted(ct_sa_); \
			cw_str((want) ? " != " : " == "); \
			cw_quoted(ct_sb_); \
			cw_str("\n"); \
		} \
	} while (0)

int		ctest_memeq(const void *a, const void *b, size_t n);

# define CT_MEM(EMIT, a, b, n) \
	do { \
		ctest_record_check(); \
		if (!ctest_memeq((a), (b), (n))) \
		{ \
			EMIT(__FILE__, __LINE__); \
			cw_str("  memcmp " #a " vs " #b " over "); \
			cw_long((long)(n)); \
			cw_str(" bytes differ\n"); \
		} \
	} while (0)

/* ---- public soft (EXPECT_*) macros ------------------------------------- */
# define EXPECT_TRUE(e)      CT_BOOL(CT_SOFT, (e), 1)
# define EXPECT_FALSE(e)     CT_BOOL(CT_SOFT, (e), 0)
# define EXPECT_EQ(a, b)     CT_CMP(CT_SOFT, (a), (b), ==)
# define EXPECT_NE(a, b)     CT_CMP(CT_SOFT, (a), (b), !=)
# define EXPECT_LT(a, b)     CT_CMP(CT_SOFT, (a), (b), <)
# define EXPECT_LE(a, b)     CT_CMP(CT_SOFT, (a), (b), <=)
# define EXPECT_GT(a, b)     CT_CMP(CT_SOFT, (a), (b), >)
# define EXPECT_GE(a, b)     CT_CMP(CT_SOFT, (a), (b), >=)
# define EXPECT_STREQ(a, b)  CT_STR(CT_SOFT, (a), (b), 1)
# define EXPECT_STRNE(a, b)  CT_STR(CT_SOFT, (a), (b), 0)
# define EXPECT_MEMEQ(a, b, n) CT_MEM(CT_SOFT, (a), (b), (n))
# define EXPECT_NULL(p)      CT_BOOL(CT_SOFT, ((p) == NULL), 1)
# define EXPECT_NOTNULL(p)   CT_BOOL(CT_SOFT, ((p) != NULL), 1)

/* ---- public fatal (ASSERT_*) macros ------------------------------------ */
# define ASSERT_TRUE(e)      CT_BOOL(CT_FATAL, (e), 1)
# define ASSERT_FALSE(e)     CT_BOOL(CT_FATAL, (e), 0)
# define ASSERT_EQ(a, b)     CT_CMP(CT_FATAL, (a), (b), ==)
# define ASSERT_NE(a, b)     CT_CMP(CT_FATAL, (a), (b), !=)
# define ASSERT_LT(a, b)     CT_CMP(CT_FATAL, (a), (b), <)
# define ASSERT_LE(a, b)     CT_CMP(CT_FATAL, (a), (b), <=)
# define ASSERT_GT(a, b)     CT_CMP(CT_FATAL, (a), (b), >)
# define ASSERT_GE(a, b)     CT_CMP(CT_FATAL, (a), (b), >=)
# define ASSERT_STREQ(a, b)  CT_STR(CT_FATAL, (a), (b), 1)
# define ASSERT_STRNE(a, b)  CT_STR(CT_FATAL, (a), (b), 0)
# define ASSERT_MEMEQ(a, b, n) CT_MEM(CT_FATAL, (a), (b), (n))
# define ASSERT_NULL(p)      CT_BOOL(CT_FATAL, ((p) == NULL), 1)
# define ASSERT_NOTNULL(p)   CT_BOOL(CT_FATAL, ((p) != NULL), 1)

#endif
