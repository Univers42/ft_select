/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ft_select.h                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dlesieur <dlesieur@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 03:07:07 by dlesieur          #+#    #+#             */
/*   Updated: 2026/06/24 03:07:07 by dlesieur         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef FT_SELECT_H
# define FT_SELECT_H

# include "libft.h"
# include <termios.h>
# include <signal.h>

# define GUTTER 2

/* a decoded keypress */
typedef enum e_key
{
	K_NONE = 0,
	K_UP,
	K_DOWN,
	K_LEFT,
	K_RIGHT,
	K_ENTER,
	K_ESC,
	K_SPACE,
	K_DEL,
	K_PRINT
}	t_key;

/* a navigation direction */
typedef enum e_dir
{
	D_UP,
	D_DOWN,
	D_LEFT,
	D_RIGHT
}	t_dir;

/* column-packing geometry (pure, computed from item count + window) */
typedef struct s_layout
{
	int	term_rows;
	int	term_cols;
	int	n_cols;
	int	n_rows;
	int	col_w;
	int	too_small;
}	t_layout;

/* termcap capability strings (loaded once from libft tgetent) */
typedef struct s_caps
{
	char	*cm;
	char	*cl;
	char	*ce;
	char	*cd;
	char	*so;
	char	*se;
	char	*us;
	char	*ue;
	char	*me;
	char	entry[4096];
	char	area[2048];
}	t_caps;

/* the whole application; passed by pointer (no globals but the signal flag) */
typedef struct s_app
{
	t_vec			items;
	t_vec			selected;
	t_vec			frame;
	t_vec			search;
	size_t			cursor;
	int				col_off;
	t_caps			caps;
	t_layout		lay;
	struct termios	saved;
	int				tty_fd;
	int				raw_active;
}	t_app;

/* ===== pure logic (no terminal; the unit-test seam) ===== */
/* args.c */
int			args_ingest(int ac, char **av, t_vec *items);
void		free_item(void *p);
size_t		max_item_width(t_vec *items);
/* layout.c */
t_layout	layout_compute(size_t n, int cols, int rows, int max_w);
int			cell_of_index(t_layout l, size_t idx, int *row, int *col);
long		index_of_cell(t_layout l, int row, int col, size_t n);
int			visible_cols(t_layout l);
/* navigate.c */
size_t		nav_move(t_layout l, size_t cur, size_t n, t_dir d);
/* select_ops.c */
void		sel_toggle(t_vec *selected, size_t i);
size_t		item_delete(t_vec *items, t_vec *selected, size_t cur);
/* output.c */
char		*build_output(t_vec *items, t_vec *selected);

/* ===== bonuses (pure where possible) ===== */
/* search.c */
long		search_match(t_vec *items, const char *query, size_t from);
const char	*search_query(t_vec *search);
/* color.c */
const char	*ext_color(const char *name);
/* interact.c */
void		scroll_to_cursor(t_app *app);
void		search_input(t_app *app, unsigned char ch);
int			search_erase(t_app *app);
int			search_cancel(t_app *app);

/* ===== terminal layer (impure) ===== */
/* term.c */
int			term_setup(t_app *app);
int			term_raw(t_app *app);
void		term_restore(t_app *app);
int			caps_load(t_caps *caps);
void		term_dims(int *rows, int *cols);
/* input.c */
t_key		key_read(unsigned char *out_ch);
/* render.c */
void		render_frame(t_app *app);
/* signals.c */
extern volatile sig_atomic_t	g_signo;
void		signals_install(t_app *app);
void		signals_restore_term(void);
void		signals_disarm(void);
/* main.c */
int			app_init(t_app *app, int ac, char **av);
void		app_free(t_app *app);
int			run_loop(t_app *app);

#endif
