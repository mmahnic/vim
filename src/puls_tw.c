/* vi:set ts=8 sts=4 sw=4 noet list:vi
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *                      Popup List by Marko Mahnič
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

/*
 * puls_tw.c: Text writer for the Popup list (PULS)
 * NOTE: this file is included by popuplist.c
 *
 * Copyright © 2011 Marko Mahnič.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


/* [ooc]
 *
  // Write text in one line. Take care of line offsets. Expand tabs.
  const PLWR_MAX_FIXED_TABS = 8;
  class LineWriter [plwr]
  {
    int	    fixed_tabs[PLWR_MAX_FIXED_TABS+1];	// predefined tab positions; last is 0
    int	    tab_size;	// tab size after the last position; negative = fixed number of spaces
    int	    offset;	// number of cells the text is shifted to the left
    int	    min_col;	// start column
    int	    max_col;	// end column
    void    init();
    // void    destroy();
    void    add_fixed_tab(int col);
    int	    get_tab_size_at(int col);
    void    set_limits(int min_col, int max_col);
    // write the text and fill to max_col with fillChar if it is not NUL
    void    write_line(char_u* text, int row, int attr, int fillChar);
  };
*/

    static void
_plwr_init(_self)
    void* _self;
    METHOD(LineWriter, init);
{
    self->fixed_tabs[0] = 0;
    self->tab_size = 8;
    self->min_col = 0;
    self->max_col = Columns-1;
    self->offset = 0;
    END_METHOD;
}

    static int
_int_compare (a, b)
    const void* a;
    const void* b;
{
    return ( *(int*)a - *(int*)b );
}

    static void
_plwr_add_fixed_tab(_self, col)
    void* _self;
    int col;
    METHOD(LineWriter, add_fixed_tab);
{
    int i = 0;
    while (i < PLWR_MAX_FIXED_TABS && self->fixed_tabs[i] > 0)
	i++;
    if (i < PLWR_MAX_FIXED_TABS)
    {
	self->fixed_tabs[i] = col;
	self->fixed_tabs[i+1] = 0;
	if (i > 0)
	    qsort(self->fixed_tabs, i+1, sizeof(int), &_int_compare);
    }
    END_METHOD;
}

    static int
_plwr_get_tab_size_at(_self, col)
    void* _self;
    int col;
    METHOD(LineWriter, get_tab_size_at);
{
    int i = 0;
    int last_tab = 0;
    if (self->fixed_tabs[0] > 0)
    {
	while(self->fixed_tabs[i] > 0 && self->fixed_tabs[i] <= col)
	    i++;
	if (self->fixed_tabs[i] > col)
	    return (self->fixed_tabs[i] - col);
	else
	    last_tab = self->fixed_tabs[i-1];
    }
    if (self->tab_size < 0)
	return -self->tab_size;
    if (self->tab_size == 0)
	return 2;
    return self->tab_size - ((col - last_tab) % self->tab_size);
    END_METHOD;
}

    static void
_plwr_set_limits(_self, min_col, max_col)
    void* _self;
    int min_col;
    int max_col;
    METHOD(LineWriter, set_limits);
{
    self->min_col = min_col;
    self->max_col = max_col;
    END_METHOD;
}

/*
 *        L         R		Border
 *        01234567890		Box column
 * 012345678901234567890	Text column
 * .......7.............	Offset = 7
 *        .         .
 * ABCDEFGHI ABCDEFGHI A	Single characters in text
 * 123456789012345678		... text width (pwidth)
 *        HI ABCDEFGH		... displayed
 *        .         .
 * ABCDDDEEE ABCDDDEEE A	Wide characters in text
 * 1236669990123666999		... text width (pwidth)
 *        .. ABCDDD..		... displayed
 */
    static void
