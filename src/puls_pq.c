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
 * puls_pq.c: Provider that adds Vim quickfix items to the Popup list (PULS).
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

/* TODO: These structures are copied from quickfix.c. They should be moved to structs.h */
typedef struct qfline_S qfline_T;
struct qfline_S
{
    qfline_T	*qf_next;
    qfline_T	*qf_prev;
    linenr_T	qf_lnum;
    int		qf_fnum;
    int		qf_col;
    int		qf_nr;
    char_u	*qf_pattern;
    char_u	*qf_text;
    char_u	qf_viscol;
    char_u	qf_cleared;
    char_u	qf_type;

    char_u	qf_valid;
};
#define LISTCOUNT   10

typedef struct qf_list_S
{
    qfline_T	*qf_start;
    qfline_T	*qf_ptr;
    int		qf_count;
    int		qf_index;
    int		qf_nonevalid;
    char_u	*qf_title;

} qf_list_T;

struct qf_info_S
{
    int		qf_refcount;
    int		qf_listcount;
    int		qf_curlist;
    qf_list_T	qf_lists[LISTCOUNT];
};

extern qf_info_T ql_info;	/* global quickfix list; TODO: make it non-static in quickfix.c */

/* [ooc]
 *
  class QuickfixItemProvider(ItemProvider) [qfxpr]
  {
    qf_info_T*  qfinfo;
    char_u*	_dispbuf;
    int		_dispbuf_len;

    void	init();
    void	destroy();
    char_u*	get_display_text(int item);
    int		list_items();
    void	on_start();
    int		select_item(int item);
    int		_prepare_dispbuf(int len);
    void	_update_title();
    int		_display_item(PopupItem* pitem);
    // void	read_options(dict_T* options);
    // void	default_keymap(PopupList* puls);
  };
*/

/* TODO: map commands: colder, cnewer */

    static void
_qfxpr_init(_self)
    void* _self;
    METHOD(QuickfixItemProvider, init);
{
    self->qfinfo = NULL;
    self->_dispbuf = NULL;
    self->_dispbuf_len = 0;
    self->has_title_items = 1;
    END_METHOD;
}

    static void
_qfxpr_destroy(_self)
    void* _self;
    METHOD(QuickfixItemProvider, destroy);
{
    self->qfinfo = NULL; /* popuplist doesn't own the quickfix list */
    vim_free(self->_dispbuf);
    END_DESTROY(QuickfixItemProvider);
}

    static int
_qfxpr__prepare_dispbuf(_self, len)
    void* _self;
    int len;
    METHOD(QuickfixItemProvider, _prepare_dispbuf);
{
    if (len > self->_dispbuf_len)
    {
	vim_free(self->_dispbuf);
	self->_dispbuf = alloc(len);
	self->_dispbuf_len = self->_dispbuf ? len : 0;
    }
    return self->_dispbuf_len;
    END_METHOD;
}

    static void
_qfxpr__update_title(_self)
    void* _self;
    METHOD(QuickfixItemProvider, _update_title);
{
    qf_list_T *plist;
    if (!self->qfinfo)
	return;

    plist = &self->qfinfo->qf_lists[self->qfinfo->qf_curlist];

    if (plist->qf_title)
    {
	int len = 16 + STRLEN(plist->qf_title);
	if (self->op->_prepare_dispbuf(self, len))
	{
	    vim_snprintf((char*)self->_dispbuf, self->_dispbuf_len, "Quick Fix: '%s'", plist->qf_title);
	    self->op->set_title(self, self->_dispbuf);
	}
    }
    END_METHOD;
}

    static void
_qfxpr_on_start(_self)
    void* _self;
    METHOD(QuickfixItemProvider, on_start);
{
    self->op->_update_title(self);
    END_METHOD;
}

    static char_u*
