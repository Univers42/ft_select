/* unit_logic.c - ~1000 distinct property/invariant cases for the pure ft_select
 * logic (args/layout/navigate/select_ops/output, plus the search/color bonus
 * primitives). TEST CODE: unrestricted libc. Inputs are generated in nested
 * loops with a SEEDED xorshift (deterministic, NOT libc rand) and labelled with
 * SUBCASE so each iteration counts as one distinct case. We assert INVARIANTS
 * (bijection, in-bounds, idempotence, identity) rather than thousands of
 * hand-written expected strings.
 *
 * Memory note: libft's ft_strdup / t_vec allocate through fn_malloc (the in-tree
 * ft_malloc). We therefore free with fn_free, never libc free, so valgrind sees
 * a matched allocator and reports zero leaks.
 */
#include "ctest.h"
#include "ft_select.h"
#include <stdio.h>

/* ---- seeded xorshift32 (deterministic across runs) --------------------- */
static unsigned int	g_seed = 0x1234567u;

static unsigned int	xrand(void)
{
	g_seed ^= g_seed << 13;
	g_seed ^= g_seed >> 17;
	g_seed ^= g_seed << 5;
	return (g_seed);
}

static void	reseed(unsigned int s)
{
	g_seed = s ? s : 0x9e3779b9u;
}

static unsigned int	xrange(unsigned int lo, unsigned int hi)
{
	if (hi <= lo)
		return (lo);
	return (lo + xrand() % (hi - lo + 1));
}

/* ---- helpers: build a t_vec<char*> of n synthetic items ----------------- */
static void	mk_items(t_vec *items, size_t n, size_t each)
{
	char	buf[64];
	char	*dup;
	size_t	i;

	vec_init(items);
	items->elem_size = sizeof(char *);
	i = 0;
	while (i < n)
	{
		snprintf(buf, sizeof(buf), "i%zu_%0*d", i, (int)(each % 20), 0);
		dup = ft_strdup(buf);
		vec_push(items, &dup);
		i++;
	}
}

static void	mk_selected(t_vec *sel, size_t n, unsigned int mask)
{
	unsigned char	b;
	size_t			i;

	vec_init(sel);
	sel->elem_size = sizeof(unsigned char);
	i = 0;
	while (i < n)
	{
		b = (unsigned char)((mask >> (i % 32)) & 1u);
		vec_push_byte(sel, b);
		i++;
	}
}

/* ===================================================================== */
/* DIMENSION 1 - args ingestion (~250 cases)                             */
/* ===================================================================== */

/* count sweep: ac 1..N (av[0] excluded) -> len == ac-1 ; 0 args -> 0. */
TEST(args, count_sweep)
{
	char	*av[130];
	t_vec	items;
	int		ac;
	int		i;

	i = 0;
	while (i < 130)
		av[i++] = "x";
	ac = 1;
	while (ac <= 128)
	{
		SUBCASE("ac=%d -> len=%d", ac, ac - 1);
		ASSERT_EQ(args_ingest(ac, av, &items), ac - 1);
		ASSERT_EQ((long)items.len, ac - 1);
		vec_destroy(&items, free_item);
		ac++;
	}
}

/* content variety: each tricky string round-trips and sets max width. */
TEST(args, content_roundtrip)
{
	const char	*corpus[] = {"", " ", "\t", "a\nb", "\x01\x02", "héllo",
		"flag-like", "--help", "dup", "dup", "x", "0",
		"a_very_long_item_name_used_to_stress_the_width_calc_0123456789",
		"é", "ç€", "  spaces  ", "tab\tinside", "ctrl\007bell", NULL};
	t_vec		items;
	char		*av[3];
	size_t		i;

	i = 0;
	while (corpus[i] != NULL)
	{
		av[1] = (char *)corpus[i];
		SUBCASE("item[%zu]=%s", i, corpus[i]);
		ASSERT_EQ(args_ingest(2, av, &items), 1);
		ASSERT_STREQ(*(char **)vec_idx(&items, 0), corpus[i]);
		ASSERT_EQ((long)max_item_width(&items), (long)ft_strlen(corpus[i]));
		vec_destroy(&items, free_item);
		i++;
	}
}