_plwr_write_line(_self, text, row, attr, fillChar)
    void* _self;
    char_u* text;
    int row;
    int attr;
    int fillChar;
    METHOD(LineWriter, write_line);
{
    char_u *p, *s;
    int pwidth, max_pwidth, w, col, endcol;

    if (!text)
	text = blankline;
    max_pwidth = self->offset + self->max_col - self->min_col + 1;

    p = text;		    /* current character */
    s = NULL;		    /* string start */
    pwidth = 0;		    /* total width including current character */
    col = self->min_col;    /* write position */

    for ( ; p != NULL; ADVANCE_CHAR_P(p))
    {
	if (*p == TAB)
	    w = self->op->get_tab_size_at(self, pwidth);
	else
	    w = ptr2cells(p);
	pwidth += w;
	if (pwidth <= self->offset)
	{
	    if (*p == NUL) break;
	    else continue;
	}
	if (s == NULL)
	{
	    if (pwidth - self->offset < w)
	    {
		/* character partly visible -- output spaces */
		endcol = col + pwidth - 1 - self->offset;
		screen_fill(row, row + 1, col, endcol + 1, ' ', ' ', attr);
		col = endcol + 1;
		continue;
	    }
	    s = p;
	}
	if (*p == NUL || *p == TAB || pwidth >= max_pwidth)
	{
	    /* Display the text that fits or comes before a Tab.
	     * First convert it to printable characters. */
	    char_u	*st;
	    int	saved = *p;

	    *p = NUL;
	    st = transstr(s);
	    *p = saved;
	    if (st)
	    {
		screen_puts_len(st, (int)STRLEN(st), row, col, attr);
		col += vim_strsize(st);
		vim_free(st);
	    }
	    s = NULL;
	    if (*p == TAB)
	    {
		endcol = limit_value(col + w - 1, col, self->max_col);
		screen_fill(row, row + 1, col, endcol + 1, ' ', ' ', attr);
		col = endcol + 1;
	    }
	    else break;
	}
    }
    if (col <= self->max_col && fillChar != NUL)
	screen_fill(row, row + 1, col, self->max_col+1, fillChar, fillChar, attr);
    END_METHOD;
}

/* [ooc]
 *
  // Filter the text to be written and calculate text attributes
  // It may hide some characters, it may add new ones.
  class Highlighter [hltr]
  {
    Highlighter* next;	    // chained highlighters
    int	    active;	    // when 0 this highlighter won't be used
    int	    state;	    // a highlighter could be a state machine
    int	    default_attr;   // the 'normal' text attribute
    int	    text_attr;	    // attribute starting at next_char
    int	    text_width;	    // width of text processed
    char_u* match_end;	    // the attrubutes remain unchanged until match_end
    void    init();
    // void    destroy();
    void    bol_init(char_u* text, void* extra_data);

    // Intended use of match_end in calc_attr:
    //    if (self->match_end >= next_char) return 1;
    //    ... process num_chars from next_char;
    //    ... processed chars _must_ have the same attribute
    //    self->match_end = next_char + num_chars;
    // @returns
    //    0 if the highlighter doesn't affect the output
    //    1 if the highlighter has suggestions for the output width and attr
    int	    calc_attr(char_u* next_char);
  };

*/

    static void
_hltr_init(_self)
    void* _self;
    METHOD(Highlighter, init);
{
    self->next = NULL;
    self->active = 1;
    self->state = 0; /* TODO: give names to the states */
    self->default_attr = 0;
    self->text_attr = 0;
    self->text_width = -1; /* let the caller calculate it */
    self->match_end = NULL;
    END_METHOD;
}

    static void
_hltr_bol_init(_self, text, extra_data)
    void* _self;
    char_u* text;
    void* extra_data;
    METHOD(Highlighter, bol_init);
{
    self->state = 0;
    self->text_attr = self->default_attr;
    self->text_width = -1;
    self->match_end = NULL;
    END_METHOD;
}

    static int
_hltr_calc_attr(_self, next_char)
    void* _self;
    char_u* next_char;
    METHOD(Highlighter, calc_attr);
{
    return 1;
    END_METHOD;
}

/* [ooc]
 *
  // Highlights shortcuts that start with the '&' symbol; '&' is removed.
  // TODO: a new state for auto-assigned shortcuts; the shortcut is assigned to an item
  // as the offset from the start of the item. In state '3', the ShortcutHighlighter
  // writes a new attribute at that offset. A negative offset means: use only '&'.
  class ShortcutHighlighter(Highlighter) [hlshrt]
  {
    int	    shortcut_attr;
    void    init();
    int	    calc_attr(char_u* next_char);
  };
*/

    static void
_hlshrt_init(_self)
    void* _self;
    METHOD(ShortcutHighlighter, init);
{
    self->shortcut_attr = self->default_attr;
    END_METHOD;
}

    static int
