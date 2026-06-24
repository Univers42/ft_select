/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   output.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dlesieur <dlesieur@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 03:07:07 by dlesieur          #+#    #+#             */
/*   Updated: 2026/06/24 03:07:07 by dlesieur         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_select.h"

/*
** Pure output assembly: join the selected items (argv order) with single
** spaces, no trailing space. The result is a freshly malloc'd, NUL-terminated
** string; an empty selection yields an allocated empty string "". The caller
** owns the returned pointer.
*/

static int	append_item(t_vec *out, const char *s, int first)
{
	if (!first && !vec_push_byte(out, ' '))
		return (-1);
	if (!vec_push_str(out, s))
		return (-1);
	return (0);
}

char	*build_output(t_vec *items, t_vec *selected)
{
	t_vec	out;
	size_t	i;
	int		first;

	vec_init(&out);
	first = 1;
	i = 0;
	while (i < items->len)
	{
		if (i < selected->len && *(unsigned char *)vec_idx(selected, i))
		{
			if (append_item(&out, *(char **)vec_idx(items, i), first) < 0)
				return (vec_destroy(&out, NULL), NULL);
			first = 0;
		}
		i++;
	}
	if (!vec_ensure_space(&out))
		return (vec_destroy(&out, NULL), NULL);
	((char *)out.ctx)[out.len] = '\0';
	return ((char *)out.ctx);
}