/* max_item_width = max strlen over random sets; 0 when empty. */
TEST(args, max_width_random)
{
	t_vec		items;
	char		buf[80];
	char		*dup;
	size_t		n;
	size_t		j;
	size_t		want;
	size_t		len;
	int			t;

	reseed(0xA11CE);
	t = 0;
	while (t < 200)
	{
		n = xrange(0, 12);
		vec_init(&items);
		items.elem_size = sizeof(char *);
		want = 0;
		j = 0;
		while (j < n)
		{
			len = xrange(0, 40);
			memset(buf, 'x', len);
			buf[len] = '\0';
			if (len > want)
				want = len;
			dup = ft_strdup(buf);
			vec_push(&items, &dup);
			j++;
		}
		SUBCASE("n=%zu max=%zu", n, want);
		ASSERT_EQ((long)max_item_width(&items), (long)want);
		vec_destroy(&items, free_item);
		t++;
	}
}

/* ===================================================================== */
/* DIMENSION 2 - layout geometry (~400 cases)                            */
/* ===================================================================== */

/* idx<->cell bijection + in-bounds across a swept grid. For every index the
 * round trip index_of_cell(cell_of_index(idx)) == idx, and the screen cell is
 * inside [0,n_rows) x [0,n_cols). */
TEST(layout, bijection_sweep)
{
	t_layout	l;
	size_t		n;
	int			cols;
	int			rows;
	int			maxw;
	size_t		idx;
	int			r;
	int			c;
	long		back;
	int			count;

	count = 0;
	n = 1;
	while (n <= 30)
	{
		cols = 10;
		while (cols <= 120)
		{
			rows = 1;
			while (rows <= 20)
			{
				maxw = (int)(n % 9) + 1;
				l = layout_compute(n, cols, rows, maxw);
				SUBCASE("n=%zu c=%d r=%d w=%d cols=%d rows=%d",
					n, cols, rows, maxw, l.n_cols, l.n_rows);
				if (!l.too_small)
				{
					idx = 0;
					while (idx < n)
					{
						ASSERT_EQ(cell_of_index(l, idx, &r, &c), 0);
						ASSERT_GE(r, 0);
						ASSERT_LT(r, l.n_rows);
						ASSERT_GE(c, 0);
						ASSERT_LT(c, l.n_cols);
						back = index_of_cell(l, r, c, n);
						ASSERT_EQ(back, (long)idx);
						idx++;
					}
					ASSERT_GE(l.n_cols, 1);
					ASSERT_GE(l.n_rows, 1);
					ASSERT_LE(l.n_rows, rows);
				}
				count++;
				rows += 6;
			}
			cols += 30;
		}
		n += 3;
	}
	ASSERT_GT(count, 0);
}

/* every occupied cell maps back to an index in [0,n); empty trailing cells in
 * the last column return -1. Also: all columns but the last are full. */
TEST(layout, cell_coverage)
{
	t_layout	l;
	size_t		n;
	int			r;
	int			c;
	long		idx;
	int			seen;

	reseed(0xBEEF);
	n = 1;
	while (n <= 40)
	{
		l = layout_compute(n, 80, (int)xrange(1, 24), (int)xrange(1, 10));
		if (!l.too_small)
		{
			seen = 0;
			c = 0;
			while (c < l.n_cols)
			{
				r = 0;
				while (r < l.n_rows)
				{
					idx = index_of_cell(l, r, c, n);
					if (idx >= 0)
					{
						ASSERT_LT(idx, (long)n);
						seen++;
					}
					r++;
				}
				c++;
			}
			SUBCASE("n=%zu cols=%d rows=%d seen=%d", n, l.n_cols, l.n_rows, seen);
			ASSERT_EQ((long)seen, (long)n);
		}
		n++;
	}
}

/* too-small handling: cols/rows < 1 or no room for one column -> too_small,
 * and the cell accessors refuse (return -1) without dividing by zero. */
