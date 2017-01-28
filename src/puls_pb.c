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
 * puls_pb.c: Buffer list provider for the Popup list (PULS)
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
  // the sort order is applied when the items are unfiltered
  const BUFSORT_NR	= 'n';
  const BUFSORT_NAME	= 'm';
  const BUFSORT_MRU	= 'r';
  const BUFSORT_EXT	= 'x';
  const BUFSORT_PATH	= 'p';
  class BufferItemProvider(ItemProvider) [bprov, variant FEAT_POPUPLIST_BUFFERS]
  {
    char	sorted_by;
    int		show_unlisted;
    list_T*	mru_list;
    void	init();
    // void	destroy();
    void	read_options(dict_T* options);
    void	on_start();
    void	default_keymap(PopupList* puls);
    void	list_buffers();
    int		sort_buffers();
    char_u*	get_title();
    int		_index_to_bufnr(int index);
    // update the status dictionary which will be sent to a VimL callback
    // function or the the caller script on 'accept'
    void	update_result(dict_T* status);
    char_u*	handle_command(PopupList* puls, char_u* command);
  };
*/

    static void
_bprov_init(_self)
    void* _self;
    METHOD(BufferItemProvider, init);
{
    self->sorted_by = BUFSORT_NR;
    self->show_unlisted = 0;
    self->mru_list = NULL;
    END_METHOD;
}

    static char_u*
_bprov_get_title(_self)
    void* _self;
    METHOD(ItemProvider, get_title);
{
    static char_u title[] = "Buffers";
    return title;
    END_METHOD;
}

    static void
_bprov_read_options(_self, options)
    void* _self;
    dict_T* options;
    METHOD(BufferItemProvider, read_options);
{
    dictitem_T* option;

    option = dict_find(options, VSTR("unlisted"), -1L);
    if (option && option->di_tv.v_type == VAR_NUMBER)
    {
	self->show_unlisted = option->di_tv.vval.v_number ? 1 : 0;
    }

    option = dict_find(options, VSTR("sort"), -1L);
    if (option && option->di_tv.v_type == VAR_STRING)
    {
	self->sorted_by = *option->di_tv.vval.v_string;
	LOG(("BufferItemProvider sort option %c", self->sorted_by));
    }

    option = dict_find(options, VSTR("mru-list"), -1L);
    if (option && option->di_tv.v_type == VAR_LIST)
    {
	self->mru_list = option->di_tv.vval.v_list;
    }
    END_METHOD;
}

    static void
_bprov_on_start(_self)
    void* _self;
    METHOD(BufferItemProvider, on_start);
{
    LOG(("BufferItemProvider on_start"));
    if (self->sorted_by != BUFSORT_NR)
	self->op->sort_buffers(self);
    END_METHOD;
}

    static void