_qfxpr_get_display_text(_self, item)
    void* _self;
    int item;
    METHOD(QuickfixItemProvider, get_display_text);
{
    PopupItem_T* pit;
    qfline_T  *pqerr;
    int len;
    pit = self->items->op->get_item(self->items, item);
    if (! pit)
	return NULL;

    if (pit->flags & ITEM_TITLE || !pit->text || !pit->data)
	return pit->text;

    /* TODO: (maybe) get_display_prefix() would help to avoid creating a new string:
     *    - prefix = " %d: " % qf_lnum  -> " 123: "
     *    - text   = pit->text  -> "error description"
     *    => display:   " 123: error description"
     * This would probably require a design change in TextWriter-s. */

    len = 16 + STRLEN(pit->text);
    if (! self->op->_prepare_dispbuf(self, len))
	return pit->text;

    pqerr = (qfline_T*) pit->data;
    vim_snprintf((char*)self->_dispbuf, self->_dispbuf_len, " %3d: %s", pqerr->qf_lnum, pit->text);

    return self->_dispbuf;
    END_METHOD;
}

    static int
_qfxpr_list_items(_self)
    void* _self;
    METHOD(QuickfixItemProvider, list_items);
{
    qf_list_T *plist;
    qfline_T  *pqerr, *pprev;
    int isel, i;
    char_u    *bufname;
    PopupItem_T *pit;
    self->op->clear_items(self);
    if (!self->qfinfo)
	return 0;

    plist = &self->qfinfo->qf_lists[self->qfinfo->qf_curlist];

    pprev = NULL;
    isel = 0;
    i = 0;
    /* NOTE: the last item points to itself; qf_index is 1-based. */
    for (i = 0, pqerr = plist->qf_start; i < plist->qf_count; ++i, pqerr = pqerr->qf_next)
    {
	if (pprev == NULL || pprev->qf_fnum != pqerr->qf_fnum)
	{
	    /* add the filename as title */;
	    bufname = buflist_nr2name(pqerr->qf_fnum, 1, 0);
	    if (bufname)
		/* bufname will be deleted when the item is deleted (!ITEM_SHARED) */
		pit = self->op->append_pchar_item(self, bufname, !ITEM_SHARED);
	    else
		pit = self->op->append_pchar_item(self, vim_strsave(VSTR("<unknown file>")), !ITEM_SHARED);
	    if (pit)
		pit->flags |= ITEM_TITLE;
	}
	pit = self->op->append_pchar_item(self, pqerr->qf_text, ITEM_SHARED);
	if (pit)
	    pit->data = (void*) pqerr;

	if ((i+1) == plist->qf_index)
	    isel = self->op->get_item_count(self) - 1;

	pprev = pqerr;
    }

    return isel;
    END_METHOD;
}

    static int
_qfxpr_select_item(_self, item)
    void* _self;
    int item;
    METHOD(QuickfixItemProvider, select_item);
{
    PopupItem_T* pit;
    if (! self->qfinfo)
	return 1;

    pit = self->op->get_item(self, item);
    if (!pit || !pit->data)
	return 1; /* continue event loop because the selected item is invalid */

    if (self->op->_display_item(self, pit))
	return 0;

    return 1;
    END_METHOD;
}

    static int
_qfxpr__display_item(_self, pitem)
    void* _self;
    PopupItem_T* pitem;
    METHOD(QuickfixItemProvider, _display_item);
{
    qf_list_T *plist;
    qfline_T  *pqerr;
    int i;
    if (! self->qfinfo)
	return 0;

    plist = &self->qfinfo->qf_lists[self->qfinfo->qf_curlist];

    /* NOTE: the last item points to itself! */
    for (i = 0, pqerr = plist->qf_start; i < plist->qf_count; ++i, pqerr = pqerr->qf_next)
    {
	if (pitem->data == pqerr)
	{
	    qf_jump(self->qfinfo, 0 /* no direction */, i+1, 0 /* !forceit */);
	    return 1;
	}
    }

    return 0;
    END_METHOD;
}

