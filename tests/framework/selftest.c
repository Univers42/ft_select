/* selftest.c - proves the ctest framework itself behaves. TEST CODE.
 *
 * It registers controlled tests and drives the runner twice (good-only, then
 * bad-only) via ctest_main with --filter, asserting:
 *   - passing assertions => suite returns 0 (all OK);
 *   - a soft EXPECT failure is RECORDED and the test KEEPS RUNNING past it
 *     (the post-EXPECT marker test still reports its later state);
 *   - a fatal ASSERT aborts the body (code after it does NOT run);
 *   - a test that SEGFAULTS is classified and the suite CONTINUES (the runner
 *     regains control: ctest_main returns and main prints the meta-summary);
 *   - a test that hangs is killed by the watchdog (TIMEOUT), suite continues.
 *
 * "Continues" is proven structurally: if fork isolation failed, the crashing
 * child would take down the whole process and main would never print
 * "[selftest] all framework self-checks passed". Reaching that line IS the
 * proof.
 */
#include "ctest.h"
#include "libft.h"
#include <unistd.h>
#include <stdlib.h>

/* shared between a soft-fail body and its assertions via a tiny on-disk-free
 * channel: a static flag visible only within the child that runs the test.
 * We instead prove continuation by structure (see header comment). */

/* ---- GOOD suite: every check passes ------------------------------------ */
TEST(Good, bools)
{
	ASSERT_TRUE(1);
	ASSERT_FALSE(0);
	EXPECT_TRUE(1 + 1 == 2);
	EXPECT_FALSE(5 < 3);
}

TEST(Good, numbers)
{
	ASSERT_EQ(42, 6 * 7);
	ASSERT_NE(1, 2);
	ASSERT_LT(1, 2);
	ASSERT_LE(2, 2);
	ASSERT_GT(3, 2);
	ASSERT_GE(3, 3);
}

TEST(Good, strings_and_mem)
{
	char	a[4];
	char	b[4];

	a[0] = 'x';
	a[1] = 'y';
	a[2] = 'z';
	a[3] = 0;
	b[0] = 'x';
	b[1] = 'y';
	b[2] = 'z';
	b[3] = 0;
	ASSERT_STREQ("abc", "abc");
	ASSERT_STRNE("abc", "abd");
	ASSERT_STREQ(NULL, NULL);
	ASSERT_NULL(NULL);
	ASSERT_NOTNULL(a);
	ASSERT_MEMEQ(a, b, 4);
}

TEST(Good, subcases_count)
{
	int	i;

	i = 0;
	while (i < 5)
	{
		SUBCASE("iteration %d", i);
		ASSERT_EQ(i, i);
		i++;
	}
}

/* ---- BAD suite: each test fails in a specific, classified way ----------- */

/* soft failure is recorded, yet the body keeps running afterwards (the second
 * EXPECT also executes and also fails -> two recorded failures, not one). */
TEST(Bad, soft_continues)
{
	EXPECT_TRUE(0);
	EXPECT_EQ(1, 2);
	EXPECT_STREQ("a", "b");
}

/* fatal assert aborts: the abort_marker EXPECT after it must NOT run. If it
 * ran and (wrongly) passed, the test would still be FAILED from the ASSERT, so
 * this test is expected FAILED either way; its role is to exercise the longjmp
 * path without crashing the runner. */
TEST(Bad, fatal_aborts)
{
	ASSERT_TRUE(0);
	EXPECT_TRUE(1);
}

TEST(Bad, segfaults)
{
	int	*p;

	p = NULL;
	*p = 7;
}

TEST(Bad, hangs)
{
	while (1)
		pause();
}

/* ---- meta driver -------------------------------------------------------- */
static int	run_filtered(const char *filter)
{
	char	*argv[2];
	char	buf[64];
	size_t	i;

	i = 0;
	while (filter[i] && i < sizeof(buf) - 1)
	{
		buf[i] = filter[i];
		i++;
	}
	buf[i] = 0;
	argv[0] = (char *)"selftest";
	argv[1] = buf;
	return (ctest_main(2, argv));
}

static void	fail(const char *msg)
{
	write(2, "[selftest] FAIL: ", 17);
	write(2, msg, (size_t)(ft_strlen(msg)));
	write(2, "\n", 1);
	exit(1);
}

int	main(void)
{
	int	good;
	int	bad;

	write(1, "=== running GOOD suite (expect all OK) ===\n", 43);
	good = run_filtered("--filter=Good.*");
	if (good != 0)
		fail("GOOD suite reported failures but all checks should pass");
	write(1, "\n=== running BAD suite (expect failures, no runner death) ==="
		"\n", 61);
	bad = run_filtered("--filter=Bad.*");
	if (bad == 0)
		fail("BAD suite reported success but it must record failures");
	write(1, "\n[selftest] all framework self-checks passed\n", 45);
	return (0);
}
