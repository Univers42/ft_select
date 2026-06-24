/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   color.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dlesieur <dlesieur@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 03:07:07 by dlesieur          #+#    #+#             */
/*   Updated: 2026/06/24 03:07:07 by dlesieur         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_select.h"

/*
** Pure extension -> ANSI color mapping (bonus, ls-like). ext_color returns a
** color escape for the file extension of `name`, or "" when there is no
** recognised extension. Pure: returns a static string, no allocation, no
** terminal. The render layer wraps the item text with this and a reset.
*/

static const char	*lookup(const char *ext)
{
	static const char	*tbl[] = {
		"c", "\033[36m", "h", "\033[36m", "o", "\033[90m",
		"a", "\033[31m", "sh", "\033[32m", "py", "\033[33m",
		"md", "\033[35m", "txt", "\033[37m", NULL};
	size_t				i;

	i = 0;
	while (tbl[i] != NULL)
	{
		if (ft_strcmp(tbl[i], ext) == 0)
			return (tbl[i + 1]);
		i += 2;
	}
	return ("");
}

const char	*ext_color(const char *name)
{
	const char	*dot;

	if (name == NULL)
		return ("");
	dot = ft_strrchr(name, '.');
	if (dot == NULL || dot == name || dot[1] == '\0')
		return ("");
	return (lookup(dot + 1));
}
