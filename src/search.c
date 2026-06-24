/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   search.c                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dlesieur <dlesieur@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 03:07:07 by dlesieur          #+#    #+#             */
/*   Updated: 2026/06/24 03:07:07 by dlesieur         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_select.h"

/*
** Pure dynamic-search primitive (bonus). search_match returns the index of the
** first item (scanning forward from `from`, wrapping once) whose text contains
** the query as a substring, or -1 when nothing matches / the query is empty.
** Pure: no terminal, fully unit-testable.
*/

long	search_match(t_vec *items, const char *query, size_t from)
{
	size_t	count;
	size_t	i;
	size_t	idx;
	char	*item;

	if (query == NULL || *query == '\0' || items->len == 0)
		return (-1);
	count = 0;
	i = from % items->len;
	while (count < items->len)
	{
		idx = (i + count) % items->len;
		item = *(char **)vec_idx(items, idx);
		if (item != NULL && ft_strstr(item, query) != NULL)
			return ((long)idx);
		count++;
	}
	return (-1);
}

/* A NUL-terminated view over the search byte-vec. vec_push_byte only writes the
** trailing NUL when there is spare capacity, so guarantee a spare slot then
** terminate explicitly here. The empty buffer reports as "". */
const char	*search_query(t_vec *search)
{
	if (search->len == 0 || !vec_ensure_space(search))
		return ("");
	((char *)search->ctx)[search->len] = '\0';
	return ((char *)search->ctx);
}