_bprov_list_buffers(_self)
    void* _self;
    METHOD(BufferItemProvider, list_buffers);
{
    buf_T	*buf;
    int		len;
    int		i;
    char_u	*fname;
    char_u	*dirname;
    char_u	curdir[] = ".";
    PopupItem_T* pit;

    self->op->clear_items(self);

    for (buf = firstbuf; buf != NULL; buf = buf->b_next)
    {
	if (got_int)
	    break;

	/* skip unlisted buffers */
	if (! self->show_unlisted && ! buf->b_p_bl)
	    continue;

	if (buf_spname(buf) != NULL)
	{
	    STRCPY(NameBuff, buf_spname(buf));
	    fname = NameBuff;
	    dirname = curdir;
	}
	else
	{
	    /* XXX: modify_fname, home_replace, shorten_fname, mch_dirname
	     * ... can't figure out a good solution, so we split by '/'
	     */
	    home_replace(buf, buf->b_ffname, NameBuff, MAXPATHL, TRUE);
	    fname = vim_strrchr(NameBuff, '/');
	    if (fname)
	    {
		*fname = NUL; /* truncate dirname */
		fname++;
		dirname = NameBuff;
	    }
	    else
	    {
		fname = NameBuff;
		dirname = curdir;
	    }
	}

	len = vim_snprintf((char *)IObuff, IOSIZE - 20, "%3d%c%c%c%c%c %s\t%s",
		buf->b_fnum,
		buf->b_p_bl ? ' ' : 'u',
		buf == curbuf ? '%' :
			(curwin->w_alt_fnum == buf->b_fnum ? '#' : ' '),
		buf->b_ml.ml_mfp == NULL ? ' ' :
			(buf->b_nwindows == 0 ? 'h' : 'a'),
		!buf->b_p_ma ? '-' : (buf->b_p_ro ? '=' : ' '),
		(buf->b_flags & BF_READERR) ? 'x' :
			(bufIsChanged(buf) ? '+' : ' '),
		fname, dirname);

	pit = self->op->append_pchar_item(self, vim_strsave(IObuff), !ITEM_SHARED);
	if (pit)
	{
	    pit->data = (void *)buf;
	    pit->filter_start = 9;

	    i = 1000;
	    while (buf->b_fnum >= i)
	    {
		++pit->filter_start;
		i *= 10;
	    }
	}
    }
    END_METHOD;
}

    static int
_BufferItem_cmp_nr(comparator, a, b)
    void* comparator;
    PopupItem_T* a;
    PopupItem_T* b;
{
    if ( ((buf_T*)a->data)->b_fnum < ((buf_T*)b->data)->b_fnum )
	return -1;
    if ( ((buf_T*)a->data)->b_fnum > ((buf_T*)b->data)->b_fnum )
	return 1;
    return 0;
}

    static int
_BufferItem_cmp_path(comparator, a, b)
    void* comparator;
    PopupItem_T* a;
    PopupItem_T* b;
{
    return STRCMP( ((buf_T*)a->data)->b_ffname, ((buf_T*)b->data)->b_ffname );
}


    static int
_BufferItem_cmp_filter_score(comparator, a, b)
    void* comparator;
    PopupItem_T* a;
    PopupItem_T* b;
{
    if (a->filter_score < b->filter_score)
	return -1;
    if (a->filter_score > b->filter_score)
	return 1;
    /* sort items with equal scores by bufnr */
    if ( ((buf_T*)a->data)->b_fnum < ((buf_T*)b->data)->b_fnum )
	return -1;
    if ( ((buf_T*)a->data)->b_fnum > ((buf_T*)b->data)->b_fnum )
	return 1;
    return 0;
}

typedef struct _pulspb_bufnr_order_s_
{
    int bufnr;
    ushort mru_order;
} _pulspb_bufnr_order_T;

    static int
_PulsPb_MruOrderCmp_bufnr(comparator, a, b)
    void* comparator;
    _pulspb_bufnr_order_T* a;
    _pulspb_bufnr_order_T* b;
{
    if (a->bufnr < b->bufnr)
	return -1;
    if (a->bufnr > b->bufnr)
	return 1;
    return 0;
}

    static int
