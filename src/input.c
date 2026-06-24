/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   input.c                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: dlesieur <dlesieur@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/24 03:07:07 by dlesieur          #+#    #+#             */
/*   Updated: 2026/06/24 03:07:07 by dlesieur         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ft_select.h"

/*
** Byte-stream keyboard decoding. A single read on stdin yields one byte; arrows
** and Delete arrive as CSI sequences (ESC [ ...). A lone Esc is ambiguous with
** the start of a CSI, so after ESC we drop to a bounded VMIN=0/VTIME=1 peek: no
** following byte => lone Esc, '[' => parse the CSI tail. Any other printable
** byte returns K_PRINT with *out_ch set. Unknown sequences map to K_NONE.
*/

static int	peek_byte(unsigned char *b)
{
	struct termios	cur;
	struct termios	tmp;
	ssize_t			r;

	if (tcgetattr(STDIN_FILENO, &cur) != 0)
		return (0);
	tmp = cur;
	tmp.c_cc[VMIN] = 0;
	tmp.c_cc[VTIME] = 1;
	tcsetattr(STDIN_FILENO, TCSANOW, &tmp);
	r = read(STDIN_FILENO, b, 1);
	tcsetattr(STDIN_FILENO, TCSANOW, &cur);
	return (r == 1);
}

static t_key	decode_csi(void)
{
	unsigned char	c;
	unsigned char	tilde;

	if (!peek_byte(&c))
		return (K_NONE);
	if (c == 'A')
		return (K_UP);
	if (c == 'B')
		return (K_DOWN);
	if (c == 'C')
		return (K_RIGHT);
	if (c == 'D')
		return (K_LEFT);
	if (c == '3' && peek_byte(&tilde) && tilde == '~')
		return (K_DEL);
	return (K_NONE);
}

static t_key	decode_esc(void)
{
	unsigned char	c;

	if (!peek_byte(&c))
		return (K_ESC);
	if (c == '[')
		return (decode_csi());
	return (K_ESC);
}

t_key	key_read(unsigned char *out_ch)
{
	unsigned char	c;

	*out_ch = 0;
	if (read(STDIN_FILENO, &c, 1) != 1)
		return (K_NONE);
	if (c == 0x1b)
		return (decode_esc());
	if (c == 0x0a || c == 0x0d)
		return (K_ENTER);
	if (c == 0x20)
		return (K_SPACE);
	if (c == 0x7f || c == 0x08)
		return (K_DEL);
	if (c >= 0x20 && c < 0x7f)
		return (*out_ch = c, K_PRINT);
	return (K_NONE);
}
