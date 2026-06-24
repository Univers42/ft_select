/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   args.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dlesieur <dlesieur@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 03:07:07 by dlesieur          #+#    #+#             */
/*   Updated: 2026/06/24 03:07:07 by dlesieur         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_select.h"

/*
** Pure argv ingestion. items is a t_vec of (char *): each argv entry is
** duplicated with ft_strdup and pushed. free_item is the vec_destroy callback
** (it receives &slot, i.e. a char **). max_item_width returns the longest
** ft_strlen across the items (0 when empty).
*/

void	free_item(void *p)
{
	char	**slot;

	if (p == NULL)
		return ;
	slot = (char **)p;
	fn_free(*slot);
	*slot = NULL;
}

int	args_ingest(int ac, char **av, t_vec *items)
{
	int		i;
	char	*dup;

	vec_init(items);
	items->elem_size = sizeof(char *);
	i = 1;
	while (i < ac)
	{
		dup = ft_strdup(av[i]);
		if (dup == NULL || !vec_push(items, &dup))
		{
			fn_free(dup);
			vec_destroy(items, free_item);
			return (-1);
		}
		i++;
	}
	return ((int)items->len);
}

size_t	max_item_width(t_vec *items)
{
	size_t	i;
	size_t	w;
	size_t	max;

	max = 0;
	i = 0;
	while (i < items->len)
	{
		w = ft_strlen(*(char **)vec_idx(items, i));
		if (w > max)
			max = w;
		i++;
	}
	return (max);
}