_bprov_sort_buffers(_self)
    void* _self;
    METHOD(BufferItemProvider, sort_buffers);
{
    SegmentedGrowArray_T order;
    SegmentedArrayIterator_T itbufs, itorder;
    ItemComparator_T cmp;
    listitem_T* plit;
    ushort i;
    _pulspb_bufnr_order_T* pord;
    PopupItem_T* ppit;
    int rv;

    LOG(("BufferItemProvider sort_buffers"));
    init_ItemComparator(&cmp);

    switch (self->sorted_by)
    {
	case BUFSORT_NR:
	    cmp.fn_compare = &_BufferItem_cmp_nr;
	    break;
	case BUFSORT_PATH:
	    cmp.fn_compare = &_BufferItem_cmp_path;
	    break;
	case BUFSORT_NAME:
	    cmp.fn_compare = &_BufferItem_cmp_path;
	    break;
	case BUFSORT_EXT:
	    cmp.fn_compare = &_BufferItem_cmp_path;
	    break;
	case BUFSORT_MRU:
	    LOG(("BufferItemProvider BUFSORT_MRU"));
	    if (self->mru_list)
	    {
		/* 1. sort by bufnr */
		cmp.fn_compare = &_BufferItem_cmp_nr;
		if (self->op->sort_items(self, &cmp))
		{
		    /* 2. sort the MRU list by bufnr, but remember the MRU position */
		    init_SegmentedGrowArray(&order);
		    order.item_size = sizeof(_pulspb_bufnr_order_T);
		    plit = self->mru_list->lv_first;
		    i = 0;
		    while (plit && i < 65000)
		    {
			if (plit->li_tv.v_type == VAR_NUMBER)
			{
			    pord = (_pulspb_bufnr_order_T*) order.op->get_new_item(&order);
			    pord->bufnr = plit->li_tv.vval.v_number;
			    pord->mru_order = i;
			    ++i;
			}
			plit = plit->li_next;
		    }
		    cmp.fn_compare = &_PulsPb_MruOrderCmp_bufnr;
		    order.op->sort(&order, &cmp);

		    /* 3. Assign mru_order to buffer items; use filter_score for that. */
		    init_SegmentedArrayIterator(&itbufs);
		    ppit = itbufs.op->begin(&itbufs, self->items);
		    init_SegmentedArrayIterator(&itorder);
		    pord = itorder.op->begin(&itorder, &order);
		    while (1)
		    {
			while(ppit && (!pord || ((buf_T*)ppit->data)->b_fnum < pord->bufnr))
			{
			    ppit->filter_score = 65111; /* not in mru => last */
			    ppit = itbufs.op->next(&itbufs);
			}
				
			if (!ppit)
			    break;

			while(pord && pord->bufnr < ((buf_T*)ppit->data)->b_fnum)
			    pord = itorder.op->next(&itorder);

			while(ppit && pord && pord->bufnr == ((buf_T*)ppit->data)->b_fnum)
			{
			    ppit->filter_score = pord->mru_order;
			    ppit = itbufs.op->next(&itbufs);
			    pord = itorder.op->next(&itorder);
			}
		    }
		    order.op->destroy(&order);

		    /* 4. Sort buffers by filter_score (ie. MRU order) */
		    cmp.fn_compare = &_BufferItem_cmp_filter_score;
		}
	    }
	    break;
	default:
	    /* unknown sort mode -> sort by number */
	    self->sorted_by = BUFSORT_NR;
	    cmp.fn_compare = &_BufferItem_cmp_nr;
	    break;
    }

    rv = cmp.fn_compare ? self->op->sort_items(self, &cmp) : 0;
    return rv;
    END_METHOD;
}

    static int
_bprov__index_to_bufnr(_self, index)
    void*   _self;
    int	    index;
    METHOD(BufferItemProvider, _index_to_bufnr);
{
    PopupItem_T* pit;
    buf_T* pbuf;
    pit = self->op->get_item(self, index);
    if (pit && pit->data)
    {
	pbuf = (buf_T*) pit->data;
	return pbuf->b_fnum;
    }
    return -1;
    END_METHOD;
}

    static void
_bprov_update_result(_self, status)
    void*	_self;
    dict_T*	status;
    METHOD(BufferItemProvider, update_result);
{
    dictitem_T* pdi;

    /* convert index to bufnr */
    /* XXX: maybe it would be better to create current-buf and marked-buf */
    pdi = dict_find(status, VSTR("current"), -1L);
    if (pdi && pdi->di_tv.v_type == VAR_NUMBER)
	pdi->di_tv.vval.v_number = self->op->_index_to_bufnr(self, (int)pdi->di_tv.vval.v_number);

    pdi = dict_find(status, VSTR("marked"), -1L);
    if (pdi && pdi->di_tv.v_type == VAR_LIST && pdi->di_tv.vval.v_list)
    {
	list_T* l = pdi->di_tv.vval.v_list;
	listitem_T *pitem = l->lv_first;
	while (pitem != NULL)
	{
	    if (pitem->li_tv.v_type == VAR_NUMBER)
		pitem->li_tv.vval.v_number = self->op->_index_to_bufnr(self, (int)pitem->li_tv.vval.v_number);
	    pitem = pitem->li_next;
	}
    }
    END_METHOD;
}


    static char_u*