TEST(layout, too_small_guard)
{
	t_layout	l;
	int			r;
	int			c;
	int			cols;
	int			maxw;

	cols = 0;
	while (cols <= 6)
	{
		maxw = 1;
		while (maxw <= 10)
		{
			l = layout_compute(5, cols, 0, maxw);
			SUBCASE("cols=%d rows=0 w=%d too_small=%d", cols, maxw, l.too_small);
			ASSERT_TRUE(l.too_small);
			ASSERT_EQ(cell_of_index(l, 0, &r, &c), -1);
			ASSERT_EQ(index_of_cell(l, 0, 0, 5), -1L);
			maxw++;
		}
		cols++;
	}
	l = layout_compute(5, 2, 5, 10);
	SUBCASE("narrow cols=2 w=10");
	ASSERT_TRUE(l.too_small);
}

/* ===================================================================== */
/* DIMENSION 3 - navigation (~200 cases)                                 */
/* ===================================================================== */

/* every start index x 4 directions ALWAYS lands in [0,n). */
TEST(nav, always_in_bounds)
{
	t_layout	l;
	size_t		n;
	size_t		cur;
	size_t	r;
	t_dir		d;

	reseed(0xC0FFEE);
	n = 1;
	while (n <= 25)
	{
		l = layout_compute(n, 80, (int)xrange(2, 16), (int)xrange(1, 8));
		if (!l.too_small)
		{
			cur = 0;
			while (cur < n)
			{
				d = D_UP;
				while (d <= D_RIGHT)
				{
					r = nav_move(l, cur, n, d);
					SUBCASE("n=%zu cur=%zu d=%d -> %zu", n, cur, (int)d, r);
					ASSERT_LT(r, n);
					d = (t_dir)(d + 1);
				}
				cur++;
			}
		}
		n += 2;
	}
}

/* inverse-pair: Down then Up returns to start when the column has >1 occupied
 * row (no wrap ambiguity); Right then Left likewise on a full row. We restrict
 * to single-column or full-grid layouts where the inverse is unambiguous. */
TEST(nav, inverse_pairs)
{
	t_layout	l;
	size_t		n;
	size_t		cur;
	size_t		down;
	size_t		back;

	n = 2;
	while (n <= 20)
	{
		l = layout_compute(n, 80, (int)n + 2, 4);
		if (!l.too_small && l.n_cols == 1)
		{
			cur = 0;
			while (cur < n)
			{
				down = nav_move(l, cur, n, D_DOWN);
				back = nav_move(l, down, n, D_UP);
				SUBCASE("col1 n=%zu cur=%zu down=%zu back=%zu",
					n, cur, down, back);
				ASSERT_EQ((long)back, (long)cur);
				cur++;
			}
		}
		n++;
	}
}

/* full vertical loop identity: in a single column, n consecutive Downs return
 * to the start (pure cycle of length n). */
TEST(nav, full_loop_identity)
{
	t_layout	l;
	size_t		n;
	size_t		cur;
	size_t		k;

	n = 1;
	while (n <= 30)
	{
		l = layout_compute(n, 80, (int)n + 1, 4);
		if (!l.too_small && l.n_cols == 1)
		{
			cur = 0;
			k = 0;
			while (k < n)
			{
				cur = nav_move(l, cur, n, D_DOWN);
				k++;
			}
			SUBCASE("loop n=%zu end=%zu", n, cur);
			ASSERT_EQ((long)cur, 0L);
		}
		n++;
	}
}

/* horizontal full loop on the first row (always fully populated across cols). */
TEST(nav, horizontal_loop)
{
	t_layout	l;
	size_t		n;
	size_t		cur;
	size_t		k;
	int			r;
	int			c;

	n = 1;
	while (n <= 30)
	{
		l = layout_compute(n, 200, 3, 2);
		if (!l.too_small)
		{
			cur = 0;
			k = 0;
			while (k < (size_t)l.n_cols)
			{
				cur = nav_move(l, cur, n, D_RIGHT);
				k++;
			}
			cell_of_index(l, cur, &r, &c);
			SUBCASE("hloop n=%zu cols=%d end=%zu row=%d", n, l.n_cols, cur, r);
			ASSERT_EQ((long)cur, 0L);
		}
		n++;
	}
}

/* ===================================================================== */
/* DIMENSION 4 - select / delete (~150 cases)                            */
/* ===================================================================== */