_hlshrt_calc_attr(_self, next_char)
    void* _self;
    char_u* next_char;
    METHOD(ShortcutHighlighter, calc_attr);
{
    int rv = 0;

    /* states: 0 - normal, 1 - hl next char, 2 - un-hl this char */
    if (self->state == 1)
    {
	self->text_width = -1;
	self->state = 2;
	if (*next_char == '&')
	{
	    self->text_attr = self->default_attr;
	    return 0;
	}
	else
	    self->text_attr = self->shortcut_attr;
	return 1;
    }
    else if (self->state == 2)
    {
	self->text_attr = self->default_attr;
	self->state = 0;
	rv = 1;
    }

    if (*next_char == '&' && !VIM_ISWHITE(*(next_char+1)))
    {
	self->state = 1;
	self->text_width = 0;
	rv = 1;
    }

    return rv;
    END_METHOD;
}

/* [ooc]
 *
  class TextMatchHighlighter(Highlighter) [hltxm]
  {
    TextMatcher* matcher;
    int	    match_attr;
    char_u* match_start;
    void    init();
    // void    destroy();
    void    set_matcher(TextMatcher* matcher);
    void    bol_init(char_u* text, void* extra_data);
    int	    calc_attr(char_u* next_char);
  };
*/

    static void
_hltxm_init(_self)
    void* _self;
    METHOD(TextMatchHighlighter, init);
{
    self->matcher = NULL;
    self->match_attr = _puls_hl_attrs[PULSATTR_SELECTED].attr;
    END_METHOD;
}

    static void
_hltxm_set_matcher(_self, matcher)
    void* _self;
    TextMatcher_T* matcher;
    METHOD(TextMatchHighlighter, set_matcher);
{
    self->matcher = matcher;
    END_METHOD;
}

    static void
_hltxm_bol_init(_self, text, extra_data)
    void* _self;
    char_u* text;
    void* extra_data;
    METHOD(TextMatchHighlighter, bol_init);
{
    super(TextMatchHighlighter, bol_init)(self, text, extra_data);

    /* Some matchers (eg. command-t) have to be initialized before they can be
     * used for highlighting. Note that the string used for filtering may be
     * different from the string being displayed and the highlighted match may
     * be different from the actual one.
     */
    if (self->matcher)
	self->matcher->op->init_highlight(self->matcher, text);
    END_METHOD;
}

    static int
_hltxm_calc_attr(_self, next_char)
    void* _self;
    char_u* next_char;
    METHOD(TextMatchHighlighter, calc_attr);
{
    int len;
    if (! self->matcher)
	return 0;

    if (self->match_end >= next_char)
	return 1;

    len = self->matcher->op->get_match_at(self->matcher, next_char);
    if (len)
    {
	self->text_attr = self->match_attr;
	self->match_end = next_char + len - 1;
	return 1;
    }
    else
    {
	self->text_attr = self->default_attr;
	self->match_end = next_char;
    }
    return 0;
    END_METHOD;
}

/* [ooc]
 *
  // Write text in one line. Take care of line offsets. Expand tabs. Highlight text.
  //
  // Uses multiple highliters, eg.: hlsyn, hlshortcut, hlspell, hlsearch, ...
  //   - each proposes an attr and a length for current char
  //   - the results are combined; the last one has top priority, unless the
  //   text is concealed by others
  class LineHighlightWriter(LineWriter) [plhlwr]
  {
    char_u* _tmpbuf;
    char_u* _tmplimit;
    Highlighter* highlighters;
    void    init();
    void    destroy();
    void    write_line(char_u* text, int row, int init_attr, int fillChar);

    // text_end points into _tmpbuf; *text_end will be set to NUL;
    int	    _flush(char_u* text_end, int row, int col, int attr);
  }
 */

    static void
_plhlwr_init(_self)
    void* _self;
    METHOD(LineHighlightWriter, init);
{
    self->_tmpbuf = (char_u*) alloc(sizeof(char_u) * (Columns + 32));
    self->_tmplimit = self->_tmpbuf + Columns + 16;
    self->highlighters = NULL;
    END_METHOD;
}

    static void