_bprov_handle_command(_self, puls, command)
    void*	    _self;
    PopupList_T*    puls;
    char_u*	    command;
    METHOD(BufferItemProvider, handle_command);
{

    if (STARTSWITH(command, "buf-sort"))
    {
	int modified = 0;
	int mode = NUL;
	if (EQUALS(command, "buf-sort-bufnr"))
	    mode = BUFSORT_NR;
	else if (EQUALS(command, "buf-sort-path"))
	    mode = BUFSORT_PATH;
	else if (EQUALS(command, "buf-sort-name"))
	    mode = BUFSORT_NAME;
	else if (EQUALS(command, "buf-sort-ext"))
	    mode = BUFSORT_EXT;
	else if (EQUALS(command, "buf-sort-mru"))
	    mode = BUFSORT_MRU;

	if (mode != NUL)
	{
	    self->sorted_by = mode;
	    modified = self->op->sort_buffers(self);
	    if (modified)
		puls->need_redraw |= PULS_REDRAW_ALL;
	}
    }
    else if (EQUALS(command, "buf-delete") || EQUALS(command, "buf-wipeout") || EQUALS(command, "buf-unload"))
    {
	char_u cbuf[32]; /* XXX: remove from stack */
	PopupItem_T* pit = self->op->get_item(self, puls->current);
	if (pit)
	{
	    buf_T* bi = (buf_T*) pit->data;
	    sprintf((char*)cbuf, "b%s %d", command+4, bi->b_fnum);
	    do_cmdline_cmd(cbuf);
	}
	
	self->op->list_buffers(self); /* TODO: remove the buffer from item list instead of recreating! */
	self->op->sort_buffers(self); /* TODO: if filter is active, sorting should be disabled */
	puls->need_redraw |= PULS_REDRAW_ALL;
    }
    else if (EQUALS(command, "buf-toggle-unlisted"))
    {
	self->show_unlisted = ! self->show_unlisted;
	self->op->list_buffers(self); /* TODO: remove the buffer from item list instead of recreating! */
	self->op->sort_buffers(self); /* TODO: if filter is active, sorting should be disabled */
	puls->need_redraw |= PULS_REDRAW_ALL;
    }
    else
    {
	return super(BufferItemProvider, handle_command)(self, puls, command);
    }

    return NULL;
    END_METHOD;
}

    static void
_bprov_default_keymap(_self, puls)
    void* _self;
    PopupList_T* puls;
    METHOD(ItemProvider, default_keymap);
{
    SimpleKeymap_T* modemap;

    modemap = puls->km_normal;
    modemap->op->set_key(modemap, VSTR("xd"), VSTR("buf-delete"));
    modemap->op->set_key(modemap, VSTR("xu"), VSTR("buf-unload"));
    modemap->op->set_key(modemap, VSTR("xw"), VSTR("buf-wipeout"));
    modemap->op->set_key(modemap, VSTR("ob"), VSTR("buf-sort-bufnr"));
    modemap->op->set_key(modemap, VSTR("on"), VSTR("buf-sort-name"));
    modemap->op->set_key(modemap, VSTR("op"), VSTR("buf-sort-path"));
    modemap->op->set_key(modemap, VSTR("or"), VSTR("buf-sort-mru"));
    modemap->op->set_key(modemap, VSTR("ox"), VSTR("buf-sort-ext"));
    modemap->op->set_key(modemap, VSTR("u"),  VSTR("buf-toggle-unlisted"));

    super(BufferItemProvider, default_keymap)(self, puls);
    END_METHOD;
}