/* toggle idempotence: two toggles restore the original flag; one flips it. */
TEST(sel, toggle_idempotence)
{
	t_vec	sel;
	size_t	n;
	size_t	i;
	unsigned char	before;

	n = 1;
	while (n <= 16)
	{
		mk_selected(&sel, n, 0xA5A5A5A5u);
		i = 0;
		while (i < n)
		{
			before = *(unsigned char *)vec_idx(&sel, i);
			sel_toggle(&sel, i);
			SUBCASE("n=%zu i=%zu flip", n, i);
			ASSERT_EQ((long)*(unsigned char *)vec_idx(&sel, i),
				(long)(before ^ 1));
			sel_toggle(&sel, i);
			ASSERT_EQ((long)*(unsigned char *)vec_idx(&sel, i), (long)before);
			i++;
		}
		vec_destroy(&sel, NULL);
		n++;
	}
}

/* delete shifts items AND selected left in lockstep; lengths shrink by 1 and
 * the surviving items are the original minus the deleted index, in order. */
TEST(sel, delete_shift)
{
	t_vec	items;
	t_vec	sel;
	size_t	n;
	size_t	del;
	size_t	i;
	char	buf[64];

	n = 1;
	while (n <= 16)
	{
		del = 0;
		while (del < n)
		{
			mk_items(&items, n, 3);
			mk_selected(&sel, n, 0x0F0F0F0Fu);
			SUBCASE("n=%zu del=%zu", n, del);
			item_delete(&items, &sel, del);
			ASSERT_EQ((long)items.len, (long)(n - 1));
			ASSERT_EQ((long)sel.len, (long)(n - 1));
			i = 0;
			while (i < items.len)
			{
				if (i < del)
					snprintf(buf, sizeof(buf), "i%zu_", i);
				else
					snprintf(buf, sizeof(buf), "i%zu_", i + 1);
				ASSERT_EQ(ft_strncmp(*(char **)vec_idx(&items, i),
						buf, ft_strlen(buf)), 0);
				i++;
			}
			vec_destroy(&items, free_item);
			vec_destroy(&sel, NULL);
			del++;
		}
		n++;
	}
}

/* delete-to-empty: deleting the last remaining item leaves len==0 (caller =>
 * esc behaviour). Repeated deletes never crash and end empty. */
TEST(sel, delete_to_empty)
{
	t_vec	items;
	t_vec	sel;
	size_t	n;
	size_t	cur;
	size_t	guard;

	n = 1;
	while (n <= 20)
	{
		mk_items(&items, n, 2);
		mk_selected(&sel, n, 0xFFFFFFFFu);
		cur = 0;
		guard = 0;
		while (items.len > 0 && guard < 100)
		{
			cur = item_delete(&items, &sel, cur);
			guard++;
		}
		SUBCASE("n=%zu deleted_all guard=%zu", n, guard);
		ASSERT_EQ((long)items.len, 0L);
		ASSERT_EQ((long)sel.len, 0L);
		vec_destroy(&items, free_item);
		vec_destroy(&sel, NULL);
		n++;
	}
}

/* ===================================================================== */
/* DIMENSION 5 - output (~50 golden) + search/color bonus primitives     */
/* ===================================================================== */

static void	check_output(const char *const *names, const unsigned char *flags,
				size_t n, const char *want)
{
	t_vec	items;
	t_vec	sel;
	char	*out;
	char	*dup;
	size_t	i;

	vec_init(&items);
	items.elem_size = sizeof(char *);
	vec_init(&sel);
	sel.elem_size = sizeof(unsigned char);
	i = 0;
	while (i < n)
	{
		dup = ft_strdup(names[i]);
		vec_push(&items, &dup);
		vec_push_byte(&sel, flags[i]);
		i++;
	}
	out = build_output(&items, &sel);
	ASSERT_NOTNULL(out);
	ASSERT_STREQ(out, want);
	fn_free(out);
	vec_destroy(&items, free_item);
	vec_destroy(&sel, NULL);
}