_plhlwr_destroy(_self)
    void* _self;
    METHOD(LineHighlightWriter, destroy);
{
    vim_free(self->_tmpbuf);
    self->_tmpbuf = NULL;
    self->_tmplimit = NULL;
    self->highlighters = NULL;
    END_DESTROY(LineHighlightWriter);
}

    static int
_plhlwr__flush(_self, text_end, row, col, attr)
    void* _self;
    char_u* text_end;
    int row;
    int col;
    int attr;
    METHOD(LineHighlightWriter, _flush);
{
    char_u  *st;
    *text_end = NUL;
    st = transstr(self->_tmpbuf);
    if (st)
    {
	screen_puts_len(st, (int)STRLEN(st), row, col, attr);
	col = vim_strsize(st);
	vim_free(st);
    }
    return col; /* number of columns written */
    END_METHOD;
}

    static void
_plhlwr_write_line(_self, text, row, init_attr, fillChar)
    void* _self;
    char_u* text;
    int row;
    int init_attr;
    int fillChar;
    METHOD(LineHighlightWriter, write_line);
{
    char_u *p, *s;
    int pwidth, w, col, max_pwidth, attr, next_attr;
    Highlighter_T *phl;
    char_u *ptmp, *ptmplimit;

    if (!text)
	text = blankline;
    ptmp = self->_tmpbuf;
    ptmplimit = self->_tmplimit;
    max_pwidth = self->offset + self->max_col - self->min_col + 1;
    next_attr = init_attr;

    phl = self->highlighters;
    while (phl)
    {
	if (phl->active)
	{
	    phl->default_attr = init_attr;
	    phl->op->bol_init(phl, text, NULL);
	}
	phl = phl->next;
    }

    p = text;		    /* current character */
    pwidth = 0;		    /* total display width including current character */
    col = self->min_col;    /* write position */

    while (p != NULL && *p != NUL && pwidth < max_pwidth)
    {
	attr = next_attr;
	s = NULL; /* contig part start */
	/* collect into tmp */
	for ( ; ; )
	{
	    if (! self->highlighters)
	    {
		if (*p == TAB)
		    w = self->op->get_tab_size_at(self, pwidth);
		else
		    w = ptr2cells(p);
	    }
	    else
	    {
		phl = self->highlighters;
		w = -1;
		next_attr = init_attr;
		while (phl)
		{
		    if (phl->active && phl->op->calc_attr(phl, p))
		    {
			w = phl->text_width;
			next_attr = phl->text_attr;
			if (w == 0)
			    break;
		    }
		    phl = phl->next;
		}

		if (w < 0)
		{
		    if (*p == TAB)
			w = self->op->get_tab_size_at(self, pwidth);
		    else
			w = ptr2cells(p);
		}
		else if (w > 0 && *p == TAB)
		    w = self->op->get_tab_size_at(self, pwidth);

		if (next_attr != attr)
		{
		    if (s)
		    {
			col += self->op->_flush(self, s, row, col, attr);
			s = NULL;
		    }
		    attr =next_attr;
		}
	    }

	    if (w == 0)
	    {
		ADVANCE_CHAR_P(p);
		continue;
	    }

	    pwidth += w;
	    if (pwidth <= self->offset)
	    {
		if (*p == NUL) break;
		else
		{
		   ADVANCE_CHAR_P(p);
		   continue;
		}
	    }
	    if (s == NULL)
	    {
		int dfirst = pwidth - self->offset; /* visible width of first char */
		s = ptmp;
		if (dfirst < w)
		{
		    /* character partly visible -- output spaces */
		    memset(s, ' ', dfirst);
		    s += dfirst;
		    ADVANCE_CHAR_P(p);
		    continue;
		}
	    }
	    if (*p == NUL || pwidth > max_pwidth)
		break;
	    else if (*p == TAB)
	    {
		memset(s, ' ', w); /* XXX: we could implement sth. like 'set list' */
		s += w;
		ADVANCE_CHAR_P(p);
	    }
	    else
		MB_COPY_CHAR(p, s);

	    if (s > ptmplimit)
	       	break;
	}

	/* output */
	if (s)
	{
	    col += self->op->_flush(self, s, row, col, attr);
	    s = NULL;
	}
    }
    if (col <= self->max_col && fillChar != NUL)
	screen_fill(row, row + 1, col, self->max_col+1, fillChar, fillChar, init_attr);
    END_METHOD;
}

