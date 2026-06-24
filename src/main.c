/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dlesieur <dlesieur@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 03:07:07 by dlesieur          #+#    #+#             */
/*   Updated: 2026/06/24 03:07:07 by dlesieur         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_select.h"

/*
** Entry point and application setup/teardown. app_init guards the terminal,
** loads capabilities, ingests argv and prepares the parallel selection vector
** plus the reusable frame/search buffers; app_free releases everything. The
** terminal is restored on every failing path here and inside the run loop. The
** loop itself (render/read/dispatch) lives in loop.c.
*/

static void	seed_selected(t_app *app)
{
	size_t			i;
	unsigned char	zero;

	vec_init(&app->selected);
	app->selected.elem_size = sizeof(unsigned char);
	i = 0;
	zero = 0;
	while (i < app->items.len)
	{
		vec_push(&app->selected, &zero);
		i++;
	}
}

int	app_init(t_app *app, int ac, char **av)
{
	ft_memset(app, 0, sizeof(*app));
	if (term_setup(app) != 0)
		return (-1);
	if (caps_load(&app->caps) != 0 || args_ingest(ac, av, &app->items) < 0)
	{
		ft_putstr_fd("ft_select: initialisation failed\n", 2);
		return (term_restore(app), -1);
	}
	signals_install(app);
	seed_selected(app);
	vec_init(&app->frame);
	vec_init(&app->search);
	term_dims(&app->lay.term_rows, &app->lay.term_cols);
	app->lay = layout_compute(app->items.len, app->lay.term_cols,
			app->lay.term_rows, (int)max_item_width(&app->items));
	return (0);
}

void	app_free(t_app *app)
{
	vec_destroy(&app->items, free_item);
	vec_destroy(&app->selected, NULL);
	vec_destroy(&app->frame, NULL);
	vec_destroy(&app->search, NULL);
}

int	main(int ac, char **av)
{
	t_app	app;
	int		rc;

	if (app_init(&app, ac, av) != 0)
		return (1);
	if (app.items.len == 0)
		rc = (term_restore(&app), 0);
	else
		rc = run_loop(&app);
	signals_disarm();
	app_free(&app);
	return (rc);
}