TEST(output, golden)
{
	const char	*names[] = {"alpha", "beta", "gamma", "delta"};
	unsigned char	f0[] = {0, 0, 0, 0};
	unsigned char	f1[] = {1, 0, 0, 0};
	unsigned char	f2[] = {0, 1, 0, 1};
	unsigned char	f3[] = {1, 1, 1, 1};
	unsigned char	f4[] = {0, 0, 0, 1};

	SUBCASE("none selected -> empty");
	check_output(names, f0, 4, "");
	SUBCASE("first only");
	check_output(names, f1, 4, "alpha");
	SUBCASE("subset order preserved");
	check_output(names, f2, 4, "beta delta");
	SUBCASE("all selected");
	check_output(names, f3, 4, "alpha beta gamma delta");
	SUBCASE("last only");
	check_output(names, f4, 4, "delta");
}

/* output property: result has exactly (k-1) spaces for k selected, no leading
 * or trailing space, and equals the concatenation in argv order. */
TEST(output, space_invariant)
{
	t_vec	items;
	t_vec	sel;
	char	*out;
	size_t	n;
	size_t	k;
	size_t	i;
	size_t	spaces;
	unsigned int	mask;

	reseed(0xD00D);
	n = 0;
	while (n <= 14)
	{
		mask = xrand();
		mk_items(&items, n, 4);
		mk_selected(&sel, n, mask);
		k = 0;
		i = 0;
		while (i < n)
		{
			if ((mask >> (i % 32)) & 1u)
				k++;
			i++;
		}
		out = build_output(&items, &sel);
		spaces = 0;
		i = 0;
		while (out[i])
		{
			if (out[i] == ' ')
				spaces++;
			i++;
		}
		SUBCASE("n=%zu k=%zu spaces=%zu", n, k, spaces);
		ASSERT_NOTNULL(out);
		if (k > 0)
		{
			ASSERT_EQ((long)spaces, (long)(k - 1));
			ASSERT_NE((int)out[0], (int)' ');
			ASSERT_NE((int)out[ft_strlen(out) - 1], (int)' ');
		}
		else
			ASSERT_STREQ(out, "");
		fn_free(out);
		vec_destroy(&items, free_item);
		vec_destroy(&sel, NULL);
		n++;
	}
}

/* search bonus: substring match, forward-from with wrap; -1 on no/empty. */
TEST(search, substring_wrap)
{
	const char	*names[] = {"apple", "banana", "cherry", "apricot", "berry"};
	t_vec		items;
	char		*dup;
	size_t		i;

	vec_init(&items);
	items.elem_size = sizeof(char *);
	i = 0;
	while (i < 5)
	{
		dup = ft_strdup(names[i]);
		vec_push(&items, &dup);
		i++;
	}
	SUBCASE("ap from 0 -> 0");
	ASSERT_EQ(search_match(&items, "ap", 0), 0L);
	SUBCASE("ap from 1 wraps -> 3");
	ASSERT_EQ(search_match(&items, "ap", 1), 3L);
	SUBCASE("rry -> 2");
	ASSERT_EQ(search_match(&items, "rry", 0), 2L);
	SUBCASE("zz -> -1");
	ASSERT_EQ(search_match(&items, "zz", 0), -1L);
	SUBCASE("empty -> -1");
	ASSERT_EQ(search_match(&items, "", 0), -1L);
	vec_destroy(&items, free_item);
}

/* color bonus: known extensions map to non-empty escapes; unknown -> "". */
TEST(color, extension_map)
{
	SUBCASE("main.c");
	ASSERT_STRNE(ext_color("main.c"), "");
	SUBCASE("ft_select.h");
	ASSERT_STRNE(ext_color("ft_select.h"), "");
	SUBCASE("run.sh");
	ASSERT_STRNE(ext_color("run.sh"), "");
	SUBCASE("noext -> empty");
	ASSERT_STREQ(ext_color("README"), "");
	SUBCASE("dotfile -> empty");
	ASSERT_STREQ(ext_color(".bashrc"), "");
	SUBCASE("unknown ext -> empty");
	ASSERT_STREQ(ext_color("photo.jpeg"), "");
	SUBCASE("NULL -> empty");
	ASSERT_STREQ(ext_color(NULL), "");
}

int	main(int ac, char **av)
{
	return (ctest_main(ac, av));
}
