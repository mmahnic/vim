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
 * popuplst.c: Popup list (PULS)
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

/*                 WORK IN PROGRESS                   */
/*  FIXME: Vim will crash on out-of-memory            */

#include "vim.h"

#if defined(FEAT_POPUPLIST)

#define VSTR(s)   ((char_u*)s)
#define EQUALS(a, b)  (0 == strcmp((char*)a, (char*)b))
#define IEQUALS(a, b)  (0 == STRICMP((char*)a, (char*)b))
#define EQUALSN(a, b, n)  (0 == strncmp((char*)a, (char*)b, n))
#define STARTSWITH(str, prefix)  (0 == strncmp((char*)str, (char*)prefix, strlen((char*)prefix)))
#ifdef FEAT_MBYTE
#define ADVANCE_CHAR_P(p) mb_ptr_adv(p)
#else
#define ADVANCE_CHAR_P(p) ++p
#endif

/* HACK from eval.c */
static dictitem_T dumdi;
#define DI2HIKEY(di) ((di)->di_key)
#define HIKEY2DI(p)  ((dictitem_T *)(p - (dumdi.di_key - (char_u *)&dumdi)))
#define HI2DI(hi)     HIKEY2DI((hi)->hi_key)

/* Required imports from eval.c */
/* TODO: move to a header file */
extern int call_func(char_u *funcname, int len, typval_T *rettv, int argcount_in, typval_T *argvars_in, int (*argv_func)(int, typval_T *, int), linenr_T firstline, linenr_T lastline, int *doesrange, int evaluate, partial_T *partial, dict_T *selfdict_in);
/* extern int call_func (char_u *funcname, int len, typval_T *rettv, int argcount, typval_T *argvars, linenr_T firstline, linenr_T lastline, int *doesrange, int evaluate, dict_T *selfdict); */
extern void dict_unref (dict_T *d);
extern void dictitem_remove (dict_T *dict, dictitem_T *item);

/*
 * Add a dict entry to dictionary "d".
 * Returns FAIL when out of memory and when key already exists.
 * ( based on dict_add_list )
 */
    static int
_dict_add_dict(d, key, dict)
    dict_T	*d;
    char	*key;
    dict_T	*dict;
{
    dictitem_T	*item;

    item = dictitem_alloc((char_u *)key);
    if (item == NULL)
	return FAIL;
    item->di_tv.v_lock = 0;
    item->di_tv.v_type = VAR_DICT;
    item->di_tv.vval.v_dict = dict;
    if (dict_add(d, item) == FAIL)
    {
	dictitem_free(item);
	return FAIL;
    }
    ++dict->dv_refcount;
    return OK;
}

/* Some constants */
static char_u blankline[] = "";
static char_u cmd_quit[] = "quit";
static char_u cmd_accept[] = "accept";

#define DEBUG
/*#define INCLUDE_TESTS*/

#if (defined(INCLUDE_TESTS) || defined(DEBUG))
static char_u str_pulstest[] = "*test*";
static char_u str_pulslog[] = "*log*";
static list_T PULSLOG; /* XXX: adding items to this list will create memory leaks */

    static void
pulslog(char* fmt, ...)
{
    va_list	ap;
    int		str_l;
    static char buf[128+1];
    listitem_T	*item;

    va_start(ap, fmt);
    str_l = vim_vsnprintf(&buf[0], 128, fmt, ap, NULL);
    va_end(ap);

    item = (listitem_T*) alloc(sizeof(listitem_T));
    item->li_tv.v_type = VAR_STRING;
    item->li_tv.vval.v_string = vim_strsave((char_u*)buf);

    /* from list_append */
    if (PULSLOG.lv_last == NULL)
    {
	PULSLOG.lv_first = item;
	PULSLOG.lv_last = item;
	item->li_prev = NULL;
    }
    else
    {
	PULSLOG.lv_last->li_next = item;
	item->li_prev = PULSLOG.lv_last;
	PULSLOG.lv_last = item;
    }
    ++PULSLOG.lv_len;
    item->li_next = NULL;
}
#define LOG( X )   pulslog X
#else
#define LOG( X )
#endif

    static int
limit_value(value, vmin, vmax)
    int value;
    int vmin;
    int vmax;
{
    if (value < vmin)
	value = vmin;
    if (value > vmax)
	value = vmax;
    return value;
}

/*
 * Search for needle in haystack, ignoring case.
 * Doesn't work for multi-byte characters.
 */
    static char_u*
_stristr(haystack, needle)
    char_u* haystack;
    char_u* needle;
{
    char_u* p;
    int hs = STRLEN(haystack);
    int ns = STRLEN(needle);
    if (hs < ns)
	return NULL;

    hs -= ns;
    p = haystack;
    while (hs >= 0)
    {
	if (0 == STRNICMP(p, needle, ns))
	    return p;
	p++;
	hs--;
    }

    return NULL;
}

    static void
_str_assign(dest, src)
    char_u** dest;
    char_u* src;
{
    if (! dest)
	return;
    if (*dest && src && STRLEN(*dest) == STRLEN(src))
    {
	STRCPY(*dest, src);
	return;
    }
    if (*dest)
	vim_free(*dest);
    if (src)
	*dest = vim_strsave(src);
    else
	*dest = NULL;
}

    static void
_str_free(str)
    char_u** str;
{
    if (!str || !*str)
	return;
    vim_free(*str);
    *str = NULL;
}

/* Attribute intialization. Use custom names for the popup list.
 * If a name doesn't exist in the syntax table, use PUM values. */
typedef struct _puls_hl_attrs_T {
    char_u* name;	/* name used in syntax files */
    int     attr;	/* attribute returned by syn_id2attr */

    /* Positive: PUM HLF_xxx values are used to get the default attr values.
     * Negative or zero: index into _puls_hl_attrs, a back-reference. */
    int   default_id;
} _puls_hl_attrs_T;

#define PULSATTR_NORMAL		0
#define PULSATTR_SELECTED	1
#define PULSATTR_TITLE		2
#define PULSATTR_TITLE_SEL	3
#define PULSATTR_MARKED		4
#define PULSATTR_MARKED_SEL	5
#define PULSATTR_DISABLED	6
#define PULSATTR_DISABLED_SEL	7
#define PULSATTR_BORDER		8
#define PULSATTR_SCROLL_BAR	9
#define PULSATTR_SCROLL_THUMB	10
#define PULSATTR_SCROLL_SPACE	11 /* special attr when scrollbar page is rendered with spaces */
#define PULSATTR_INPUT		12
#define PULSATTR_INPUT_ACTIVE	13
#define PULSATTR_SHORTCUT	14
#define PULSATTR_SHORTCUT_SEL	15
#define PULSATTR_HL_FILTER	16
#define PULSATTR_HL_SEARCH	17
#define PULSATTR_HL_USER	18

static _puls_hl_attrs_T _puls_hl_attrs[] = {
    { VSTR("PulsNormal"),	    0, HLF_PNI },
    { VSTR("PulsSelected"),	    0, HLF_PSI },
    { VSTR("PulsTitleItem"),	    0, -PULSATTR_NORMAL },
    { VSTR("PulsTitleItemSel"),	    0, -PULSATTR_SELECTED },
    { VSTR("PulsMarked"),	    0, HLF_PSI },
    { VSTR("PulsMarkedSel"),	    0, -PULSATTR_SELECTED },
    { VSTR("PulsDisabled"),	    0, HLF_PNI },
    { VSTR("PulsDisabledSel"),	    0, -PULSATTR_SELECTED },
    { VSTR("PulsBorder"),	    0, -PULSATTR_NORMAL },
    { VSTR("PulsScrollBar"),	    0, -PULSATTR_BORDER },
    { VSTR("PulsScrollThumb"),	    0, HLF_PST },
    { VSTR("PulsScrollBarSpace"),   0, HLF_PSB },
    { VSTR("PulsInput"),	    0, -PULSATTR_BORDER },
    { VSTR("PulsInputActive"),	    0, HLF_PSI },
    { VSTR("PulsShortcut"),	    0, HLF_PSI },
    { VSTR("PulsShortcutSel"),	    0, HLF_PSI },
    { VSTR("PulsHlFilter"),	    0, HLF_V },
    { VSTR("PulsHlSearch"),	    0, HLF_I },
    { VSTR("PulsHlUser"),	    0, HLF_L }
};

    static void
_update_hl_attrs()
{
    int i, size, id;
    size = sizeof(_puls_hl_attrs) / sizeof(_puls_hl_attrs[0]);
    for (i = 0; i < size; i++)
    {
	id = syn_name2id(_puls_hl_attrs[i].name);
	if (id > 0)
	    _puls_hl_attrs[i].attr = syn_id2attr(id);
	else
	{
	    id = _puls_hl_attrs[i].default_id;
	    if (id > 0)
		_puls_hl_attrs[i].attr = syn_id2attr(id);
	    else
		_puls_hl_attrs[i].attr = _puls_hl_attrs[-id].attr;
	}
    }
}

#if defined(FEAT_POPUPLIST_MENUS) && !defined(FEAT_MENU)
#undef FEAT_POPUPLIST_MENUS
#endif

#include "popupls_.ci" /* created by mmoocc.py from class definitions in [ooc] blocks */
#include "puls_st.c"
#include "puls_tw.c"

/*
 * Providers:
 *  quickfix/location list
 *  buffer lines
 *  DONE: list of vim strings/arbitrary objects
 *  DONE: buffer list
 *     - TODO: handle marked buffers
 *  DONE: menu tree
 *  DONE: quickfix
 *  DONE in vimuiex: filesystem tree
 *  DONE in vimuiex: filesystem flat view (files in subdirs)
 *  WONT DO: list of C strings
 *
 * Interface:
 *  DONE: char_u* get_text(i)
 *     - returns a pointer to item text, NTS
 *     - the pointer can reference a temporary value
 *  int select_item(i)
 *     - DONE: an item was selected by pressing Enter
 *     - if item contains subitems (hierarchical) the function can return 1 (redisplay, new items)
 *     - the provider manages level and path
 *	    int level;			the current level in a hierarchical dataset
 *	    int path[MAX_LEVELS];	the selected item in a hierarchical dataset
 *     - we may want to do something with the path
 *	    char_u* get_path_text()
 *     - DONE: we need a function to move up in the hierarchy
 *	    int select_parent()	returns 1 to redisplay the list (new items)
 *  DONE: (option 'nextcmd') It may take a longer time for the provider to
 *    obtain all the items. In this case the PULS could poll the provider for
 *    new items in fixed intervals.
 *	int get_more_items()
 *	    - 0 - no more items
 *	    - 1 - items appended to the list
 *	    - 2 - the list was modified (maybe it was sorted)
 *  DONE: The provider could respond to some actions:
 *	int exec_action(char_u* action, int current)
 *
 *  DONE: The items in the list can be marked.
 *	XXX: do we want to have marked items that are currently hidden by the filter?
 *
 *  TODO: Add context items (flags ITEM_CONTEXT_BACK, ITEM_CONTEXT_FORWARD)
 *	- handling of context items can be on, off or ignored (treated as normal items)
 *	- when on, the context items are displayed only if the main item is displayed
 *	- the number of displayed context items per normal item can be changed
 *  TODO: display and manage multiple listboxes
 *  TODO: attach a listbox to a Vim window (puls as a special buffer)
 *	- exit the puls main loop immediately but keep the listbox when another
 *	  window is activated
 *	- enter the puls main loop when the window is activated
 *	- add a pointer to the attached puls to the window structure
 *	- render the listbox on window update
 *	- ?? display the puls entry in :ls
 *
 */

/* [ooc]
 *
  class CommandQueueItem [cmdqit]
  {
    CommandQueueItem* next;
    char_u* command;
    void init();
    void destroy();
  }

  class CommandQueue [cmdque]
  {
    CommandQueueItem* _cmd_list_first;
    CommandQueueItem* _cmd_list_last;
    ListHelper*       _commands;
    void    init();
    void    destroy();
    void    add(char_u* command);
    void    pop();
    char_u* head();
  };
*/

    static void
_cmdqit_init(_self)
    void* _self;
    METHOD(CommandQueueItem, init);
{
    self->command = NULL;
    self->next = NULL;
    END_METHOD;
}

    static void
_cmdqit_destroy(_self)
    void* _self;
    METHOD(CommandQueueItem, destroy);
{
    vim_free(self->command);
    END_DESTROY(CommandQueueItem);
}

    static void
_cmdque_init(_self)
    void* _self;
    METHOD(CommandQueue, init);
{
    self->_cmd_list_first = NULL;
    self->_cmd_list_last = NULL;
    self->_commands = new_ListHelper();
    self->_commands->first = (void**) &self->_cmd_list_first;
    self->_commands->last = (void**) &self->_cmd_list_last;
    self->_commands->offs_next = offsetof(CommandQueueItem_T, next);
    self->_commands->fn_destroy = &_cmdqit_destroy;
    END_METHOD;
}

    static void
_cmdque_destroy(_self)
    void* _self;
    METHOD(CommandQueue, destroy);
{
    self->_commands->op->delete_all(self->_commands, NULL);
    CLASS_DELETE(self->_commands);
    END_DESTROY(CommandQueue);
}

    static void
_cmdque_add(_self, command)
    void* _self;
    char_u* command;
    METHOD(CommandQueue, add);
{
    char_u *ps, *pe;
    CommandQueueItem_T* pit;
    if (!command || !*command)
	return;
    ps = command;
    while (isspace(*ps))
	++ps;
    pe = vim_strchr(ps, '|');
    while (pe)
    {
	command = pe + 1; /* next command */
	--pe;
	while (isspace(*pe) && pe > ps)
	    --pe;
	if (pe >= ps)
	{
	    pit = new_CommandQueueItem();
	    pit->command = vim_strnsave(ps, pe-ps+1);
	    self->_commands->op->add_tail(self->_commands, pit);
	}
	ps = command;
	while (isspace(*ps))
	    ++ps;
	pe = vim_strchr(ps, '|');
    }
    pe = (char_u*)strchr((char*)ps, NUL); /* find end of string; *don't* use vim_strchr! */
    --pe;
    while (isspace(*pe) && pe > ps)
	--pe;
    if (pe >= ps)
    {
	pit = new_CommandQueueItem();
	pit->command = vim_strnsave(ps, pe-ps+1);
	self->_commands->op->add_tail(self->_commands, pit);
    }
    END_METHOD;
}

    static char_u*
_cmdque_head(_self)
    void* _self;
    METHOD(CommandQueue, head);
{
    if (! self->_cmd_list_first)
	return NULL;
    return self->_cmd_list_first->command;
    END_METHOD;
}

    static void
_cmdque_pop(_self)
    void* _self;
    METHOD(CommandQueue, pop);
{
    CommandQueueItem_T* pit;
    pit = (CommandQueueItem_T*) self->_commands->op->remove_head(self->_commands);
    if (pit)
	vim_free(pit);
    END_METHOD;
}

/* [ooc]
 *
  const ITEM_SHARED		= 0x01;
  const ITEM_MARKED		= 0x02;
  const ITEM_TITLE		= 0x04;
  const ITEM_DISABLED		= 0x08;
  const ITEM_SEPARATOR		= 0x10;
  const ITEM_CONTEXT_FORWARD	= 0x20;
  const ITEM_CONTEXT_BACK	= 0x40;
  struct PopupItem [ppit]
  {
    void*	data; // additional data for the item
    char_u*	text;

    // @var flags: cached; marked; titleitem; ...
    // when the list is regenerated, the flags are lost
    ushort	flags;
    ushort	filter_start;
    ushort	filter_length;
    ushort	filter_parent_score; // for title items (up to 64k titles should be enough)
    ulong	filter_score;

    void	init();
    void	destroy();
  };
*/

    static void
_ppit_init(_self)
    void* _self;
    METHOD(PopupItem, init);
{
    self->data		= NULL;
    self->text		= NULL;
    self->flags		= 0;
    self->filter_start	= 0;
    self->filter_length	= 65535; /* assume NUL terminated string */
    self->filter_score	= 1;
    self->filter_parent_score = 0;
    END_METHOD;
}

    static void
_ppit_destroy(_self)
    void* _self;
    METHOD(PopupItem, destroy);
{
    if (self->text && !(self->flags & ITEM_SHARED))
    {
	vim_free(self->text);
	self->text = NULL;
    }
    END_DESTROY(PopupItem);
}

/* [ooc]
 *
  class ItemProvider(object) [iprov]
  {
    SegmentedGrowArray* items;
    dict_T*		commands;  // commands defined in vim-script
    char_u*		title;
    int			has_title_items;
    int			has_shortcuts;
    int			out_of_sync;  // if TRUE, the popup-items have to be updated
    NotificationList    title_obsrvrs;

    void	init();
    void	destroy();
    void	read_options(dict_T* options);
    void	on_start();	    // called before the items are displayed for the first time
    void	clear_items();
    void	sync_items();       // to be called when out_of_sync is TRUE
    int		get_item_count();
    PopupItem_T* get_item(int item);

    // Values returned by append_pchar_item should be considered temporary!
    // @param shared=0 => will be free()-d
    PopupItem_T* append_pchar_item(char_u* text, int shared);
    char_u*	get_display_text(int item);

    // Filternig can be implemented with a separate string or with a
    // pointer into the original string and the size of the substring.
    // The part of the string to be filtered is contiguous.
    char_u*	get_filter_text(int item);
    char_u*	get_path_text();
    char_u*	get_title();
    void	set_title(char_u* title);
    void	set_marked(int item, int marked);
    uint	has_flag(int item, uint flag);

    // select_item()
    // @returns:
    //	 0 - let the caller handle it
    //	 1 - remain in the event loop; update the items, they may have changed
    //	-1 - exit the loop; return the result 'done' (executed by the handler)
    int		select_item(int item);

    // select_parent()
    // @returns:
    //   -1 - no parent
    //    i - index of current sublist in the parent list
    int		select_parent();

    // find_shortcut()
    // @returns TRUE if an item with a shortcut was found and fills index and unique.
    // Starts looking from the 'start' item and wraps around.
    int find_shortcut(char_u* qchar_mb, int startidx, int* index, int* unique);

    // @returns TRUE if qsort was called
    int		sort_items(ItemComparator* cmp);

    // the provider may add a extra information to the result or change the existing information
    void	update_result(dict_T* status);

    // the provider handles the command and returns the next command
    char_u*	handle_command(PopupList* puls, char_u* command);
    int		vim_cb_command(PopupList* puls, char_u* command, typval_T* rettv);
    void	_process_vim_cb_result(PopupList* puls, dict_T* options);

    // the provider can provide default keymaps for its commands
    void	default_keymap(PopupList* puls);
  };

*/

    static void
_iprov_init(_self)
    void* _self;
    METHOD(ItemProvider, init);
{
    self->commands = NULL;
    self->title = NULL;
    self->items = new_SegmentedGrowArrayP(sizeof(PopupItem_T), &_ppit_destroy);
    self->has_title_items = 0;
    self->has_shortcuts = 0;
    self->out_of_sync = 0;
    END_METHOD;
}

    static void
_iprov_destroy(_self)
    void* _self;
    METHOD(ItemProvider, destroy);
{
    /* delete cached text from items */
    self->op->clear_items(self);

    self->commands = NULL; /* we don't own them */
    _str_free(&self->title);

    CLASS_DELETE(self->items);

    END_DESTROY(ItemProvider);
}

/* XXX: Options for options
 * The options can be provided by the user every time or they can be remembered
 * between runs.
 * To remember them we could have a static structure that would be created
 * the first time BufferItemProvider is executed.
 * The other possibility is to let the user handle the options for each use-case
 * in vimscript; in this case we pass the command 'option-changed' to the
 * script after an option is changed.
 */
    static void
_iprov_read_options(_self, options)
    void* _self;
    dict_T* options;
    METHOD(ItemProvider, read_options);
{
    END_METHOD;
}

    static void
_iprov_on_start(_self)
    void* _self;
    METHOD(ItemProvider, on_start);
{
    END_METHOD;
}

    static void
_iprov_clear_items(_self)
    void* _self;
    METHOD(ItemProvider, clear_items);
{
    /* clear deletes cached text from items */
    self->items->op->clear(self->items);
    END_METHOD;
}

    static void
_iprov_sync_items(_self)
    void* _self;
    METHOD(ItemProvider, sync_items);
{
    END_METHOD;
}

    static int
_iprov_get_item_count(_self)
    void* _self;
    METHOD(ItemProvider, get_item_count);
{
    return self->items->len;
    END_METHOD;
}

    static PopupItem_T*
_iprov_get_item(_self, item)
    void* _self;
    int item;
    METHOD(ItemProvider, get_item);
{
    if (item < 0 || item >= self->items->len)
	return NULL;
    return (PopupItem_T*) (self->items->op->get_item(self->items, item));
    END_METHOD;
}

    static PopupItem_T*
_iprov_append_pchar_item(_self, text, shared)
    void* _self;
    char_u* text;
    int shared;
    METHOD(ItemProvider, append_pchar_item);
{
    PopupItem_T* pitnew;
    pitnew = self->items->op->get_new_item(self->items);
    if (pitnew)
    {
	init_PopupItem(pitnew);
	pitnew->text = text;
	if (shared)
	    pitnew->flags |= ITEM_SHARED;
	return pitnew;
    }
    return NULL;
    END_METHOD;
}

    static char_u*
_iprov_get_display_text(_self, item)
    void* _self;
    int item;
    METHOD(ItemProvider, get_display_text);
{
    PopupItem_T* pit;
    pit = self->items->op->get_item(self->items, item);
    return pit ? pit->text : NULL;
    END_METHOD;
}

    static char_u*
_iprov_get_filter_text(_self, item)
    void* _self;
    int item;
    METHOD(ItemProvider, get_filter_text);
{
    PopupItem_T* pit;
    pit = self->items->op->get_item(self->items, item);
    if (!pit || !pit->text)
	return NULL;
    return pit->text + pit->filter_start;
    END_METHOD;
}

    static char_u*
_iprov_get_path_text(_self)
    void* _self;
    METHOD(ItemProvider, get_path_text);
{
    return NULL;
    END_METHOD;
}

    static char_u*
_iprov_get_title(_self)
    void* _self;
    METHOD(ItemProvider, get_title);
{
    return self->title;
    END_METHOD;
}

    static void
_iprov_set_title(_self, title)
    void*   _self;
    char_u* title;
    METHOD(ItemProvider, set_title);
{
    _str_assign(&self->title, title);
    self->title_obsrvrs.op->notify(&self->title_obsrvrs, NULL);
    END_METHOD;
}

    static void
_iprov_set_marked(_self, item, marked)
    void* _self;
    int item;
    int marked;
    METHOD(ItemProvider, set_marked);
{
    PopupItem_T* pit;
    pit = self->items->op->get_item(self->items, item);
    if (!pit)
	return;
    if (marked) pit->flags |= ITEM_MARKED;
    else pit->flags &= ~ITEM_MARKED;
    END_METHOD;
}

    static uint
_iprov_has_flag(_self, item, flag)
    void* _self;
    int item;
    uint flag;
    METHOD(ItemProvider, has_flag);
{
    PopupItem_T* pit;
    pit = self->items->op->get_item(self->items, item);
    if (!pit)
	return 0;

    return pit->flags & flag;
    END_METHOD;
}

    static int
_iprov_select_item(_self, item)
    void* _self;
    int item;
    METHOD(ItemProvider, select_item);
{
    return 0; /* a leaf item, execute */
    /* TODO: VimlistItemProvider calls handle_command('itemselected') when it
     * is defined in options. Something similar should be done in
     * select_parent: call handle_command('parentselected') if defined. An
     * alternative is to keep the contents of the list on stack before
     * executing 'itemselected'. */
    END_METHOD;
}

    static int
_iprov_select_parent(_self)
    void* _self;
    METHOD(ItemProvider, select_parent);
{
    return -1; /* no parent, ignore */
    END_METHOD;
}

    static int
_iprov_find_shortcut(_self, qchar_mb, startidx, index, unique)
    void* _self;
    char_u* qchar_mb;
    int startidx;
    int* index;
    int* unique;
    METHOD(ItemProvider, find_shortcut);
{
    int item_count, i, start, end, step, found, len;
    char_u *text, *p;

    item_count = self->op->get_item_count(self);
    start = startidx;
    if (start < 0 || start >= item_count)
	start = 0;
    end = item_count-1;

    len = STRLEN(qchar_mb);
    found = 0;
    for (step = 0; step < 2; ++step)
    {
	for(i = start; i <= end; ++i)
	{
	    text = self->op->get_display_text(self, i);
	    p = text ? vim_strchr(text, '&') : NULL;
	    if (!p)
		continue;
	    ++p;
	    if (0 == STRNICMP(p, qchar_mb, len))
	    {
		if (found)
		{
		    *unique = 0;
		    return 1;
		}
		found = 1;
		*unique = 1;
		*index = i;
	    }
	}
	end = start-1;
	start = 0;
    }
    return found;
    END_METHOD;
}

    static void
_iprov_update_result(_self, status)
    void*	    _self;
    dict_T*	    status;   /* the status of the popup list: selected item, marked items, etc. */
    METHOD(ItemProvider, update_result);
{
    END_METHOD;
}

    static char_u*
_iprov_handle_command(_self, puls, command)
    void*	    _self;
    PopupList_T*    puls;
    char_u*	    command;
    METHOD(ItemProvider, handle_command);
{
    typval_T rettv;

    vim_memset(&rettv, 0, sizeof(typval_T)); /* XXX: init_tv is not accessible */
    self->op->vim_cb_command(self, puls, command, &rettv);

    if (rettv.v_type == VAR_DICT && rettv.vval.v_dict)
    {
	self->op->_process_vim_cb_result(self, puls, rettv.vval.v_dict);
    }
    clear_tv(&rettv);

    return NULL;
    END_METHOD;
}

    static void
_iprov__process_vim_cb_result(_self, puls, options)
    void*	    _self;
    PopupList_T*    puls;
    dict_T*	    options;
    METHOD(ItemProvider, _process_vim_cb_result);
{
    dictitem_T* option;
    if (! options)
	return;
    if (puls)
    {
	option = dict_find(options, VSTR("nextcmd"), -1L);
	if (option && option->di_tv.v_type == VAR_STRING && option->di_tv.vval.v_string)
	    puls->cmds_macro->op->add(puls->cmds_macro, option->di_tv.vval.v_string);

	option = dict_find(options, VSTR("redraw"), -1L);
	if (option && option->di_tv.v_type == VAR_NUMBER && option->di_tv.vval.v_number)
	    puls->need_redraw |= PULS_REDRAW_CLEAR;
    }
    option = dict_find(options, VSTR("title"), -1L);
    if (option && option->di_tv.v_type == VAR_STRING)
	self->op->set_title(self, option->di_tv.vval.v_string);
    END_METHOD;
}

    static int
_iprov_vim_cb_command(_self, puls, command, rettv)
    void*	    _self;
    PopupList_T*    puls;
    char_u*	    command;
    typval_T*	    rettv;
    METHOD(ItemProvider, vim_cb_command);
{
    dictitem_T* icmd;

    if (!self->commands || !command)
	return 0;

    icmd = dict_find(self->commands, command, -1L);
    if (!icmd)
	return 0;

    {
	typval_T    argv[2 + 1]; /* command, status */
	int	    argc = 0;
	int	    dummy;
	dict_T	    *selfdict = NULL;
	char_u	    *fn = icmd->di_tv.vval.v_string;
	dict_T	    *status = NULL;

	status = dict_alloc();
	++status->dv_refcount;
	puls->op->prepare_result(puls, status);

	argv[0].v_type = VAR_STRING;
	argv[0].vval.v_string = command;
	++argc;

	argv[1].v_type = VAR_DICT;
	argv[1].vval.v_dict = status;
	++argc;

	call_func(fn, (int)STRLEN(fn), rettv, argc, argv,
		NULL /* TODO: some function (*argv_func)(int, typval_T *, int) */,
		curwin->w_cursor.lnum, curwin->w_cursor.lnum,
		&dummy, TRUE,
		NULL /* TODO: partial_T *partial */,
		selfdict);

	dict_unref(status);
    }

    return 1;
    END_METHOD;
}

    static void
_iprov_default_keymap(_self, puls)
    void* _self;
    PopupList_T* puls;
    METHOD(ItemProvider, default_keymap);
{
    END_METHOD;
}

    static int
_iprov_sort_items(_self, cmp)
    void* _self;
    ItemComparator_T* cmp;
    METHOD(ItemProvider, sort_items);
{
    int ni;
    ni = self->op->get_item_count(self);
    if (ni < 2)
	return 0;

    self->items->op->sort(self->items, cmp);
    return 1;
    END_METHOD;
}

/* [ooc]
 *
  class VimlistItemProvider(ItemProvider) [vlprov]
  {
    list_T*	vimlist;
    int		_refcount;	// how many times have we referenced the list
    char	_list_lock;	// the state of list->v_lock when popuplist started

    // @var skip_leading is the number of characters to skip in the text
    // returned by get_display_text. The leading characters can be used to pass
    // additional information to each item (eg. title, disabled, marked,
    // hierarchical level). At most 2 leading characters can be used.
    int		skip_leading;

    // @var title_expr is an search string used to select titles in the
    // item list. Each item that matches the search string will be marked
    // as ITEM_TITLE.
    char_u* title_expr;

    void	init();
    void	destroy();
    PopupItem*  _cache_list_item(listitem_T* item);
    void	sync_items();
    void	set_list(list_T* vimlist);
    void	read_options(dict_T* options);
    void	update_titles();
    char_u*	get_display_text(int item);

    // Handle command will call vim_cb_command. We use update_result to add
    // the items to the status passed to vim_cb_command. We make sure that
    // the list is locked. We restore the locked state when the process
    // returns to handle_command.
    void	update_result(dict_T* status);
    char_u*	handle_command(PopupList* puls, char_u* command);
  };
*/

    static void
_vlprov_init(_self)
    void* _self;
    METHOD(VimlistItemProvider, init);
{
    self->vimlist = NULL;
    self->title_expr = NULL;
    self->_refcount = 0;
    self->skip_leading = 0;
    END_METHOD;
}

    static void
_vlprov_destroy(_self)
    void* _self;
    METHOD(VimlistItemProvider, destroy);
{
    if (self->vimlist)
    {
	self->vimlist->lv_lock = self->_list_lock; /* restore the initial lock */
	if (self->_refcount > 0)
	    list_unref(self->vimlist);
	self->_refcount = 0;
	self->vimlist = NULL;
    }
    vim_free(self->title_expr);
    END_DESTROY(VimlistItemProvider);
}

    static void
_vlprov_read_options(_self, options)
    void* _self;
    dict_T* options;
    METHOD(VimlistItemProvider, read_options);
{
    dictitem_T* option;
    super(VimlistItemProvider, read_options)(self, options);

    option = dict_find(options, (char_u*)"titles", -1L);
    if (option && option->di_tv.v_type == VAR_STRING)
    {
	if (option->di_tv.vval.v_string && *option->di_tv.vval.v_string)
	    self->title_expr = vim_strsave(option->di_tv.vval.v_string);
	else
	    _str_free(&self->title_expr);
	self->op->update_titles(self);
    }
    END_METHOD;
}

    static void
_vlprov_update_titles(_self)
    void* _self;
    METHOD(VimlistItemProvider, update_titles);
{
    PopupItem_T *ppit;
    int i, item_count;

    self->has_title_items = 0;
    if (! self->title_expr)
	return;
    item_count = self->op->get_item_count(self);
    for(i = 0; i < item_count; i++)
    {
	ppit = self->op->get_item(self, i);
	if (ppit)
	{
	    /* Check if it's a title. TODO: Use Vim regular expressions. */
	    if (STARTSWITH(ppit->text, self->title_expr))
	    {
		ppit->flags |= ITEM_TITLE;
		self->has_title_items = 1;
	    }
	}
    }
    END_METHOD;
}

    static PopupItem_T*
_vlprov__cache_list_item(_self, pitem)
    void* _self;
    listitem_T* pitem;
    METHOD(VimlistItemProvider, _cache_list_item);
{
    char_u numbuf[NUMBUFLEN];
    PopupItem_T* pit = NULL;
    static char_u str_list[] = "<list>";
    static char_u str_dict[] = "<dict>";
    switch (pitem->li_tv.v_type)
    {
	default:
	    pit = self->op->append_pchar_item(self, blankline, ITEM_SHARED);
	    break;
	/* We assume the list will remain unchanged, and we share the values if possible. */
	case VAR_FUNC:
	case VAR_STRING:
	    if (pitem->li_tv.vval.v_string)
		pit = self->op->append_pchar_item(self, pitem->li_tv.vval.v_string, ITEM_SHARED);
	    else
		pit = self->op->append_pchar_item(self, blankline, ITEM_SHARED);
	    break;
	case VAR_NUMBER:
	    vim_snprintf((char *)numbuf, NUMBUFLEN, "%d", pitem->li_tv.vval.v_number);
	    pit = self->op->append_pchar_item(self, vim_strsave(numbuf), !ITEM_SHARED);
	    break;
#ifdef FEAT_FLOAT
	case VAR_FLOAT:
	    vim_snprintf((char *)numbuf, NUMBUFLEN, "%g", pitem->li_tv.vval.v_float);
	    pit = self->op->append_pchar_item(self, vim_strsave(numbuf), !ITEM_SHARED);
	    break;
#endif
	case VAR_LIST:
	    pit = self->op->append_pchar_item(self, str_list, ITEM_SHARED);
	    break;
	case VAR_DICT:
	    pit = self->op->append_pchar_item(self, str_dict, ITEM_SHARED);
	    break;
    }

    return pit;
    END_METHOD;
}

/* The current popuplist items are removed and replaced with the items from vimlist.
 */
    static void
_vlprov_sync_items(_self)
    void* _self;
    METHOD(VimlistItemProvider, sync_items);
{
    list_T* vimlist = self->vimlist;

    /* clear the items but keep the allocated space */
    self->items->op->clear_contents(self->items);
    self->has_title_items = 0;

    if (vimlist)
    {
	listitem_T *pitem;
	for (pitem = vimlist->lv_first; pitem != NULL; pitem = pitem->li_next)
	    self->op->_cache_list_item(self, pitem);
    }

    /* free the unused space */
    self->items->op->truncate(self->items);
    END_METHOD;
}

/*
 * VimlistItemProvider.set_list(vimlist)
 * Use the vimlist as the source for popuplist items.
 */
    static void
_vlprov_set_list(_self, vimlist)
    void* _self;
    list_T* vimlist;
    METHOD(VimlistItemProvider, set_list);
{
    if (self->vimlist != vimlist)
    {
	if (self->vimlist)
	{
	    self->vimlist->lv_lock = self->_list_lock;
	    if (self->_refcount > 0)
		list_unref(self->vimlist);
	    self->_refcount = 0;
	    self->_list_lock = 0;
	    self->vimlist = NULL;
	}
	if (vimlist)
	{
	    self->vimlist = vimlist;
	    ++vimlist->lv_refcount;
	    self->_refcount = 1;
	    self->_list_lock = vimlist->lv_lock;
	}
    }

    self->op->sync_items(self);
    if (self->title_expr)
	self->op->update_titles(self);
    END_METHOD;
}

    static void
_vlprov_update_result(_self, status)
    void*	_self;
    dict_T*	status;
    METHOD(VimlistItemProvider, update_result);
{
    if (status && self->vimlist)
    {
	dict_add_list(status, "items", self->vimlist);
	self->vimlist->lv_lock |= VAR_LOCKED;
    }
    END_METHOD;
}

    static char_u*
_vlprov_handle_command(_self, puls, command)
    void*	    _self;
    PopupList_T*    puls;
    char_u*	    command;
    METHOD(VimlistItemProvider, handle_command);
{
    PopupItem_T* ppit;
    dictitem_T* option;
    listitem_T *pitem, *pcopy;
    typval_T rettv;
    int must_rebuild, i;

    vim_memset(&rettv, 0, sizeof(typval_T)); /* init_tv is not accessible */
    if ( ! self->op->vim_cb_command(self, puls, command, &rettv))
	return NULL;

    if (self->vimlist)
	self->vimlist->lv_lock = self->_list_lock;

    /* A verification if the list itemes are still valid, to prevent a crash:
     *   check every ITEM_SHARED if it points to the same address as it did initially.
     * If an invalid item is encountered, sync_items has to be called.
     *
     */
    must_rebuild = 0;
    if (self->op->get_item_count(self) != self->vimlist->lv_len)
    {
	LOG(("List size changed. The list must be rebuilt."));
	must_rebuild = 1;
    }
    else
    {
	i = 0;
	for (pitem = self->vimlist->lv_first; pitem != NULL; ++i, pitem = pitem->li_next)
	{
	    ppit = self->op->get_item(self, i);
	    if (!ppit || ((ppit->flags & ITEM_SHARED) && ppit->text != pitem->li_tv.vval.v_string))
	    {
		LOG(("Item %d mismatched. The list must be rebuilt.", i));
		must_rebuild = 1;
		break;
	    }
	}
    }

    /*
     *  TODO: replace-items: list of items that replace the current list
     *    when we replace the items, must_rebuild will be 1, so the above
     *    verification is not necessary.
     *  TODO: if the list was initilally locked, we shouldn't modify it.
     */

    if (rettv.v_type == VAR_DICT && rettv.vval.v_dict)
    {
	self->op->_process_vim_cb_result(self, puls, rettv.vval.v_dict);

	option = dict_find(rettv.vval.v_dict, VSTR("additems"), -1L);
	if (option && option->di_tv.v_type == VAR_LIST)
	{
	    for (pitem = option->di_tv.vval.v_list->lv_first; pitem != NULL; pitem = pitem->li_next)
	    {
		if (list_append_tv(self->vimlist, &pitem->li_tv) != OK)
		    continue;
		if (must_rebuild)
		    continue;
		pcopy = self->vimlist->lv_last;
		ppit = self->op->_cache_list_item(self, pcopy);
		if (ppit)
		{
		    /* XXX: synchronize with code in update_titles() */
		    if (self->title_expr && STARTSWITH(ppit->text, self->title_expr))
		    {
			ppit->flags |= ITEM_TITLE;
			self->has_title_items = 1;
		    }
		}
	    }
	}
    }
    clear_tv(&rettv);

    if (must_rebuild)
    {
	/*
	 * MAYBE: must_rebuild could be a VimlistItemProvider member; we could
	 * apply sync_items right before we would access an item for the first
	 * time after must_rebuild was set.
	 */
	self->op->sync_items(self);
	if (self->title_expr)
	    self->op->update_titles(self);
    }

    /* TODO: apply filter to new items; this should be done by notifying the observers */

    return NULL;
    END_METHOD;
}

    static char_u*
_vlprov_get_display_text(_self, item)
    void* _self;
    int item;
    METHOD(VimlistItemProvider, get_display_text);
{
    PopupItem_T* pit;
    char_u* text;
    pit = self->op->get_item(self, item);
    if (!pit)
	return NULL;
    text = pit->text;
    if (text && self->skip_leading > 0)
    {
	if (*text) text++;
	if (self->skip_leading > 1 && *text) text++;
    }
    return text;
    END_METHOD;
}


#if defined(FEAT_POPUPLIST_BUFFERS)
#include "puls_pb.c"
#endif

#if defined(FEAT_POPUPLIST_MENUS)
#include "puls_pm.c"
#endif

#if defined(FEAT_QUICKFIX)
#include "puls_pq.c"
#endif

/* [ooc]
 *
  struct Box [box] {
    int left;
    int top;
    int width;
    int height;
    void init();
    int right();   // right column
    int bottom();  // bottom row
    void move(int x, int y);
    // [unused] void resize(int width, int height);
  };
*/

    static void
_box_init(_self)
    void* _self;
    METHOD(Box, init);
{
    self->left = 0;
    self->top = 0;
    self->width = 0;
    self->height = 0;
    END_METHOD;
}

    static int
_box_right(_self)
    void* _self;
    METHOD(Box, right);
{
    return self->left + self->width - 1;
    END_METHOD;
}

    static int
_box_bottom(_self)
    void* _self;
    METHOD(Box, bottom);
{
    return self->top + self->height - 1;
    END_METHOD;
}

    static void
_box_move(_self, x, y)
    void* _self;
    int x;
    int y;
    METHOD(Box, move);
{
    self->left = x;
    self->top = y;
    END_METHOD;
}

#if 0
    static void
_box_resize(_self, width, height)
    void* _self;
    int width;
    int height;
    METHOD(Box, resize);
{
    self->width = width;
    self->height = height;
    END_METHOD;
}
#endif

/* [ooc]
 *
  // Basic line edit 'widget'. ATM editing is possible only at the end of line.
  class LineEdit(object) [lned]
  {
    char_u* text;
    int	    size;       // space allocated for text
    int	    len;	// the actual lenght
    int	    max_len;    // max allowed length
    Box     position;   // the position of the line edit on screen (TODO: relative to the parent)
    NotificationList change_obsrvrs;
    void    init();
    void    destroy();
    void    set_text(char_u* ptext);
    int	    add_text(char_u* ptext);
    int	    backspace();
  };
*/

    static void
_lned_init(_self)
    void* _self;
    METHOD(LineEdit, init);
{
    self->text = NULL;
    self->len = 0;
    self->size = 0;
    self->max_len = 0; /* unlimited */
    init_Box(&self->position);
    self->position.height = 1;
    END_METHOD;
}

    static void
_lned_destroy(_self)
    void* _self;
    METHOD(LineEdit, destroy);
{
    _str_free(&self->text);
    END_DESTROY(LineEdit);
}

    static int
_lned_add_text(_self, ptext)
    void* _self;
    char_u* ptext;
    METHOD(LineEdit, add_text);
{
    int nlen = self->len + STRLEN(ptext);

    if (! ptext || ! *ptext)
	return 0;

    if (self->max_len > 0 && nlen > self->max_len)
	return 0;

    if (nlen >= self->size)
    {
	self->size *= 2;
	if (self->size <= nlen)
	    self->size = ((nlen + 1) / 32 + 1) * 32;
	if (! self->text)
	{
	    self->text = (char_u*) alloc(self->size);
	    *self->text = NUL;
	}
	else
	    self->text = (char_u*) vim_realloc(self->text, self->size);
    }

    STRCAT(self->text, ptext);
    self->len = nlen;
    return 1;
    END_METHOD;
}

    static void
_lned_set_text(_self, ptext)
    void* _self;
    char_u* ptext;
    METHOD(LineEdit, set_text);
{
    _str_free(&self->text);
    self->len = 0;
    self->size = 0;
    self->op->add_text(self, ptext);
    END_METHOD;
}

    static int
_lned_backspace(_self)
    void* _self;
    METHOD(LineEdit, backspace);
{
    char_u* last_char;
    if (! self->text || STRLEN(self->text) < 1)
	return 0;

    last_char = self->text + STRLEN(self->text);
    mb_ptr_back(self->text, last_char);
    *last_char = NUL;
    return 1;
    END_METHOD;
}

/* [ooc]
 *
  // Used by the ItemFilter to assign a score to every item.
  // Used by the highlighter to higlight the matches.
  // TODO: The default implementation does a regex search.
  class TextMatcher [txm]
  {
    char_u  mode_char; // a character to display in the border, identifies the matcher
    char_u* _needle;
    int	    _need_strlen;
    ulong   empty_score; // score for empty needle, default is 1
    void    init();
    void    destroy();

    void    set_search_str(char_u* needle);

    // @returns the score of the match or 0 when needle is not in haystack
    ulong   match(char_u* haystack);

    // Init data for the highligter
    void    init_highlight(char_u* haystack);
    // @returns the length of the match (to be highlighted)
    int     get_match_at(char_u* haystack);
  };
 */

    static void
_txm_init(_self)
    void* _self;
    METHOD(TextMatcher, init);
{
    self->mode_char = 'S'; /* simple */
    self->_needle = NULL;
    self->_need_strlen = 0;
    self->empty_score = 1;
    END_METHOD;
}

    static void
_txm_destroy(_self)
    void* _self;
    METHOD(TextMatcher, destroy);
{
    vim_free(self->_needle);
    END_DESTROY(TextMatcher);
}

    static void
_txm_set_search_str(_self, needle)
    void*    _self;
    char_u*  needle;
    METHOD(TextMatcher, set_search_str);
{
    if (self->_needle && needle && EQUALS(self->_needle, needle))
	return;

    vim_free(self->_needle);
    if (needle && *needle)
    {
	self->_needle = vim_strsave(needle);
	self->_need_strlen = STRLEN(needle);
    }
    else
    {
	self->_needle = NULL;
	self->_need_strlen = 0;
    }
    END_METHOD;
}

    static ulong
_txm_match(_self, haystack)
    void* _self;
    char_u* haystack;
    METHOD(TextMatcher, match);
{
    char_u *p, *needle;
    ulong score;
    int d;
    if (! haystack || ! *haystack)
	return 0;
    needle = self->_needle;
    if (! needle || ! *needle)
	return self->empty_score;

    p = _stristr(haystack, needle);
    if (! p)
	return 0;
    score = 1;
    d = p - haystack;
    if (d < 50)
       score += 50 - d;
    /* simplistic start-of-word check */
    if (d == 0 || isalnum(*(p-1)) != isalnum(*p))
	score += 30;
    return score;
    END_METHOD;
}

    static void
_txm_init_highlight(_self, haystack)
    void*	_self;
    char_u*	haystack;
    METHOD(TextMatcher, init_highlight);
{
    END_METHOD;
}

    static int
_txm_get_match_at(_self, haystack)
    void*	_self;
    char_u*	haystack;
    METHOD(TextMatcher, get_match_at);
{
    if (! haystack || ! self->_needle || ! *haystack || ! *self->_needle)
	return 0;

    if (0 == STRNICMP(haystack, self->_needle, self->_need_strlen))
	return self->_need_strlen;

    return 0;
    END_METHOD;
}

/* [ooc]
 *
  class TextMatcherRegexp(TextMatcher) [txmrgxp]
  {
    regmatch_T _regmatch;
    int	       found;

    void    init();
    void    destroy();
    void    set_search_str(char_u* needle);
    ulong   match(char_u* haystack);
    void    init_highlight(char_u* haystack);
    int     get_match_at(char_u* haystack);
  };
*/

    static void
_txmrgxp_init(_self)
    void* _self;
    METHOD(TextMatcherRegexp, init);
{
    self->mode_char = 'R'; /* regexp */
    self->_regmatch.regprog = NULL;
    self->_regmatch.rm_ic = FALSE;
    END_METHOD;
}

    static void
_txmrgxp_destroy(_self)
    void* _self;
    METHOD(TextMatcherRegexp, destroy);
{
    vim_free(self->_regmatch.regprog);
    END_DESTROY(TextMatcherRegexp);
}

    static void
_txmrgxp_set_search_str(_self, needle)
    void* _self;
    char_u* needle;
    METHOD(TextMatcherRegexp, set_search_str);
{
    vim_free(self->_regmatch.regprog);
    self->_regmatch.regprog = NULL;

    super(TextMatcherRegexp, set_search_str)(self, needle);
    if (! self->_needle || ! self->_need_strlen)
	return;

    /* TODO: (eventualy) a user can define a hook to transform _needle into a vim regex */

    ++emsg_skip;
    self->_regmatch.regprog = vim_regcomp(self->_needle, (p_magic ? RE_MAGIC : 0) | RE_STRING );
    --emsg_skip;
    self->_regmatch.rm_ic = p_ic;
    END_METHOD;
}

    static ulong
_txmrgxp_match(_self, haystack)
    void* _self;
    char_u* haystack;
    METHOD(TextMatcherRegexp, match);
{
    if (! self->_regmatch.regprog)
	return 1; /* no (valid) program => everything matches */

    self->found = vim_regexec(&self->_regmatch, haystack, 0);

    return self->found;
    END_METHOD;
}

    static void
_txmrgxp_init_highlight(_self, haystack)
    void* _self;
    char_u* haystack;
    METHOD(TextMatcherRegexp, init_highlight);
{
    if (! self->_regmatch.regprog)
    {
	self->found = FALSE;
	return;
    }

    /* the first match is stored in _regmatch and used in get_match_at */
    self->found = vim_regexec(&self->_regmatch, haystack, 0);
    END_METHOD;
}

    static int
_txmrgxp_get_match_at(_self, haystack)
    void* _self;
    char_u* haystack;
    METHOD(TextMatcherRegexp, get_match_at);
{
    if (!self->_regmatch.regprog || !self->found)
	return 0;

    if (haystack >= self->_regmatch.endp[0])
    {
	/* find next match */
	self->op->init_highlight(self, haystack);
	if (!self->_regmatch.regprog || !self->found)
	    return 0;
    }

    if (haystack < self->_regmatch.startp[0])
	return 0;

    return self->_regmatch.endp[0] - haystack;
    END_METHOD;
}


/* [ooc]
 *
  const TMWME_MAX_WORDS = 16;
  struct TmWordMatchExpr [tmwmxpr]
  {
    TmWordMatchExpr* next;
    // words are stored in (a copy of) _needle
    // max 16 words per group to avoid reallocation; we could use a small segmented grow array, instead
    char_u** not_words;
    int	     not_count;
    char_u** yes_words;
    int	     yes_count;
    void    init();
    void    destroy();
    void    add_word_start(char_u* _word, int yesno);
  };

  // Find all (space) delimited words.
  // Words preceeded by '-' must not be in the match. (alternative: '!')
  // Later AND space-delimited, OR '|' delimited.
  class TextMatcherWords(TextMatcher) [txmwrds]
  {
    char_u*	     _str_words;  // a modified copy of _needle (with NUL characters)
    TmWordMatchExpr* expressions; // list of OR-ed expression
    ListHelper*      lst_expr;

    void    init();
    void    destroy();
    void    clear_words();
    void    set_search_str(char_u* needle);
    ulong   match(char_u* haystack);
    void    init_highlight(char_u* haystack);
    int     get_match_at(char_u* haystack);
  };
*/

    static void
_tmwmxpr_init(_self)
    void* _self;
    METHOD(TmWordMatchExpr, init);
{
    self->next = NULL;
    self->not_count = 0;
    self->yes_count = 0;
    self->not_words = NULL;
    self->yes_words = NULL;
    END_METHOD;
}

    static void
_tmwmxpr_destroy(_self)
    void* _self;
    METHOD(TmWordMatchExpr, destroy);
{
    vim_free(self->not_words);
    vim_free(self->yes_words);
    END_METHOD;
}

    static void
_tmwmxpr_add_word_start(_self, _word, yesno)
    void* _self;
    char_u* _word;
    int yesno;
    METHOD(TmWordMatchExpr, add_word_start);
{
    if (yesno)
    {
	if (! self->yes_words)
	    self->yes_words = (char_u**) alloc(sizeof(char_u*) * TMWME_MAX_WORDS);
	if (self->yes_count < TMWME_MAX_WORDS)
	{
	    self->yes_words[self->yes_count] = _word;
	    ++self->yes_count;
	}
    }
    else
    {
	if (! self->not_words)
	    self->not_words = (char_u**) alloc(sizeof(char_u*) * TMWME_MAX_WORDS);
	if (self->not_count < TMWME_MAX_WORDS)
	{
	    self->not_words[self->not_count] = _word;
	    ++self->not_count;
	}
    }
    END_METHOD;
}

    static void
_txmwrds_init(_self)
    void* _self;
    METHOD(TextMatcherWords, init);
{
    self->mode_char = 'W'; /* words */
    self->_str_words = NULL;
    self->expressions = NULL;
    self->lst_expr = new_ListHelper();
    self->lst_expr->fn_destroy = &_tmwmxpr_destroy;
    self->lst_expr->first = (void**) &self->expressions;
    self->lst_expr->offs_next = offsetof(TmWordMatchExpr_T, next);
    END_METHOD;
}

    static void
_txmwrds_destroy(_self)
    void* _self;
    METHOD(TextMatcherWords, destroy);
{
    self->op->clear_words(self);
    CLASS_DELETE(self->lst_expr);
    END_METHOD;
}

    static void
_txmwrds_clear_words(_self)
    void* _self;
    METHOD(TextMatcherWords, clear_words);
{
    self->lst_expr->op->delete_all(self->lst_expr, NULL /* no condition => all */);
    vim_free(self->_str_words);
    self->_str_words = NULL;
    END_METHOD;
}

    static void
_txmwrds_set_search_str(_self, needle)
    void*    _self;
    char_u*  needle;
    METHOD(TextMatcherWords, set_search_str);
{
    int i, wordstart, notword;
    char_u* p;
    TmWordMatchExpr_T* pexpr;

    self->op->clear_words(self);
    super(TextMatcherWords, set_search_str)(self, needle);
    if (! self->_needle || ! self->_need_strlen)
	return;

    self->_str_words = vim_strsave(self->_needle);
    p = self->_str_words;
    pexpr = NULL;
    wordstart = 1;
    for(i = 0; i < self->_need_strlen; ++p, ++i)
    {
	if (*p == NUL)
	    break;
	if (isspace(*p))
	{
	    *p = NUL;
	    wordstart = 1;
	    continue;
	}
	if (*p == '|')
	{
	    *p = NUL;
	    pexpr = NULL;
	    wordstart = 1;
	    continue;
	}
	if (wordstart)
	{
	    if (!pexpr)
	    {
		pexpr = new_TmWordMatchExpr();
		self->lst_expr->op->add_tail(self->lst_expr, pexpr);
	    }
	    wordstart = 0;
	    notword = (*p == '-');
	    if (*p == '-' || *p == '+')
	    {
		++p;
		if (*p == NUL)
		    break;
	    }
	    if (! isspace(*p) && *p != '|')
		_tmwmxpr_add_word_start(pexpr, p, !notword);
	}
    }
    END_METHOD;
}

    static ulong
_txmwrds_match(_self, haystack)
    void* _self;
    char_u* haystack;
    METHOD(TextMatcherWords, match);
{
    TmWordMatchExpr_T* pexpr;
    int i, notword, score, total_score, d;
    char_u* p;
    if (! self->_str_words)
	return 1;

    pexpr = self->expressions;
    while(pexpr)
    {
	/* find any not_word */
	notword = 0;
	for (i = 0; i < pexpr->not_count; i++)
	{
	    if (! *pexpr->not_words[i]) /* empty word */
		continue;
	    p = _stristr(haystack, pexpr->not_words[i]);
	    if (p)
	    {
		pexpr = pexpr->next;
		notword = 1;
		break;
	    }
	}
	if (notword)
	    continue;

	/* find all yeswords */
	total_score = 0;
	notword = 0;
	for (i = 0; i < pexpr->yes_count; i++)
	{
	    p = pexpr->yes_words[i];
	    if (! *p) /* empty word */
		continue;
	    p = _stristr(haystack, p);
	    if (! p)
	    {
		notword = 1;
		break;
	    }
	    d = p - haystack;
	    if (d > 100)
		d = 100;
	    score =  101 - d;
	    /* simplistic start-of-word check */
	    if (d == 0 || isalnum(*(p-1)) != isalnum(*p))
		score += 30;
	    /* weigh the words by the order they were defined in needle */
	    if (i < 10)
		score = score * (20 - i) / 10;
	    total_score += score;
	}
	if (! notword)
	    return total_score + pexpr->not_count * 15;

	pexpr = pexpr->next;
    }

    return 0;
    END_METHOD;
}

    static void
_txmwrds_init_highlight(_self, haystack)
    void* _self;
    char_u* haystack;
    METHOD(TextMatcherWords, init_highlight);
{
    END_METHOD;
}

    static int
_txmwrds_get_match_at(_self, haystack)
    void* _self;
    char_u* haystack;
    METHOD(TextMatcherWords, get_match_at);
{
    TmWordMatchExpr_T* pexpr;
    int i, n;
    if (! self->_str_words)
	return 0;

    /* find *ANY* yes_words */
    /* TODO: maybe we should match only words from the actual matching subexpression */
    pexpr = self->expressions;
    while(pexpr)
    {
	for (i = 0; i < pexpr->yes_count; i++)
	{
	    if (! *pexpr->yes_words[i]) /* empty word */
		continue;
	    n = STRLEN(pexpr->yes_words[i]);
	    if (0 == STRNICMP(haystack, pexpr->yes_words[i], n))
		return n;
	}
	pexpr = pexpr->next;
    }

    return 0;
    END_METHOD;
}

/* [ooc]
 *
  // A text matcher similar to Command-T
  class TextMatcherCmdT(TextMatcher) [txmcmdt]
  {
    // The position up to which we try to improve the score of the match.
    // -1 : try all possibilities (default)
    //  0 : accept the firts match
    //  n : try to improve the score if a matched character is before this point in string
    int     last_retry_offset;

    // TODO: scores for preceeding special characters
    // char_u specials[];

    // needle parameters used in search, updated in set_search_str
    int		_need_len;
    char_u**	_need_chars;
    short*	_need_char_lens;
    char_u**	_hays_positions;
    char_u**	_hays_best_positions;

    void    init();
    void    destroy();
    void    _clear();
    // Matching is a 2 step process: set_search_str(needle), match(haystack).
    // In some matchers a lot of data has to be calculated for needle on every call!
    void    set_search_str(char_u* needle);
    ulong   match(char_u* haystack);
    ulong   _calc_pos_score(char_u* haystack, char_u** positions, int npos);
    void    init_highlight(char_u* haystack);
    int     get_match_at(char_u* haystack);
  };
 */

    static void
_txmcmdt_init(_self)
    void* _self;
    METHOD(TextMatcherCmdT, init);
{
    self->mode_char = 'T'; /* command-t */
    self->last_retry_offset = -1;
    self->_need_len = 0;
    self->_need_chars = NULL;
    self->_need_char_lens = NULL;
    self->_hays_positions = NULL;
    self->_hays_best_positions = NULL;
    END_METHOD;
}

    static void
_txmcmdt_destroy(_self)
    void* _self;
    METHOD(TextMatcherCmdT, destroy);
{
    self->op->_clear(self);
    END_DESTROY(TextMatcherCmdT);
}

    static void
_txmcmdt__clear(_self)
    void* _self;
    METHOD(TextMatcherCmdT, _clear);
{
    self->_need_len = 0;
    vim_free(self->_need_chars);
    self->_need_chars = NULL;
    vim_free(self->_need_char_lens);
    self->_need_char_lens = NULL;
    vim_free(self->_hays_positions);
    self->_hays_positions = NULL;
    vim_free(self->_hays_best_positions);
    self->_hays_best_positions = NULL;
    END_METHOD;
}

    static void
_txmcmdt_set_search_str(_self, needle)
    void*    _self;
    char_u*  needle;
    METHOD(TextMatcherCmdT, set_search_str);
{
    char_u *p, *pend;
    int l;

    if (self->_needle && needle && EQUALS(self->_needle, needle))
	return;

    self->op->_clear(self);
    super(TextMatcherCmdT, set_search_str)(self, needle);

    if (!self->_needle)
	return;

    self->_need_chars = (char_u**) alloc(sizeof(char_u*) * self->_need_strlen);
    self->_need_char_lens = (short*) alloc(sizeof(short) * self->_need_strlen);
    p = self->_needle;
    pend = p + self->_need_strlen;
    l = 0;
    while (p < pend)
    {
	self->_need_chars[l] = p;
	self->_need_char_lens[l] = mb_ptr2len(p); /* XXX: combining chars cause problems */
	p += self->_need_char_lens[l]; /* mb_ptr_adv(p); */
	++l;
    }
    self->_need_len = l;
    self->_hays_positions = (char_u**) alloc(sizeof(char_u*) * self->_need_len);
    self->_hays_best_positions = (char_u**) alloc(sizeof(char_u*) * self->_need_len);
    END_METHOD;
}

    static ulong
_txmcmdt__calc_pos_score(_self, haystack, positions, npos)
    void*    _self;
    char_u*  haystack;
    char_u** positions;
    int	     npos;
    METHOD(TextMatcherCmdT, _calc_pos_score);
{
    ulong   score;
    int	    i, d;
    char_u  *p;
    char_u  prevch;
    /* TODO: this is an option; also add a field with score-per-char; only ascii chars. */
    static char_u special[] = "/.-_ 0123456789";

    if (npos < 1)
	return 0;

    score = 0;
    for (i = 0; i < npos; i++)
    {
	p = positions[i];
	d = p - haystack;
	if (d < 200)
	    score += 200 - d;
	d = p - ( (i == 0) ? haystack : positions[i-1] ); /* not reliable with wide-chars */
	if (d < 2)
	    score += 1000;
	else
	{
	    prevch = *(p-1); /* (p == haystack ? ' ' : *(p-1)); */
	    p = vim_strchr(special, prevch);
	    if (p)
		score += 800 - (p - special) * 10;
	    else
		score += 1000 / d + 1;
	}
	/* TODO: cmdt - add score if cur is upper and prev is lower */

    }
    return score;
    END_METHOD;
}

    static char_u*
_find_char(text, ch)
    char_u* text;
    char_u* ch;
{
    /* TODO: handle multibyte; handle case */
    return vim_strchr(text, *ch);
}

    static char_u*
_rfind_char(text, ch)
    char_u* text;
    char_u* ch;
{
    /* TODO: handle multibyte; handle case */
    return vim_strrchr(text, *ch);
}

    static ulong
_txmcmdt_match(_self, haystack)
    void* _self;
    char_u* haystack;
    METHOD(TextMatcherCmdT, match);
{
    int		stack_pos;
    char_u	*first_pos, *last_pos, *last_retry_pos;
    char_u	*p;
    ulong	score, best_score;
    /* copies of members */
    int		need_len;
    char_u**	hays_positions;
    char_u**	need_chars;

    if (! haystack || ! *haystack)
	return 0;
    if (! self->_needle || ! *self->_needle)
	return self->empty_score;

    need_len = self->_need_len;
    need_chars = self->_need_chars;
    hays_positions = self->_hays_positions;

    /* find the limits of the search */
    first_pos = _find_char(haystack, need_chars[0]);
    if (! first_pos)
	return 0;

    last_pos = _rfind_char(first_pos, need_chars[need_len-1]);
    if (! last_pos || (last_pos - first_pos + self->_need_char_lens[need_len-1]) < self->_need_strlen)
	return 0; /* too short, a match is not possible */

    last_retry_pos = last_pos + 1; /* retry every possible combination */
    if (self->last_retry_offset >= 0)
    {
	p = haystack + self->last_retry_offset;
	if (p < last_retry_pos)  /* retry less */
	    last_retry_pos = p;
    }

    hays_positions[0] = first_pos;
    best_score = 0;
    if (need_len < 2)
	stack_pos = 0;
    else
    {
	hays_positions[1] = first_pos;
	ADVANCE_CHAR_P(hays_positions[1]);
	stack_pos = 1;
    }

    /* find best match */
    while (stack_pos >= 0)
    {
	p = _find_char(hays_positions[stack_pos], need_chars[stack_pos]);
	if (p > last_pos)
	    p = NULL;
	if (! p)
	{
	    --stack_pos;
	    if (stack_pos >= 0)
		ADVANCE_CHAR_P(hays_positions[stack_pos]);
	    continue;
	}
	hays_positions[stack_pos] = p;
	if (stack_pos < need_len - 1)
	{
	    ++stack_pos;
	    hays_positions[stack_pos] = hays_positions[stack_pos-1];
	    ADVANCE_CHAR_P(hays_positions[stack_pos]);
	    continue;
	}
	else
	{
	    /* We have a match so we calculate the score */
	    score = self->op->_calc_pos_score(self, haystack, hays_positions, need_len);
	    if (score > best_score)
	    {
		best_score = score;
		memcpy(self->_hays_best_positions, hays_positions, sizeof(char_u*) * need_len);
	    }

	    /* try to improve the score */
	    if (self->last_retry_offset < 0)
	    {
		/* always retry */
		ADVANCE_CHAR_P(hays_positions[stack_pos]);
	    }
	    else
	    {
		/* retry only positions that are not past the last_retry_pos */
		while (stack_pos >= 0 && hays_positions[stack_pos] >= last_retry_pos)
		    --stack_pos;
		if (stack_pos >= 0)
		    ADVANCE_CHAR_P(hays_positions[stack_pos]);
	    }
	}
    }

    return best_score;
    END_METHOD;
}

    static void
_txmcmdt_init_highlight(_self, haystack)
    void*	_self;
    char_u*	haystack;
    METHOD(TextMatcherCmdT, init_highlight);
{
    /* find a match to set _hays_positions */
    self->op->match(self, haystack);
    END_METHOD;
}

    static int
_txmcmdt_get_match_at(_self, haystack)
    void*	_self;
    char_u*	haystack;
    METHOD(TextMatcherCmdT, get_match_at);
{
    int i, len;
    char_u**	bestpos = self->_hays_best_positions;
    if (! bestpos || self->_need_len < 1
	    || haystack < bestpos[0]
	    || haystack > bestpos[self->_need_len-1])
    {
	return 0;
    }

    i = 0;
    while (bestpos[i] + self->_need_char_lens[i] - 1 < haystack)
	++i;
    if (bestpos[i] > haystack)
	return 0;
    len = self->_need_char_lens[i] - (haystack - bestpos[i]);
    if (len <= 0)
	return 0;
    /* TODO: increase len while _hays_positions[++i] points to next character */

    return len;
    END_METHOD;
}

/* [ooc]
 *
  typedef void* (*NewObject_Fn)(void);

  class TextMatcherFactoryEntry [tmfent]
  {
    TextMatcherFactoryEntry* next;
    char_u* name;
    NewObject_Fn fn_new;
    void init();
    void destroy();
    void set(char_u* name, NewObject_Fn fn_new);
  };
*/

    static void
_tmfent_init(_self)
    void* _self;
    METHOD(TextMatcherFactoryEntry, init);
{
    self->next = NULL;
    self->name = NULL;
    self->fn_new = NULL;
    END_METHOD;
}

    static void
_tmfent_destroy(_self)
    void* _self;
    METHOD(TextMatcherFactoryEntry, destroy);
{
    vim_free(self->name);
    END_METHOD;
}

    static void
_tmfent_set(_self, name, fn_new)
    void* _self;
    char_u* name;
    NewObject_Fn fn_new;
    METHOD(TextMatcherFactoryEntry, set);
{
    vim_free(self->name);
    self->name = vim_strsave(name);
    self->fn_new = fn_new;
    END_METHOD;
}

/* [ooc]
 *
  class TextMatcherFactory(object) [txmfac]
  {
    TextMatcherFactoryEntry* _entries;
    ListHelper* _lst_entries;
    void    init();
    void    destroy();
    TextMatcher* create_matcher(char_u* name);
    char_u* next_matcher(char_u* name);
  };
*/

    static void
_txmfac_init(_self)
    void* _self;
    METHOD(TextMatcherFactory, init);
{
    TextMatcherFactoryEntry_T *pme;
    self->_entries = NULL;
    self->_lst_entries = new_ListHelper();
    self->_lst_entries->first = (void**) &self->_entries;
    self->_lst_entries->offs_next = offsetof(TextMatcherFactoryEntry_T, next);
    self->_lst_entries->fn_destroy = &_tmfent_destroy;

    pme = new_TextMatcherFactoryEntry();
    pme->op->set(pme, VSTR("simple"), (NewObject_Fn) &new_TextMatcher);
    self->_lst_entries->op->add_tail(self->_lst_entries, pme);

    pme = new_TextMatcherFactoryEntry();
    pme->op->set(pme, VSTR("words"), (NewObject_Fn) &new_TextMatcherWords);
    self->_lst_entries->op->add_tail(self->_lst_entries, pme);

    pme = new_TextMatcherFactoryEntry();
    pme->op->set(pme, VSTR("regexp"), (NewObject_Fn) &new_TextMatcherRegexp);
    self->_lst_entries->op->add_tail(self->_lst_entries, pme);

    pme = new_TextMatcherFactoryEntry();
    pme->op->set(pme, VSTR("sparse"), (NewObject_Fn) &new_TextMatcherCmdT);
    self->_lst_entries->op->add_tail(self->_lst_entries, pme);
    END_METHOD;
}

    static void
_txmfac_destroy(_self)
    void* _self;
    METHOD(TextMatcherFactory, destroy);
{
    self->_lst_entries->op->delete_all(self->_lst_entries, NULL);
    CLASS_DELETE(self->_lst_entries);
    END_DESTROY(TextMatcherFactory);
}

    static TextMatcher_T*
_txmfac_create_matcher(_self, name)
    void* _self;
    char_u* name;
    METHOD(TextMatcherFactory, create_matcher);
{
    TextMatcherFactoryEntry_T *pme;
    if (!name)
	return (TextMatcher_T*) new_TextMatcher();

    pme = self->_entries;
    while (pme && ! IEQUALS(pme->name, name))
	pme = pme->next;

    if (pme)
	return pme->fn_new();

    return NULL;
    END_METHOD;
}

/* TODO: the current matchers are returned in state so that the caller can restore them on next call. */
    static char_u*
_txmfac_next_matcher(_self, name)
    void* _self;
    char_u* name;
    METHOD(TextMatcherFactory, next_matcher);
{
    TextMatcherFactoryEntry_T *pme;
    pme = self->_entries;
    if (!name && pme)
	return pme->name;

    while (pme && ! IEQUALS(pme->name, name))
	pme = pme->next;
    if (pme && pme->next)
	pme = pme->next;
    else
	pme = self->_entries;

    if (pme)
	return pme->name;

    return NULL;
    END_METHOD;
}


/* [ooc]
 *
  const MAX_FILTER_SIZE = 127;
  class ISearch(object) [isrch]
  {
    char_u  text[MAX_FILTER_SIZE + 1];
    int	    start;    // start index for incremental search
    TextMatcher* matcher;
    void    init();
    void    destroy();
    void    set_matcher(TextMatcher* pmatcher);
    void    set_text(char_u* ptext);
    int	    match(char_u* ptext);
  };
*/

    static void
_isrch_init(_self)
    void* _self;
    METHOD(ISearch, init);
{
    self->text[0] = NUL;
    self->start = 0;
    self->matcher = (TextMatcher_T*) new_TextMatcher();
    END_METHOD;
}

    static void
_isrch_destroy(_self)
    void* _self;
    METHOD(ISearch, destroy);
{
    CLASS_DELETE(self->matcher);
    END_DESTROY(ISearch);
}

    static void
_isrch_set_matcher(_self, pmatcher)
    void* _self;
    TextMatcher_T* pmatcher;
    METHOD(ISearch, set_matcher);
{
    if (pmatcher == self->matcher)
	return;
    CLASS_DELETE(self->matcher);
    self->matcher = pmatcher;
    if (self->matcher)
	self->matcher->op->set_search_str(self->matcher, self->text);
    END_METHOD;
}

    static void
_isrch_set_text(_self, ptext)
    void* _self;
    char_u* ptext;
    METHOD(ISearch, set_text);
{
    if (! ptext)
	*self->text = NUL;
    else
    {
	STRNCPY(self->text, ptext, MAX_FILTER_SIZE);
	self->text[MAX_FILTER_SIZE] = NUL;
    }
    if (self->matcher)
	self->matcher->op->set_search_str(self->matcher, self->text);
    END_METHOD;
}

    static int
_isrch_match(_self, haystack)
    void* _self;
    char_u* haystack;
    METHOD(ISearch, match);
{
    if (! self->matcher)
	return 0;
    return self->matcher->op->match(self->matcher, haystack);
    END_METHOD;
}

/* [ooc]
 *
  class FltComparator_Score(ItemComparator) [flcmpscr]
  {
    ItemProvider* model;
    void  init();
    int   compare(void* pia, void* pib);
  };

  class FltComparator_TitleScore(ItemComparator) [flcmpttsc]
  {
    ItemProvider* model;
    void  init();
    int   compare(void* pia, void* pib);
  };

  // Filter items into an index field. A TextMatcher gives each item a score.
  // Items are sorted by score. The original item order is preserved for the items
  // with the same score. If the item list contains title items, the titles are
  // ordered by the highest score of their children. The children below each title
  // are sorted by score.
  //
  class ItemFilter(object) [iflt]
  {
    ItemProvider* model;
    char_u  text[MAX_FILTER_SIZE + 1];
    TextMatcher* matcher;
    SegmentedGrowArray* items; // indices of items in the model

    // @var keep_titles defines how titles are treated after filtering.
    // 0 - Titles are treated as normal items; items are sorted by score.
    // 1 - Show titles with matching 'child' items and hide other titles.
    //     Titles are sorted by the score of the highest scored child item.
    //     Child items are displayed after the appropriate title, sorted by score.
    // TODO: Put keep_titles also in options.
    int	    keep_titles;

    void    init();
    void    destroy();
    void    set_matcher(TextMatcher* pmatcher);
    void    set_text(char_u* ptext);
    void    filter_items();
    int	    get_item_count();
    int	    is_active();

    // get the model-index for index-th item in filtered items
    int	    get_model_index(int index);

    // get the index of model_index in filtred items or -1 if not there
    int	    get_index_of(int model_index);
  };
*/

    static void
_flcmpscr_init(_self)
    void* _self;
    METHOD(FltComparator_Score, init);
{
    self->model = NULL;
    END_METHOD;
}

    static int
_flcmpscr_compare(_self, pia, pib)
    void* _self;
    void* pia;
    void* pib;
    METHOD(FltComparator_Score, compare);
{
    /* pia and pib are pointers to indices of items in the model */
    PopupItem_T *pa, *pb;
    if (!self->model)
	return 0;
    pa = self->model->op->get_item(self->model, *(int*)pia);
    pb = self->model->op->get_item(self->model, *(int*)pib);
    if (!pa)
    {
	if(!pb)
	    return 0;
	return 1;
    }
    else if (!pb)
	return -1;
    if (pa->filter_score < pb->filter_score) return self->reverse ? 1 : -1;
    if (pa->filter_score > pb->filter_score) return self->reverse ? -1 : 1;

    /* Sort items with the same score by the original sorting order; ignore self->reverse */
    if (*(int*) pia < *(int*) pib) return -1;
    if (*(int*) pia > *(int*) pib) return 1;

    return 0;
    END_METHOD;
}

    static void
_flcmpttsc_init(_self)
    void* _self;
    METHOD(FltComparator_TitleScore, init);
{
    self->model = NULL;
    END_METHOD;
}

    static int
_flcmpttsc_compare(_self, pia, pib)
    void* _self;
    void* pia;
    void* pib;
    METHOD(FltComparator_TitleScore, compare);
{
    /* pia and pib are pointers to indices of items in the model */
    PopupItem_T *pa, *pb;
    if (!self->model)
	return 0;
    pa = self->model->op->get_item(self->model, *(int*)pia);
    pb = self->model->op->get_item(self->model, *(int*)pib);
    if (!pa)
    {
	if(!pb)
	    return 0;
	return 1;
    }
    else if (!pb)
	return -1;

    if (pa->filter_parent_score < pb->filter_parent_score) return self->reverse ? 1 : -1;
    if (pa->filter_parent_score > pb->filter_parent_score) return self->reverse ? -1 : 1;

    /* we have to ignore self->reverse for children and sort them from high to
     * low score, otherwise the parent (title) would come after its children */
    if (pa->filter_score < pb->filter_score) return  1;
    if (pa->filter_score > pb->filter_score) return -1;

    /* Sort items with the same score by the original sorting order; ignore self->reverse */
    if (*(int*) pia < *(int*) pib) return -1;
    if (*(int*) pia > *(int*) pib) return 1;

    return 0;
    END_METHOD;
}

    static void
_iflt_init(_self)
    void* _self;
    METHOD(ItemFilter, init);
{
    self->model = NULL;
    self->text[0] = NUL;
    self->items = new_SegmentedGrowArrayP(sizeof(int), NULL);
    self->matcher = (TextMatcher_T*) new_TextMatcherWords();
    self->keep_titles = 1;
    END_METHOD;
}

    static void
_iflt_destroy(_self)
    void* _self;
    METHOD(ItemFilter, destroy);
{
    self->model = NULL; /* filter doesn't own the model */
    CLASS_DELETE(self->items);
    CLASS_DELETE(self->matcher);
    END_DESTROY(ItemFilter);
}

    static void
_iflt_set_matcher(_self, pmatcher)
    void* _self;
    TextMatcher_T* pmatcher;
    METHOD(ItemFilter, set_matcher);
{
    if (pmatcher == self->matcher)
	return;
    CLASS_DELETE(self->matcher);
    self->matcher = pmatcher;
    if (self->matcher)
	self->matcher->op->set_search_str(self->matcher, self->text);
    END_METHOD;
}

    static void
_iflt_set_text(_self, ptext)
    void* _self;
    char_u* ptext;
    METHOD(ItemFilter, set_text);
{
    if (! ptext)
	*self->text = NUL;
    else
    {
	STRNCPY(self->text, ptext, MAX_FILTER_SIZE);
	self->text[MAX_FILTER_SIZE] = NUL;
    }
    if (self->matcher)
	self->matcher->op->set_search_str(self->matcher, self->text);
    END_METHOD;
}

    static int
_iflt_is_active(_self)
    void* _self;
    METHOD(ItemFilter, is_active);
{
    return *self->text != NUL;
    END_METHOD;
}

    static void
_iflt_filter_items(_self)
    void* _self;
    METHOD(ItemFilter, filter_items);
{
    ItemProvider_T *pmodel;
    PopupItem_T* pit;
    TextMatcher_T* matcher;
    FltComparator_Score_T* pcmp;
    SegmentedGrowArray_T* title_items;
    int item_count, i, handle_titles;
    int *pmi;
    ulong score;

    self->items->op->clear(self->items);
    if (STRLEN(self->text) < 1 || !self->matcher)
	return;

    pcmp = NULL;
    pmodel = self->model;
    matcher = self->matcher;
    handle_titles = pmodel->has_title_items;
    item_count = pmodel->op->get_item_count(pmodel);
    for(i = 0; i < item_count; i++)
    {
	if (handle_titles && !self->keep_titles && pmodel->op->has_flag(pmodel, i, ITEM_TITLE))
	    score = 0;
	else
	    score = matcher->op->match(matcher, pmodel->op->get_filter_text(pmodel, i));
	pit = pmodel->op->get_item(pmodel, i); /* TODO: set-item-score() */
	if (pit)
	    pit->filter_score = score;
	if (score <= 0)
	    continue;

	pmi = (int*) self->items->op->get_new_item(self->items);
	if (pmi)
	    *pmi = i;
    }

    if (! handle_titles || ! self->keep_titles)
    {
	/* TODO: an option to sort by score or to keep the original order */
	if (!pcmp)
	    pcmp = new_FltComparator_Score();
	pcmp->model = self->model;
	pcmp->reverse = 1;
	self->items->op->sort(self->items, (ItemComparator_T*)pcmp);
	CLASS_DELETE(pcmp);

	return;
    }

    /* Title items require special processing. They are visible only if they
     * have any children. A title item gets the score of the best-scored child. */
    score = 0;
    title_items = new_SegmentedGrowArrayP(sizeof(int), NULL);
    for (i = item_count-1; i >= 0; --i)
    {
	pit = pmodel->op->get_item(pmodel, i);
	if (! pit)
	    continue;
	if (pmodel->op->has_flag(pmodel, i, ITEM_TITLE))
	{
	    if (pit->filter_score <= 0 && score > 0)
	    {
		/* a title item has to be displayed because a child matched */
		pmi = (int*) self->items->op->get_new_item(self->items);
		if (pmi)
		    *pmi = i;
	    }
	    /* pit->filter_score > 0 && score <= 0:
	     * A title item has to be hidden because no child matched. This can
	     * be done after sorting since the ones to be removed will have
	     * filter_score=0 and will be placed at the end of the index. */
	    pit->filter_score = score;
	    if (score > 0)
	    {
		pmi = (int*) title_items->op->get_new_item(title_items);
		if (pmi)
		    *pmi = i;
	    }
	    score = 0;
	    continue;
	}
	else
	{
	    if (pit->filter_score > score)
		score = pit->filter_score;
	}
    }

    /* Each title item gets a unique score based on the sorting order. */
    if (!pcmp)
	pcmp = new_FltComparator_Score();
    pcmp->model = pmodel;
    pcmp->reverse = 1;
    title_items->op->sort(title_items, (ItemComparator_T*)pcmp);
    score = 0xffff; /* ushort_max */
    for (i = 0; i < title_items->len; ++i)
    {
	pmi = (int*) title_items->op->get_item(title_items, i);
	if (! pmi)
	    continue;
	pit = pmodel->op->get_item(pmodel, *pmi);
	if (! pit)
	    continue;
	pit->filter_parent_score = score;
	pit->filter_score = 0xffffffff; /* ulong_max; the parent must be displayed first. */
	if (score > 0)
	    --score;
    }
    CLASS_DELETE(title_items);
    CLASS_DELETE(pcmp);

    /* the parent_score of every title is copied to its children */
    score = 0; /* items without a parent will go to the end */
    for (i = 0; i < item_count; ++i)
    {
	pit = pmodel->op->get_item(pmodel, i);
	if (! pit)
	    continue;
	if (pmodel->op->has_flag(pmodel, i, ITEM_TITLE))
	    score = pit->filter_parent_score;
	else
	{
	    pit->filter_parent_score = score;
	    if (pit->filter_score > 0xfffffff0)
		pit->filter_score = 0xfffffff0; /* less than parent-title */
	}
    }

    /* the items can finally be sorted by parent_score/score */
    {
	FltComparator_TitleScore_T* ptcmp;
	ptcmp = new_FltComparator_TitleScore();
	ptcmp->model = self->model;
	ptcmp->reverse = 1;
	self->items->op->sort(self->items, (ItemComparator_T*)ptcmp);
	CLASS_DELETE(ptcmp);
    }
    END_METHOD;
}

    static int
_iflt_get_item_count(_self)
    void* _self;
    METHOD(ItemFilter, get_item_count);
{
    if (STRLEN(self->text) < 1)
	return self->model->op->get_item_count(self->model);

    return self->items->len;
    END_METHOD;
}

    static int
_iflt_get_model_index(_self, index)
    void* _self;
    int index;
    METHOD(ItemFilter, get_model_index);
{
    if (STRLEN(self->text) < 1)
	return index;

    if (index < 0 || index >= self->items->len)
	return -1;

    return *(int*) self->items->op->get_item(self->items, index);
    END_METHOD;
}

    static int
_iflt_get_index_of(_self, model_index)
    void* _self;
    int model_index;
    METHOD(ItemFilter, get_index_of);
{
    int i, item_count;
    int *pmi;
    if (STRLEN(self->text) < 1)
	return model_index;

    item_count = self->model->op->get_item_count(self->model);
    if (model_index < 0 || model_index >= item_count)
	return -1;

    item_count = self->items->len;
    for(i = 0; i < item_count; i++)
    {
	pmi = (int*) self->items->op->get_item(self->items, i);
	if (pmi && *pmi == model_index)
	    return i;
    }

    return -1;
    END_METHOD;
}

/* [ooc]
 *
  class BoxAligner [bxal] {
    Box	    limits;
    char    popup[2];	// part of popup-box to align; 012; a=auto(larger space)
    char    screen[2];	// screen point to align to; 0123 4 5678; c=cursor; C=below/above cursor; xy
    int	    screen_x;	// coordinate for screen[0]=='x'
    int	    screen_y;	// coordinate for screen[1]=='y'

    void    init();
    // void    destroy();
    void    set_limits(int left, int top, int right, int bottom);
    void    parse_screen_pos(char_u* posdef);
    void    set_align_params(dict_T* params);
    void    align(Box* box, WindowBorder* border);
  };
*/

    static void
_bxal_init(_self)
    void*   _self;
    METHOD(BoxAligner, init);
{

    init_Box(&self->limits);
    self->op->set_limits(self, 0, 0, Columns-1, Rows-1);

    /* default alignment: centered on screen */
    self->popup[0] = 's';
    self->popup[1] = 's';
    self->screen[0] = '4';
    self->screen[1] = '4';
    END_METHOD;
}

    static void
_bxal_set_limits(_self, left, top, right, bottom)
    void*   _self;
    int	    left;
    int	    top;
    int	    right;
    int	    bottom;
    METHOD(BoxAligner, set_limits);
{
    self->limits.left = left;
    self->limits.top = top;
    self->limits.width = right-left+1;
    self->limits.height = bottom-top+1;
    END_METHOD;
}

    static void
_bxal_parse_screen_pos(_self, posdef)
    void*	_self;
    char_u*	posdef;
    METHOD(BoxAligner, parse_screen_pos);
{
    int i;
    if (! posdef)
	return;

    for (i = 0; i < 2; i++)
    {
	if (*posdef >= '0' && *posdef <= '8')
	{
	    self->screen[i] = *posdef;
	}
	/* TODO: screen position: c, x, y */
	if (*posdef == NUL)
	    break;
	++posdef;
    }
    END_METHOD;
}

    static void
_bxal_set_align_params(_self, params)
    void*	_self;
    dict_T*	params;
    METHOD(BoxAligner, set_align_params);
{
    END_METHOD;
}

    static int
_num_to_coord(num, cmin, cmax)
    char    num;
    int	    cmin;
    int	    cmax;
{
    if (num < '0' || num > '8' || cmax < cmin)
	return -1;

    return (int) (cmin + (num - '0') * (cmax - cmin) / 8.0f + 0.5f);
}

    static void
_bxal_align(_self, box, border)
    void*		_self;
    Box_T*		box;
    WindowBorder_T*	border;
    METHOD(BoxAligner, align);
{
    int px, py, sx, sy;
    Box_T* limits = &self->limits;

    LOG(("Screen: rows=%d cols=%d", Rows, Columns));
    LOG(("Limits: l=%d, t=%d, r=%d, b=%d", limits->left, limits->top, _box_right(limits), _box_bottom(limits)));
    LOG(("Pos: on_screen=%c%c box_point=%c%c", self->screen[0], self->screen[1], self->popup[0], self->popup[1]));

    sx = _num_to_coord(self->screen[0], 0, limits->width-1);
    sy = _num_to_coord(self->screen[1], 0, limits->height-1);
    if (self->popup[0] == 's')
	px = (int) (box->width * (1.0 * sx / limits->width) + 0.5);
    else
	px = _num_to_coord(self->popup[0], 0, box->width-1);
    if (self->popup[1] == 's')
	py = (int) (box->height * (1.0 * sy / limits->height) + 0.5);
    else
	py = _num_to_coord(self->popup[1], 0, box->height-1);

    box->left = limit_value(limits->left + sx - px, limits->left, _box_right(limits));
    box->top = limit_value(limits->top + sy - py, limits->top, _box_bottom(limits));

    LOG(("Box A: l=%d, t=%d, r=%d, b=%d", box->left, box->top, _box_right(box), _box_bottom(box)));

    if (border)
    {
	/* adjust if the border is present and outside of limits */
	if (border->active[WINBORDER_LEFT] && box->left - 1 < limits->left)
	    ++box->left;
	else if (border->active[WINBORDER_RIGHT] && _box_right(box) + 1 > _box_right(limits))
	    --box->left;
	if (border->active[WINBORDER_TOP] && box->top - 1 < limits->top)
	    ++box->top;
	else if (border->active[WINBORDER_BOTTOM] && _box_bottom(box) + 1 > _box_bottom(limits))
	    --box->top;

	LOG(("Box B: l=%d, t=%d, r=%d, b=%d", box->left, box->top, _box_right(box), _box_bottom(box)));
    }
    END_METHOD;
}

/* [ooc]
 *
  const KM_NOTFOUND  = 0;
  const KM_PREFIX    = 1;
  const KM_MATCH     = 2;
  const KM_AMBIGUOUS = (KM_PREFIX | KM_MATCH);
  // TODO: SimpleKeymap could be implemented as a tree where every level
  // consumes a byte of the input sequence. Maybe it would be easier to
  // find ambiguities this way.
  //    struct _kmap_ {
  //       char_u  piece;
  //       char_u* command;
  //       struct _kmap_ children[];
  //    }
  // TODO: (maybe) special commands
  //    @'seq   - stuff the sequence seq into the input buffer (similar to remap)
  //    @#cmd   - accept an operator and pass it to the command cmd
  //    @@cmd   - literal '@'; the command name is '@seq'.
  class SimpleKeymap [skmap]
  {
     char_u*	name;
     dict_T*	key2cmd;    // maps a raw Vim sequence to a command name
     int	has_insert; // kmap is used for text insertion
     char_u	mode_char;  // displayed in the border when input is active; only used with has_insert

     void   init();
     void   destroy();
     void   set_name(char_u* name);

     // Encode a Vim <key> sequence to a raw vim key sequence
     char_u*	encode_key(char_u* sequence);
     void	set_vim_key(char_u* sequence, char_u* command);

     // The following commands work with raw sequences.
     void	set_key(char_u* sequence, char_u* command);
     char_u*	get_command(char_u* sequence, int copy);
     int	find_key(char_u* sequence);
     // TODO later:
     // void	clear_key(char_u* sequence);
     // void	clear_key_prefix(char_u* sequence);
     void	clear_all_keys();
     // void	get_mapped_keys(); // needed only to implement help
  };
*/

    static void
_skmap_init(_self)
    void* _self;
    METHOD(SimpleKeymap, init);
{
    self->name = NULL;
    self->has_insert = 0;
    self->mode_char = NUL;
    self->key2cmd = dict_alloc();
    ++self->key2cmd->dv_refcount;
    END_METHOD;
}

    static void
_skmap_destroy(_self)
    void* _self;
    METHOD(SimpleKeymap, destroy);
{
    _str_free(&self->name);
    if (self->key2cmd)
    {
	dict_unref(self->key2cmd);
	self->key2cmd = NULL;
    }
    END_DESTROY(SimpleKeymap);
}

    static void
_skmap_set_name(_self, name)
    void* _self;
    char_u* name;
    METHOD(SimpleKeymap, set_name);
{
    _str_assign(&self->name, name);
    END_METHOD;
}

    static char_u*
_skmap_encode_key(_self, sequence)
    void* _self;
    char_u* sequence;
    METHOD(SimpleKeymap, encode_key);
{
    char_u *seq, *raw;

    /* enclose in quotes to create a vim string */
    seq = (char_u*) alloc(128);
    raw = vim_strsave_escaped(sequence, VSTR("\"<"));
    seq[0] = '"';
    STRNCPY(seq + 1, raw, 126);
    STRCAT(seq, "\"");
    vim_free(raw);

    /* evaluate the vim string */
    raw = eval_to_string_safe(seq, NULL, TRUE);
    vim_free(seq);
    return raw;
    END_METHOD;
}

    static void
_skmap_set_vim_key(_self, sequence, command)
    void* _self;
    char_u* sequence;
    char_u* command;
    METHOD(SimpleKeymap, set_vim_key);
{
    sequence = self->op->encode_key(self, sequence);
    self->op->set_key(self, sequence, command);
    vim_free(sequence);
    END_METHOD;
}

    static void
_skmap_set_key(_self, sequence, command)
    void* _self;
    char_u* sequence;
    char_u* command;
    METHOD(SimpleKeymap, set_key);
{
    dictitem_T* pi;
    if (!sequence || !command)
	return;

    /* TODO: Check for conflicting sequences */
    pi = dict_find(self->key2cmd, sequence, -1);
    if (pi)
	dictitem_remove(self->key2cmd, pi);
    dict_add_nr_str(self->key2cmd, (char*)sequence, 0, command);
    END_METHOD;
}

    static char_u*
_skmap_get_command(_self, sequence, copy)
    void* _self;
    char_u* sequence;
    int copy;
    METHOD(SimpleKeymap, get_command);
{
    dictitem_T* seqmap;
    seqmap = dict_find(self->key2cmd, sequence, -1L);
    if (seqmap)
    {
	if (copy)
	    return vim_strsave(seqmap->di_tv.vval.v_string);
	else
	    return seqmap->di_tv.vval.v_string;
    }

    return NULL;
    END_METHOD;
}

    static int
_skmap_find_key(_self, sequence)
    void* _self;
    char_u* sequence;
    METHOD(SimpleKeymap, find_key);
{
    dictitem_T* seqmap;
    DictIterator_T* pitkeys;
    int seq_len, match;

    match = 0; /* KM_NOTFOUND */
    seqmap = dict_find(self->key2cmd, sequence, -1L);
    if (seqmap)
    {
	match |= KM_MATCH;
#ifdef DEBUG
	LOG(("   '%s' --> '%s'", sequence, seqmap->di_tv.vval.v_string));
#endif
    }

    /* Test if sequence is a prefix of any of the items in the current modemap */
    seq_len = STRLEN(sequence);

    pitkeys = new_DictIterator();
    for(seqmap = pitkeys->op->begin(pitkeys, self->key2cmd); seqmap != NULL; seqmap = pitkeys->op->next(pitkeys))
    {
	if (EQUALSN(seqmap->di_key, sequence, seq_len) && *(seqmap->di_key + seq_len) != NUL)
	{
#ifdef DEBUG
	    if (match)
		LOG(("   '%s' --> '%s' AMBIGUOUS", seqmap->di_key, seqmap->di_tv.vval.v_string));
#endif
	    match |= KM_PREFIX;
	    break;
	}
    }
    CLASS_DELETE(pitkeys);

    return match;
    END_METHOD;
}

    static void
_skmap_clear_all_keys(_self)
    void* _self;
    METHOD(SimpleKeymap, clear_all_keys);
{
    if (! self->key2cmd)
	return;
    if ((int)self->key2cmd->dv_hashtab.ht_used < 1)
	return;

    /* XXX: can't find suitable methods, so we delete and rectreate the dictionary */
    dict_unref(self->key2cmd);
    self->key2cmd = dict_alloc();
    ++self->key2cmd->dv_refcount;
    END_METHOD;
}


/* [ooc]
 *
  const WINBORDER_TOP = 0;
  const WINBORDER_RIGHT = 1;
  const WINBORDER_BOTTOM = 2;
  const WINBORDER_LEFT = 3;
  const SCROLLBAR_BAR = 0;
  const SCROLLBAR_THUMB = 1;
  class WindowBorder [wbor]
  {
    Box*    inner_box;          // the area for drawing the items; owned by popuplist
    int     item_count;
    int     active[4];		// visible sides: 'trbl'; 0 - off, otherwise on
    int     border_chars[8];	// spaces, single, double, minus/bar, ...
    int     scrollbar_chars[2];
    int     scrollbar_attr[2];
    int     scrollbar_thumb;	// size of the scrollbar thumb; 0 - SB is off
    int     scrollbar_pos;	// left or right border
    int     border_attr;
    char_u* title;
    char_u* mode; 	    // one or two characters
    char_u* info; 	    // arbitrary info, eg. position; right aligned in frame
    int     input_active;   // input field is active, special display
    LineEdit*  line_edit;   // input line to be displayed in the border; owned by popuplist

    void init();
    void destroy();
    void set_title(char_u* title);
    void set_mode_text(char_u* mode);
    void set_input_active(int active);
    void set_info(char_u* text);
    void prepare_scrollbar(int item_count);
    int  get_scrollbar_kind(int line, int current); // kind of char to draw in line; bar or thumb
    void draw_top();
    void draw_item_left(int line, int current);
    void draw_item_right(int line, int current);
    void draw_bottom();
  };

*/

/* t, tr, r, br, b, bl, l, tl */
static int _frame_blank[]  = { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' };
static int _frame_ascii[]  = { '-', '+', '|', '+', '-', '+', '|', '+' };
#ifndef FEAT_MBYTE
static int _frame_single[]  = { '-', '+', '|', '+', '-', '+', '|', '+' };
static int _frame_double[]  = { '=', '+', '|', '+', '=', '+', '|', '+' };
static int _frame_mixed1[]  = { '-', '+', '|', '+', '-', '+', '|', '+' };
static int _frame_mixed2[]  = { '=', '+', '|', '+', '=', '+', '|', '+' };
static int _frame_custom[]  = { '-', '+', '|', '+', '-', '+', '|', '+' };
#else
/* utf-8: "─┐│┘─└│┌"  */
static int _frame_single[]  = { 0x2500, 0x2510, 0x2502, 0x2518, 0x2500, 0x2514, 0x2502, 0x250c };
/* utf-8: "═╗║╝═╚║╔" */
static int _frame_double[]  = { 0x2550, 0x2557, 0x2551, 0x255d, 0x2550, 0x255a, 0x2551, 0x2554 };
/* utf-8: "─╖║╜─╙║╓" */
static int _frame_mixed1[]  = { 0x2500, 0x2556, 0x2551, 0x255c, 0x2500, 0x2559, 0x2551, 0x2553 };
/* utf-8: "═╕│╛═╘│╒" */
static int _frame_mixed2[]  = { 0x2550, 0x2555, 0x2502, 0x255b, 0x2550, 0x2558, 0x2502, 0x2552 };
/* single again */
static int _frame_custom[]  = { 0x2500, 0x2510, 0x2502, 0x2518, 0x2500, 0x2514, 0x2502, 0x250c };
#endif
static int* _frames[] = {
    _frame_blank, _frame_ascii,
    _frame_single, _frame_double,
    _frame_mixed1, _frame_mixed2,
    _frame_custom
};


    static void
_wbor_init(_self)
    void* _self;
    METHOD(WindowBorder, init);
{
    int i;
    int *pframe;
    for (i = 0; i < 4; i++)
	self->active[i] = 1;
    self->scrollbar_thumb = 0;

    /* TODO: user option for frame type: 0-7; 0: no frame, 1: blank, 2: ascii, ...;
     * TODO: frames 3-7 can be changed with options */
#ifdef FEAT_MBYTE
    if (has_mbyte) pframe = _frames[3];
    else pframe = _frames[0];
#else
    pframe = _frames[0];
#endif
    for (i = 0; i < 8; i++)
	self->border_chars[i] = pframe[i];

    self->border_attr = _puls_hl_attrs[PULSATTR_BORDER].attr;

    /* background, thumb */
    self->scrollbar_chars[SCROLLBAR_BAR]   = self->border_chars[WINBORDER_RIGHT*2];
    self->scrollbar_chars[SCROLLBAR_THUMB] = ' ';
    if (self->scrollbar_chars[SCROLLBAR_BAR] == ' ')
	self->scrollbar_attr[SCROLLBAR_BAR] = _puls_hl_attrs[PULSATTR_SCROLL_SPACE].attr;
    else
	self->scrollbar_attr[SCROLLBAR_BAR] = _puls_hl_attrs[PULSATTR_SCROLL_BAR].attr;
    self->scrollbar_attr[SCROLLBAR_THUMB]  = _puls_hl_attrs[PULSATTR_SCROLL_THUMB].attr;
    self->scrollbar_pos = WINBORDER_RIGHT;
    self->title = NULL;
    self->info = NULL;
    self->mode = NULL;
    self->input_active = 0;
    self->line_edit = NULL;
    END_METHOD;
}

    static void
_wbor_destroy(_self)
    void* _self;
    METHOD(WindowBorder, destroy);
{
    self->inner_box = NULL;	/* frame doesn't own the box */
    self->line_edit = NULL;	/* frame doesn't own the line_edit */
    _str_free(&self->title);
    _str_free(&self->info);
    _str_free(&self->mode);
    END_DESTROY(WindowBorder);
}

    static void
_wbor_set_title(_self, title)
    void* _self;
    char_u* title;
    METHOD(WindowBorder, set_title);
{
    _str_assign(&self->title, title);
    END_METHOD;
}

    static void
_wbor_set_mode_text(_self, mode)
    void* _self;
    char_u* mode;
    METHOD(WindowBorder, set_mode_text);
{
    char_u buf[16]; /* XXX: small, but remove from stack, anyway */
    char_u *p;
    int i = 2;
    p = buf;
    if (mode)
    {
	while (i > 0 && *mode != NUL)
	{
	    MB_COPY_CHAR(mode, p);
	    --i;
	}
    }
    while (i > 0)
    {
	int ch = self->border_chars[WINBORDER_BOTTOM*2];
	p += mb_char2bytes(ch, p);
	--i;
    }
    *p = NUL;

    _str_assign(&self->mode, buf);
    END_METHOD;
}

    static void
_wbor_set_input_active(_self, active)
    void* _self;
    int active;
    METHOD(WindowBorder, set_input_active);
{
    self->input_active = active;
    END_METHOD;
}

    static void
_wbor_set_info(_self, text)
    void* _self;
    char_u* text;
    METHOD(WindowBorder, set_info);
{
    _str_assign(&self->info, text);
    END_METHOD;
}

    static void
_wbor_prepare_scrollbar(_self, item_count)
    void* _self;
    int item_count;
    METHOD(WindowBorder, prepare_scrollbar);
{
    int h;
    self->item_count = item_count;
    if (! self->inner_box)
	return;
    h = self->inner_box->height;
    if (item_count <= h)
    {
	self->scrollbar_thumb = 0;
	return;
    }
    self->scrollbar_thumb = h * h / item_count;
    if (self->scrollbar_thumb < 1)
	self->scrollbar_thumb = 1;
    END_METHOD;
}

    static int
_wbor_get_scrollbar_kind(_self, line, current)
    void* _self;
    int line;
    int current;
    METHOD(WindowBorder, get_scrollbar_kind);
{
    int sbtop = self->inner_box->height - self->scrollbar_thumb;
    sbtop = (int) ((float)sbtop * current / self->item_count + 0.5);
    if (line >= sbtop && line < sbtop + self->scrollbar_thumb)
	return SCROLLBAR_THUMB;
    return SCROLLBAR_BAR;
    END_METHOD;
}

    static void
_wbor_draw_top(_self)
    void* _self;
    METHOD(WindowBorder, draw_top);
{
    int row, col, right, ch;
    LineWriter_T* writer;
    if (! self->inner_box || !self->active[WINBORDER_TOP])
	return;

    row = self->inner_box->top - 1;
    col = self->inner_box->left;
    right = _box_right(self->inner_box);
    if (self->active[WINBORDER_LEFT])
	screen_putchar(self->border_chars[WINBORDER_LEFT*2+1], row, col-1, self->border_attr);
    screen_putchar(self->border_chars[WINBORDER_TOP*2], row, col, self->border_attr);

    writer = new_LineWriter();
    writer->min_col = self->inner_box->left + 1;
    writer->max_col = right - 1;
    writer->tab_size = -1;
    ch = self->border_chars[WINBORDER_TOP*2];
    writer->op->write_line(writer, self->title, row, self->border_attr, ch);

    screen_putchar(self->border_chars[WINBORDER_TOP*2], row, right, self->border_attr);
    if (self->active[WINBORDER_RIGHT])
	screen_putchar(self->border_chars[WINBORDER_TOP*2+1], row, right+1, self->border_attr);

    CLASS_DELETE(writer);
    END_METHOD;
}

    static void
_wbor_draw_bottom(_self)
    void* _self;
    METHOD(WindowBorder, draw_bottom);
{
    int row, col, endcol, right, ch, chbot, attr, iw;
    char_u* input;
    LineWriter_T* writer;

    if (! self->inner_box || !self->active[WINBORDER_BOTTOM])
	return;

    row = _box_bottom(self->inner_box) + 1;
    col = self->inner_box->left;
    right = _box_right(self->inner_box);
    attr = self->border_attr;
    /*LOG(("draw_bottom: row=%d col=%d", row, col));*/
    if (self->active[WINBORDER_LEFT])
    {
	screen_putchar(self->border_chars[WINBORDER_BOTTOM*2+1], row, col-1, attr);
    }

    chbot = self->border_chars[WINBORDER_BOTTOM*2];
    screen_putchar(chbot, row, col, attr);
    ++col;

    writer = new_LineWriter();
    writer->tab_size = -1;

    /* MODE */
    writer->op->set_limits(writer, col, col+2);
    writer->op->write_line(writer, self->mode, row, attr, chbot);
    col += 2;

    screen_putchar(chbot, row, col, attr);
    ++col;

    /* INPUT */
    if (self->line_edit)
	input = self->line_edit->text;
    else
	input = NULL;

    if (self->input_active && self->line_edit)
    {
	attr = _puls_hl_attrs[PULSATTR_INPUT_ACTIVE].attr;
	ch = ' ';
    }
    else
    {
	attr = _puls_hl_attrs[PULSATTR_INPUT].attr;
	ch = chbot;
    }
    /* TODO: the position/width of the edit box is calculated on resize */
    iw = self->inner_box->width / 3;
    if (iw < 8)
	iw = 8;
    endcol = col + iw;
    if (endcol > right)
	endcol = right;
    if (self->line_edit)
    {
	self->line_edit->position.left = col;
	self->line_edit->position.top = row;
	self->line_edit->position.width = endcol - col + 1;
    }
    /* TODO: right-aligned text when it is too long for the line_edit width */
    writer->op->set_limits(writer, col, endcol);
    writer->op->write_line(writer, input, row, attr, ch);
    col = endcol + 1;

    attr = self->border_attr;
    if (col < right)
    {
	screen_putchar(chbot, row, col, attr);
	++col;
    }

    /* INFO */
    if (col < right)
    {
	writer->op->set_limits(writer, col, right - 1);
	writer->op->write_line(writer, self->info, row, attr, chbot);
    }

    screen_putchar(chbot, row, right, attr);

    if (self->active[WINBORDER_RIGHT])
    {
	screen_putchar(self->border_chars[WINBORDER_BOTTOM*2-1], row, right+1, attr);
    }

    CLASS_DELETE(writer);
    END_METHOD;
}

    static void
_wbor_draw_item_left(_self, line, current)
    void* _self;
    int line;
    int current;
    METHOD(WindowBorder, draw_item_left);
{
    int row, col;
    if (! self->inner_box)
	return;
    if (! self->active[WINBORDER_LEFT])
	return;

    col = self->inner_box->left - 1;
    if (col < 0)
	return;
    row = self->inner_box->top + line;

    if (self->scrollbar_pos == WINBORDER_LEFT && self->scrollbar_thumb > 0)
    {
	int ci = self->op->get_scrollbar_kind(self, line, current);
	screen_putchar(self->scrollbar_chars[ci], row, col, self->scrollbar_attr[ci]);
	return;
    }

    screen_putchar(self->border_chars[WINBORDER_RIGHT*2], row, col, self->border_attr);
    END_METHOD;
}

    static void
_wbor_draw_item_right(_self, line, current)
    void* _self;
    int line;
    int current;
    METHOD(WindowBorder, draw_item_right);
{
    int row, col;
    if (! self->inner_box)
	return;
    if (! self->active[WINBORDER_RIGHT])
	return;

    col = _box_right(self->inner_box) + 1;
    if (col > Columns)
	return;
    row = self->inner_box->top + line;

    if (self->scrollbar_pos == WINBORDER_RIGHT && self->scrollbar_thumb > 0)
    {
	int ci = self->op->get_scrollbar_kind(self, line, current);
	screen_putchar(self->scrollbar_chars[ci], row, col, self->scrollbar_attr[ci]);
	return;
    }

    screen_putchar(self->border_chars[WINBORDER_RIGHT*2], row, col, self->border_attr);
    END_METHOD;
}

/* [ooc]
 *
  const PULS_DEF_HEIGHT = 10;
  const PULS_DEF_WIDTH  = 20;
  const PULS_MIN_WIDTH  = 10;
  const PULS_MIN_HEIGHT = 1;
  // TODO: use the flags correctly!!!
  const PULS_REDRAW_CURRENT = 0x01;
  const PULS_REDRAW_FRAME   = 0x02;
  const PULS_REDRAW_ALL     = 0x0f;
  // clear the screen before drawing the puls; implies REDRAW_ALL
  const PULS_REDRAW_CLEAR   = 0x10;
  // resize before drawing; implies REDRAW_CLEAR
  const PULS_REDRAW_RESIZE  = 0x20;
  // main loop control
  const PULS_LOOP_BREAK	    = 0;
  const PULS_LOOP_CONTINUE  = 1;
  class PopupList(object) [puls]
  {
    ItemProvider*   model;	// items of the displayed puls
    ItemFilter*	    filter;	// selects a subset of model->items
    ISearch*	    isearch;	// searches for a string in a filtered subset
    char_u*         filter_matcher_name;
    char_u*         isearch_matcher_name;
    TextMatcherFactory* filter_matcher_factory;
    TextMatcherFactory* isearch_matcher_factory;
    SimpleKeymap*   km_normal;
    SimpleKeymap*   km_filter;
    SimpleKeymap*   km_search;
    SimpleKeymap*   km_shortcut;
    SimpleKeymap*   modemap;	// current mode map
    CommandQueue*   cmds_keyboard;
    CommandQueue*   cmds_macro;
    BoxAligner*	    aligner;	// positions the list box on the screen
    WindowBorder*   border;
    LineEdit*	    line_edit;
    ShortcutHighlighter*    hl_menu;
    TextMatcher*            user_matcher; // used with hl_user to highlight a user-defined string
    TextMatchHighlighter*   hl_user;      // set in options
    TextMatchHighlighter*   hl_filter;    // TODO: depends on the selected filter
    TextMatchHighlighter*   hl_isearch;
    Highlighter*	    hl_chain;  // chain of highlighters, updated in update_hl_chain()
    Box position;	    // position of the items, without the frame
    int current;	    // index of selected item or -1
    int first;		    // index of top item
    int leftcolumn;	    // first displayed column
    int column_split;	    // TRUE when displayed in two columns split by a tab
    int col0_width;	    // width of column 0
    int col1_width;	    // width of column 1; only valid when column_split is TRUE
    int split_width;	    // the displayed width of column 0; less or eqal to col0_width
    int need_redraw;	    // redraw is needed

    void    init();
    void    destroy();
    void    set_model(ItemProvider* model);
    void    read_options(dict_T* options);
    void    default_keymap();
    void    map_keys(char_u* kmap_name, dict_T* kmap);
    int	    calc_size(int limit_width, int limit_height);
    void    reposition();
    void    update_hl_chain();
    void    redraw();
    int	    refilter(int track_item, int always_track);
    void    move_cursor();
    int	    do_command(char_u* command);
    void    switch_mode(char_u* modename);
    void    prepare_result(dict_T* result);
    void    save_state(dict_T* result);
    void    set_title(char_u* title);
    void    set_current(int index);
    int	    do_isearch(int dir); // perfrom isearch with the current isearch settings; @param dir=+-1
    int	    on_filter_change(void* data);   // callback to update filter when input changes
    int	    on_isearch_change(void* data);  // callback to uptate isearch when input changes
    int	    on_model_title_changed(void* data);  // callback to uptate the title when it changes

    // main loop
    int	    process_command(char_u* command);
  };

*/

    static void
_puls_init(_self)
    void* _self;
    METHOD(PopupList, init);
{
    self->model = NULL;
    self->aligner = NULL;
    self->first = 0;
    self->current = 0;
    self->leftcolumn = 0;
    self->column_split = 0;
    self->col0_width = 0;
    self->col1_width = 0;
    self->need_redraw = 0;
    self->isearch = new_ISearch();
    self->filter = new_ItemFilter();
    self->filter_matcher_name = vim_strsave(VSTR("words"));
    self->isearch_matcher_name = vim_strsave(VSTR("simple"));
    self->filter_matcher_factory = new_TextMatcherFactory();
    self->isearch_matcher_factory = new_TextMatcherFactory();
    self->km_normal = new_SimpleKeymap();
    /* TODO: add parameter: mode_indicator, eg. "/" */
    self->km_normal->op->set_name(self->km_normal, VSTR("normal"));
    self->km_filter = new_SimpleKeymap();
    self->km_filter->op->set_name(self->km_filter, VSTR("filter"));
    self->km_search = new_SimpleKeymap();
    self->km_search->op->set_name(self->km_search, VSTR("isearch"));
    self->km_shortcut = new_SimpleKeymap();
    self->km_shortcut->op->set_name(self->km_shortcut, VSTR("shortcut"));
    self->modemap = self->km_normal;
    self->cmds_keyboard = new_CommandQueue();
    self->cmds_macro = new_CommandQueue();
    init_Box(&self->position);
    self->border = new_WindowBorder();
    self->border->inner_box = &self->position;
    self->line_edit = new_LineEdit();
    self->border->line_edit = self->line_edit;

    self->op->default_keymap(self);

    self->user_matcher = NULL;
    self->hl_user = NULL;
    self->hl_chain = NULL;
    self->hl_menu = NULL;
    self->hl_isearch = new_TextMatchHighlighter();
    self->hl_isearch->match_attr = _puls_hl_attrs[PULSATTR_HL_SEARCH].attr;
    self->hl_filter = new_TextMatchHighlighter();
    self->hl_filter->match_attr = _puls_hl_attrs[PULSATTR_HL_FILTER].attr;
    self->hl_menu = new_ShortcutHighlighter();
    self->hl_menu->shortcut_attr = _puls_hl_attrs[PULSATTR_SHORTCUT].attr;
    END_METHOD;
}

    static void
_puls_destroy(_self)
    void* _self;
    METHOD(PopupList, destroy);
{
    self->model = NULL;	    /* puls doesn't own the model */
    self->aligner = NULL;   /* puls doesn't own the aligner */
    self->hl_chain = NULL;  /* points to one of the highlighters */

    vim_free(self->filter_matcher_name);
    vim_free(self->isearch_matcher_name);

    CLASS_DELETE(self->km_normal);
    CLASS_DELETE(self->km_filter);
    CLASS_DELETE(self->km_search);
    CLASS_DELETE(self->km_shortcut);
    CLASS_DELETE(self->cmds_keyboard);
    CLASS_DELETE(self->cmds_macro);
    CLASS_DELETE(self->border);
    CLASS_DELETE(self->line_edit);
    CLASS_DELETE(self->user_matcher);
    CLASS_DELETE(self->hl_user);
    CLASS_DELETE(self->hl_menu);
    CLASS_DELETE(self->hl_isearch);
    CLASS_DELETE(self->hl_filter);
    CLASS_DELETE(self->filter);
    CLASS_DELETE(self->isearch);
    CLASS_DELETE(self->filter_matcher_factory)
    CLASS_DELETE(self->isearch_matcher_factory)

    END_DESTROY(PopupList);
}

    static void
_puls_set_model(_self, model)
    void* _self;
    ItemProvider_T* model;
    METHOD(PopupList, set_model);
{
    self->model = model;
    if (self->filter)
	self->filter->model = model;

    if (self->model)
    {
	self->model->title_obsrvrs.op->add(&self->model->title_obsrvrs,
		self, &_puls_on_model_title_changed);
	self->op->set_title(self, self->model->op->get_title(self->model));
    }
    END_METHOD;
}

    static int
_puls_on_model_title_changed(_self, data)
    void* _self;
    void* data;
    METHOD(PopupList, on_model_title_changed);
{
    if (self->model)
	self->op->set_title(self, self->model->op->get_title(self->model));
    return 1;
    END_METHOD;
}

    static void
_puls_read_options(_self, options)
    void* _self;
    dict_T* options;
    METHOD(PopupList, read_options);
{
    dictitem_T *option, *di;

    option = dict_find(options, VSTR("keymap"), -1L);
    if (option && option->di_tv.v_type == VAR_DICT)
    {
	/* create the keymaps */
	DictIterator_T* pitd = new_DictIterator();
	for(di = pitd->op->begin(pitd, option->di_tv.vval.v_dict); di != NULL; di = pitd->op->next(pitd))
	{
	    if (di->di_tv.v_type != VAR_DICT)
	    {
		/* TODO: WARN - invalid keymap type, should be dict */
		continue;
	    }
	    self->op->map_keys(self, di->di_key, di->di_tv.vval.v_dict);
	}
	CLASS_DELETE(pitd);
    }

    option = dict_find(options, VSTR("pos"), -1L);
    if (option && self->aligner)
    {
	if (option->di_tv.v_type == VAR_STRING)
	{
	    self->aligner->op->parse_screen_pos(self->aligner, option->di_tv.vval.v_string);
	}
	else if (option->di_tv.v_type == VAR_DICT)
	    self->aligner->op->set_align_params(self->aligner, option->di_tv.vval.v_dict);
    }

    option = dict_find(options, VSTR("mode"), -1L);
    if (option && option->di_tv.v_type == VAR_STRING)
    {
	self->op->switch_mode(self, option->di_tv.vval.v_string);
    }

    option = dict_find(options, VSTR("isearch_matcher"), -1L);
    if (option && option->di_tv.v_type == VAR_STRING && option->di_tv.vval.v_string)
    {
	_str_assign(&self->isearch_matcher_name, option->di_tv.vval.v_string);
    }

    option = dict_find(options, VSTR("filter_matcher"), -1L);
    if (option && option->di_tv.v_type == VAR_STRING && option->di_tv.vval.v_string)
    {
	_str_assign(&self->filter_matcher_name, option->di_tv.vval.v_string);
    }

    option = dict_find(options, VSTR("highlight_matcher"), -1L);
    if (option && option->di_tv.v_type == VAR_STRING && option->di_tv.vval.v_string)
    {
	TextMatcher_T* matcher;
	matcher = self->isearch_matcher_factory->op->create_matcher(
		self->isearch_matcher_factory, option->di_tv.vval.v_string);
	if (matcher)
	{
	    vim_free(self->user_matcher);
	    self->user_matcher = matcher;
	}
    }

    option = dict_find(options, VSTR("isearch"), -1L);
    if (option && option->di_tv.v_type == VAR_STRING && option->di_tv.vval.v_string)
    {
	if (self->isearch)
	    self->isearch->op->set_text(self->isearch, option->di_tv.vval.v_string);
    }

    option = dict_find(options, VSTR("filter"), -1L);
    if (option && option->di_tv.v_type == VAR_STRING && option->di_tv.vval.v_string)
    {
	if (self->filter)
	    self->filter->op->set_text(self->filter, option->di_tv.vval.v_string);
    }

    option = dict_find(options, VSTR("highlight"), -1L);
    if (option && option->di_tv.v_type == VAR_STRING && option->di_tv.vval.v_string)
    {
	if (! self->user_matcher)
	    self->user_matcher = (TextMatcher_T*) new_TextMatcher();
	if (self->user_matcher)
	    self->user_matcher->op->set_search_str(self->user_matcher, option->di_tv.vval.v_string);
	if (! self->hl_user)
	    self->hl_user = new_TextMatchHighlighter();
	if (self->hl_user)
	{
	    self->hl_user->matcher = self->user_matcher;
	    self->hl_user->match_attr = _puls_hl_attrs[PULSATTR_HL_USER].attr;
	}
    }

    option = dict_find(options, VSTR("current"), -1L);
    if (option && option->di_tv.v_type == VAR_NUMBER)
    {
	self->current = option->di_tv.vval.v_number;
	if (self->current < 0)
	    self->current = 0;
    }

    option = dict_find(options, VSTR("columns"), -1L);
    if (option && option->di_tv.v_type == VAR_NUMBER)
    {
	self->column_split = (option->di_tv.vval.v_number != 0);
	if (self->current < 0)
	    self->current = 0;
    }

    option = dict_find(options, VSTR("nextcmd"), -1L);
    if (option && option->di_tv.v_type == VAR_STRING && option->di_tv.vval.v_string)
	self->cmds_macro->op->add(self->cmds_macro, option->di_tv.vval.v_string);
    END_METHOD;
}

    static void
_puls_set_title(_self, title)
    void* _self;
    char_u* title;
    METHOD(PopupList, set_title);
{
    self->border->op->set_title(self->border, title);
    END_METHOD;
}


    static void
_puls_default_keymap(_self)
    void* _self;
    METHOD(PopupList, default_keymap);
{
    SimpleKeymap_T* modemap;
    int i;

    modemap = self->km_normal;
    modemap->op->clear_all_keys(modemap);
    modemap->op->set_key(modemap, VSTR("q"), VSTR("quit"));
    modemap->op->set_key(modemap, VSTR("j"), VSTR("next-item"));
    modemap->op->set_key(modemap, VSTR("k"), VSTR("prev-item"));
    modemap->op->set_key(modemap, VSTR("n"), VSTR("next-page")); /* XXX:? <space> */
    modemap->op->set_key(modemap, VSTR("p"), VSTR("prev-page")); /* XXX:? <b> */
    modemap->op->set_key(modemap, VSTR("gg"), VSTR("go-home"));
    modemap->op->set_key(modemap, VSTR("G"), VSTR("go-end"));
    modemap->op->set_key(modemap, VSTR("h"), VSTR("shift-left"));
    modemap->op->set_key(modemap, VSTR("l"), VSTR("shift-right"));
    modemap->op->set_key(modemap, VSTR("m"), VSTR("toggle-marked|next-item"));
    modemap->op->set_key(modemap, VSTR("f"), VSTR("modeswitch:filter"));
    modemap->op->set_key(modemap, VSTR("%"), VSTR("modeswitch:filter"));
    modemap->op->set_key(modemap, VSTR("/"), VSTR("modeswitch:isearch"));
    modemap->op->set_key(modemap, VSTR("&"), VSTR("modeswitch:shortcut"));
    modemap->op->set_vim_key(modemap, VSTR("<c-n>"), VSTR("isearch-next"));
    modemap->op->set_vim_key(modemap, VSTR("<c-p>"), VSTR("isearch-prev"));
    modemap->op->set_vim_key(modemap, VSTR("<c-l>"), VSTR("auto-resize"));
    modemap->op->set_vim_key(modemap, VSTR("<c-t>"), VSTR("filter-toggle-titles"));
    modemap->op->set_vim_key(modemap, VSTR("<tab>"), VSTR("next-item"));
    modemap->op->set_vim_key(modemap, VSTR("<s-tab>"), VSTR("prev-item"));
    modemap->op->set_vim_key(modemap, VSTR("<down>"), VSTR("next-item"));
    modemap->op->set_vim_key(modemap, VSTR("<up>"), VSTR("prev-item"));
    modemap->op->set_vim_key(modemap, VSTR("<pagedown>"), VSTR("next-page"));
    modemap->op->set_vim_key(modemap, VSTR("<pageup>"), VSTR("prev-page"));
    modemap->op->set_vim_key(modemap, VSTR("<home>"), VSTR("go-home"));
    modemap->op->set_vim_key(modemap, VSTR("<end>"), VSTR("go-end"));
    modemap->op->set_vim_key(modemap, VSTR("<left>"), VSTR("shift-left"));
    modemap->op->set_vim_key(modemap, VSTR("<right>"), VSTR("shift-right"));
    modemap->op->set_vim_key(modemap, VSTR("<cr>"), VSTR("accept"));
    modemap->op->set_vim_key(modemap, VSTR("<backspace>"), VSTR("select-parent"));
    modemap->op->set_vim_key(modemap, VSTR("<esc>"), VSTR("quit"));
#if 0 && defined(DEBUG)
    /* This sequence is ambiguous in a terminal: '<a-d>' -> '<esc>d'.
     * :map <a-d> ... creates lhs='ä' instead of lhs='<esc>d';
     * a terminal (gnome, konsole) sends '<esc>d'; gui sends 'ä'.
     * see |map-alt-keys|.
     * */
    modemap->op->set_vim_key(modemap, VSTR("<a-d>"), VSTR("next-item"));
    modemap->op->set_vim_key(modemap, VSTR("<esc>d"), VSTR("next-item"));

    modemap->op->set_vim_key(modemap, VSTR("aa"), VSTR("next-item"));
    modemap->op->set_vim_key(modemap, VSTR("aab"), VSTR("prev-item"));
#endif

    for (i = 0; i < 2; i++)
    {
	if (i == 0)
	{
	    modemap = self->km_filter;
	    modemap->mode_char = '%';
	}
	else if (i == 1)
	{
	    modemap = self->km_search;
	    modemap->mode_char = '/';
	}
	modemap->op->clear_all_keys(modemap);
	modemap->op->set_vim_key(modemap, VSTR("<cr>"), VSTR("accept"));
	modemap->op->set_vim_key(modemap, VSTR("<tab>"), VSTR("modeswitch:normal|next-item"));
	modemap->op->set_vim_key(modemap, VSTR("<s-tab>"), VSTR("modeswitch:normal|prev-item"));
	modemap->op->set_vim_key(modemap, VSTR("<esc>"), VSTR("modeswitch:normal"));
	modemap->op->set_vim_key(modemap, VSTR("<backspace>"), VSTR("input-bs"));
	modemap->op->set_vim_key(modemap, VSTR("<c-u>"), VSTR("input-clear"));
	modemap->has_insert = 1;
    }
    modemap = self->km_filter;
    modemap->op->set_vim_key(modemap, VSTR("<c-f>"), VSTR("filter-next-matcher"));
    modemap->op->set_vim_key(modemap, VSTR("<c-t>"), VSTR("filter-toggle-titles"));
    modemap = self->km_search;
    modemap->op->set_vim_key(modemap, VSTR("<c-n>"), VSTR("isearch-next"));
    modemap->op->set_vim_key(modemap, VSTR("<c-p>"), VSTR("isearch-prev"));
    modemap->op->set_vim_key(modemap, VSTR("<c-f>"), VSTR("isearch-next-matcher"));

    modemap = self->km_shortcut;
    modemap->mode_char = '&';
    modemap->op->set_key(modemap, VSTR("%"), VSTR("modeswitch:filter"));
    modemap->op->set_key(modemap, VSTR("/"), VSTR("modeswitch:isearch"));
    modemap->op->set_key(modemap, VSTR("&"), VSTR("modeswitch:normal"));
    modemap->op->set_vim_key(modemap, VSTR("<tab>"), VSTR("next-item"));
    modemap->op->set_vim_key(modemap, VSTR("<s-tab>"), VSTR("prev-item"));
    modemap->op->set_vim_key(modemap, VSTR("<down>"), VSTR("next-item"));
    modemap->op->set_vim_key(modemap, VSTR("<up>"), VSTR("prev-item"));
    modemap->op->set_vim_key(modemap, VSTR("<pagedown>"), VSTR("next-page"));
    modemap->op->set_vim_key(modemap, VSTR("<pageup>"), VSTR("prev-page"));
    modemap->op->set_vim_key(modemap, VSTR("<left>"), VSTR("shift-left"));
    modemap->op->set_vim_key(modemap, VSTR("<right>"), VSTR("shift-right"));
    modemap->op->set_vim_key(modemap, VSTR("<cr>"), VSTR("accept"));
    modemap->op->set_vim_key(modemap, VSTR("<esc>"), VSTR("quit"));
    modemap->op->set_vim_key(modemap, VSTR("<backspace>"), VSTR("select-parent"));
    END_METHOD;
}

    static void
_puls_map_keys(_self, kmap_name, kmap)
    void* _self;
    char_u* kmap_name;
    dict_T* kmap;
    METHOD(PopupList, map_keys);
{
    SimpleKeymap_T* modemap = NULL;
    dictitem_T* pkd;
    DictIterator_T* pitkey;

    if (!kmap_name || !kmap)
	return;
    if (self->km_normal && EQUALS(kmap_name, self->km_normal->name))
	modemap = self->km_normal;
    else if (self->km_filter && EQUALS(kmap_name, self->km_filter->name))
	modemap = self->km_filter;
    else if (self->km_search && EQUALS(kmap_name, self->km_search->name))
	modemap = self->km_search;
    else if (self->km_shortcut && EQUALS(kmap_name, self->km_shortcut->name))
	modemap = self->km_shortcut;
    else
	return; /* TODO: WARN - invalid keymap name! */

    pitkey = new_DictIterator();
    for (pkd = pitkey->op->begin(pitkey, kmap); pkd != NULL; pkd = pitkey->op->next(pitkey))
    {
	if (pkd->di_tv.v_type == VAR_STRING)
	{
	    modemap->op->set_vim_key(modemap, pkd->di_key, pkd->di_tv.vval.v_string);
	}
    }
    CLASS_DELETE(pitkey);
    END_METHOD;
}

    static void
_puls_prepare_result(_self, result)
    void* _self;
    dict_T* result;
    METHOD(PopupList, prepare_result);
{
    list_T*	marked;
    typval_T	tv;
    int		idx, item_count;
    if (! result)
	return;
    idx = self->filter->op->get_model_index(self->filter, self->current);
    item_count = self->model->op->get_item_count(self->model);
    dict_add_nr_str(result, "current", idx, NULL);
    marked = list_alloc();

    tv.v_type = VAR_NUMBER;
    tv.v_lock = 0;

    /* fill the list of marked items */
    dict_add_list(result, "marked", marked);
    for (idx = 0; idx < item_count; ++idx)
    {
	if (self->model->op->has_flag(self->model, idx, ITEM_MARKED))
	{
	    tv.vval.v_number = idx;
	    list_append_tv(marked, &tv);
	}
    }

    if (self->model)
	self->model->op->update_result(self->model, result);
    END_METHOD;
}

/* Save various information about the state of the puls
 * except current and marked. */
    static void
_puls_save_state(_self, result)
    void* _self;
    dict_T* result;
    METHOD(PopupList, save_state);
{
    dict_T  *d = dict_alloc();
    dict_add_nr_str(d, "isearch_matcher", 0, self->isearch_matcher_name);
    dict_add_nr_str(d, "filter_matcher", 0, self->filter_matcher_name);
    _dict_add_dict(result, "state", d);
    END_METHOD;
}

    static int
_puls_calc_size(_self, limit_width, limit_height)
    void*	_self;
    int		limit_width;
    int		limit_height;
    METHOD(PopupList, calc_size);
{
    int i;
    int w, max_width, max_width_1;
    char_u *text;
    char_u *start;
    char_u *pos;
    int item_count;

    limit_width =  limit_value(limit_width, PULS_MIN_WIDTH, Columns-2);
    limit_height = limit_value(limit_height, 1, Rows-2);
    item_count = self->model->op->get_item_count(self->model);
    max_width = 0;
    max_width_1 = 0;

    /* TODO: the PULS could be configured for autosize. In this case we would measure
     * the size of filtered items instead of all items.
     */

    /* Compute the width of the widest item. */
    if ( ! self->column_split)
    {
	for (i = 0; i < item_count; ++i)
	{
	    text = self->model->op->get_display_text(self->model, i);
	    w = vim_strsize(text);
	    if (max_width < w)
		max_width = w;
	}
	self->col0_width = max_width;
	self->col1_width = 0;
    }
    else
    {
	for (i = 0; i < item_count; ++i)
	{
	    start = self->model->op->get_display_text(self->model, i);
	    pos = vim_strchr(start, '\t');
	    if (pos)
	    {
		w = vim_strnsize(start, pos-start);
		if (max_width < w)
		    max_width = w;
		w = vim_strsize(pos+1);
		if (max_width_1 < w)
		    max_width_1 = w;
	    }
	    else
	    {
		w = vim_strsize(start);
		if (max_width < w)
		    max_width = w;
	    }
	}
	self->col0_width = max_width;
	self->col1_width = max_width_1;
    }

    /* calculate the size within limits */
    self->position.height = limit_value(item_count, 1, limit_height);

    if (! self->column_split)
    {
	self->position.width = limit_value(max_width, PULS_MIN_WIDTH, limit_width);
	self->split_width = 0;
    }
    else
    {
	self->position.width = limit_value(max_width + max_width_1 + 2, PULS_MIN_WIDTH, limit_width+1);
	self->split_width = max_width + 2;
	if (self->position.width > limit_width)
	{
	    self->position.width = limit_width;
	    w = (int) (limit_width * 0.4); /* TODO: option first_column_size_max=0.4 */
	    w = limit_value(w, PULS_MIN_WIDTH, max_width + 2);
	    self->split_width = w;
	}
    }

    /* accomodate space for the (outer) border */
    if (self->border)
    {
	/* WIDTH */
	w = self->position.width;
	if (self->border->active[WINBORDER_LEFT])
	    ++w;
	if (self->border->active[WINBORDER_RIGHT])
	    ++w;
	while(w > limit_width && w > PULS_MIN_WIDTH)
	{
	    --self->position.width;
	    --w;
	}
	while(w > limit_width && self->border->active[WINBORDER_LEFT])
	{
	    self->border->active[WINBORDER_LEFT] = 0;
	    --w;
	}
	while(w > limit_width && self->border->active[WINBORDER_RIGHT])
	{
	    self->border->active[WINBORDER_RIGHT] = 0;
	    --w;
	}

	/* HEIGHT */
	w = self->position.height;
	if (self->border->active[WINBORDER_TOP])
	    ++w;
	if (self->border->active[WINBORDER_BOTTOM])
	    ++w;
	while(w > limit_height && w > PULS_MIN_HEIGHT)
	{
	    --self->position.height;
	    --w;
	}
	while(w > limit_height && self->border->active[WINBORDER_TOP])
	{
	    self->border->active[WINBORDER_TOP] = 0;
	    --w;
	}
	while(w > limit_height && self->border->active[WINBORDER_BOTTOM])
	{
	    self->border->active[WINBORDER_BOTTOM] = 0;
	    --w;
	}
    }

    return self->position.width >= PULS_MIN_WIDTH && self->position.height >= PULS_MIN_HEIGHT;
    END_METHOD;
}

    static void
_puls_reposition(_self)
    void*	_self;
    METHOD(PopupList, reposition);
{
    _box_move(&self->position, 1, 1);
    if (! self->aligner)
	self->op->calc_size(self, Columns-1, Rows-1);
    else
    {
	self->op->calc_size(self, self->aligner->limits.width, self->aligner->limits.height);
	self->aligner->op->align(self->aligner, &self->position, self->border);
    }
    END_METHOD;
}

    static void
_puls_update_hl_chain(_self)
    void *_self;
    METHOD(PopupList, update_hl_chain);
{
    ListHelper_T* plst_chain;
    plst_chain  = new_ListHelper();
    plst_chain->first = (void**) &self->hl_chain; /* TODO?: lst_chain.set_list() */
    plst_chain->offs_next = offsetof(Highlighter_T, next);

    self->hl_chain = NULL;

    /* least important highlighter first */
    if (self->hl_user && self->hl_user->active)
	plst_chain->op->add_tail(plst_chain, self->hl_user);

    if (self->hl_menu && self->hl_menu->active)
	plst_chain->op->add_tail(plst_chain, self->hl_menu);

    self->hl_filter->op->set_matcher(self->hl_filter, self->filter->matcher);
    self->hl_filter->match_attr = _puls_hl_attrs[PULSATTR_HL_FILTER].attr;
    plst_chain->op->add_tail(plst_chain, self->hl_filter);

    self->hl_isearch->op->set_matcher(self->hl_isearch, self->isearch->matcher);
    self->hl_isearch->match_attr = _puls_hl_attrs[PULSATTR_HL_SEARCH].attr;
    plst_chain->op->add_tail(plst_chain, self->hl_isearch);

    CLASS_DELETE(plst_chain);
    END_METHOD;
}

    static void
_puls_redraw(_self)
    void *_self;
    METHOD(PopupList, redraw);
{
    int		thumb_heigth;
    int		thumb_pos;
    int		i, idx_filter, idx_model, is_current;
    int		row, col, bottom, right;
    int		attr;
    int		attr_norm   = _puls_hl_attrs[PULSATTR_NORMAL].attr;
    int		attr_select = _puls_hl_attrs[PULSATTR_SELECTED].attr;
    int		menu_mode;
    char_u	*text;
    int		item_count;
    LineHighlightWriter_T* lhwriter;
    LineWriter_T* writer;
    int		hidden, blank, scrollbar;


    item_count = self->filter->op->get_item_count(self->filter);

    hidden = item_count - self->position.height;
    blank = 0;
    if ((self->first + self->position.height) > item_count)
	/* we have empty lines after the last item */
	blank = self->first + self->position.height - item_count;
    hidden += blank;
    scrollbar = hidden > 0;
    right = _box_right(&self->position);
    bottom = _box_bottom(&self->position);
    if (scrollbar)
    {
	thumb_heigth = self->position.height * self->position.height / (item_count + blank);
	if (thumb_heigth == 0)
	    thumb_heigth = 1;
	thumb_pos = (self->first * (self->position.height - thumb_heigth) + hidden / 2) / hidden;
	if (thumb_pos + thumb_heigth > self->position.height)
	    thumb_pos = self->position.height - thumb_heigth;
    }
    self->border->op->prepare_scrollbar(self->border, item_count);
    self->border->op->draw_top(self->border);

    if (self->hl_menu)
	self->hl_menu->active = self->model->has_shortcuts;

    self->op->update_hl_chain(self); /* TODO: update when really needed */
    lhwriter = new_LineHighlightWriter();
    lhwriter->highlighters = self->hl_chain;
    menu_mode = self->hl_menu && self->hl_menu->active;

    writer = (LineWriter_T*) lhwriter;
    writer->offset = self->leftcolumn;
    writer->min_col = self->position.left;
    writer->max_col = _box_right(&self->position);
    writer->tab_size = -2;
    writer->fixed_tabs[0] = -1;
    if (self->column_split)
	writer->op->add_fixed_tab(writer, self->split_width);

    for (i = 0; i < self->position.height; ++i)
    {
	idx_filter = i + self->first;
	is_current = (idx_filter == self->current);
	attr = is_current ? attr_select : attr_norm;

	self->border->op->draw_item_left(self->border, i, self->current);
	row = self->position.top + i;
	col = self->position.left;
	if (idx_filter >= item_count)
	{
	    screen_fill(row, row + 1, col, right+1, ' ', ' ', attr);
	    self->border->op->draw_item_right(self->border, i, self->current);
	    continue;
	}

	idx_model = self->filter->op->get_model_index(self->filter, idx_filter);

	if (self->model->op->has_flag(self->model, idx_model, ITEM_SEPARATOR))
	{
	    /* TODO: separator char setting; maybe as a part of border? */
	    screen_fill(row, row + 1, col, right+1, '-', '-', attr);
	}
	else
	{
	    if (self->model->op->has_flag(self->model, idx_model, ITEM_DISABLED))
		attr = _puls_hl_attrs[is_current ? PULSATTR_DISABLED_SEL : PULSATTR_DISABLED].attr;
	    else if (self->model->op->has_flag(self->model, idx_model, ITEM_MARKED))
		attr = _puls_hl_attrs[is_current ? PULSATTR_MARKED_SEL : PULSATTR_MARKED].attr;
	    else if (self->model->op->has_flag(self->model, idx_model, ITEM_TITLE))
		attr = _puls_hl_attrs[is_current ? PULSATTR_TITLE_SEL : PULSATTR_TITLE].attr;

	    if (menu_mode)
	    {
		if (self->model->op->has_flag(self->model, idx_model, ITEM_DISABLED))
		    self->hl_menu->shortcut_attr = attr;
		else
		    self->hl_menu->shortcut_attr =
			_puls_hl_attrs[is_current ? PULSATTR_SHORTCUT_SEL : PULSATTR_SHORTCUT].attr;
	    }

	    text = self->model->op->get_display_text(self->model, idx_model);
	    writer->op->write_line(writer, text, row, attr, ' ');
	}

	self->border->op->draw_item_right(self->border, i, self->current);
    }
    self->border->op->draw_bottom(self->border);
    self->need_redraw = 0;

    CLASS_DELETE(writer);
    END_METHOD;
}

    static void
_puls_move_cursor(_self)
    void* _self;
    METHOD(PopupList, move_cursor);
{
    int row, col;
    if (self->border->input_active && self->line_edit)
    {
	/* TODO: let the LineEdit place the cursor */
	row = self->line_edit->position.top;
	col = self->line_edit->text ? mb_string2cells(self->line_edit->text, -1) : 0;
	if (col >= self->line_edit->position.width)
	    col = _box_right(&self->line_edit->position);
	else
	    col = self->line_edit->position.left + col;
    }
    else
    {
	row = self->current - self->first;
	if (row < 0 || row >= self->position.height)
	    return;
	row = self->position.top + row;
	col = self->position.left + self->position.width - 1;
    }
    windgoto(row, col);
    END_METHOD;
}

    static void
_puls_set_current(_self, index)
    void* _self;
    int index;
    METHOD(PopupList, set_current);
{
    int item_count = self->filter->op->get_item_count(self->filter);
    if (index < 0)
	index = 0;
    if (index >= item_count)
	index = item_count - 1;
    self->current = index;
    self->need_redraw |= PULS_REDRAW_CURRENT;

    /* TODO: optional scrolloff for the puls */
    if (self->current - self->first >= self->position.height)
    {
	self->first = self->current - self->position.height + 1;
	if (self->first < 0)
	    self->first = 0;
	self->need_redraw |= PULS_REDRAW_ALL;
    }
    else if (self->current < self->first)
    {
	self->first = self->current - 3;
	if (self->first < 0)
	    self->first = 0;
	self->need_redraw |= PULS_REDRAW_ALL;
    }
    END_METHOD;
}

    static int
_puls_refilter(_self, track_item, always_track)
    void* _self;
    int track_item;
    int always_track;
    METHOD(PopupList, refilter);
{
    int item_count;

    item_count = self->filter->op->get_item_count(self->filter);
    self->filter->op->filter_items(self->filter);

    /* Track the position of the current item and try to find it in the
     * filtered list.  When always_track is 0, tracking is done only when the
     * number of matching items grows. */
    if (track_item >= 0)
    {
	if (always_track || item_count <= self->filter->op->get_item_count(self->filter))
	    track_item = self->filter->op->get_index_of(self->filter, track_item);
	else
	    track_item = 0;
    }
    return track_item;
    END_METHOD;
}

    static int
_puls_on_filter_change(_self, data)
    void* _self;
    void* data;
    METHOD(PopupList, on_filter_change);
{
    int icur;

    icur = self->filter->op->get_model_index(self->filter, self->current);

    self->filter->op->set_text(self->filter, self->line_edit->text);

    icur = self->op->refilter(self, icur, 0);
    self->op->set_current(self, icur);

    self->need_redraw |= PULS_REDRAW_ALL;
    /* TODO: if (autosize) pplist->need_redraw |= PULS_REDRAW_RESIZE; */

    return 1;
    END_METHOD;
}

    static int
_puls_do_isearch(_self, dir)
    void* _self;
    int   dir;
    METHOD(PopupList, do_isearch);
{
    int item_count, start, end, step, i, idx_model;
    char_u* text;
    if (! self->filter || ! self->isearch)
	return 0;

    item_count = self->filter->op->get_item_count(self->filter);
    start = self->isearch->start;
    if (dir > 0)
    {
	dir = 1;
	if (start < 0 || start >= item_count)
	    start = 0;
	end = item_count-1;
	if (start >= end)
	    return 0;
    }
    else
    {
	dir = -1;
	if (start < 0 || start >= item_count)
	    start = item_count - 1;
	end = 0;
	if (start <= end)
	    return 0;
    }

    for (step = 0; step < 2; step++)
    {
	for(i = start; i != end + dir; i += dir)
	{
	    idx_model = self->filter->op->get_model_index(self->filter, i);
	    text = self->model->op->get_display_text(self->model, idx_model);
	    if (self->isearch->op->match(self->isearch, text))
	    {
		step = 99; /* FOUND; used below */
		self->op->set_current(self, i);
		break;
	    }
	}
	/* wrap around on isearch */
	end = start;
	start = (dir > 0) ? 0 : item_count - 1;
    }

    if (step < 99)
    {
	self->op->set_current(self, self->isearch->start);
	return 0;
    }
    return 1;
    END_METHOD;
}

    static int
_puls_on_isearch_change(_self, data)
    void* _self;
    void* data;
    METHOD(PopupList, on_isearch_change);
{
    self->isearch->op->set_text(self->isearch, self->line_edit->text);

    self->op->do_isearch(self, 1);
    return 1;
    END_METHOD;
}

    static int
_puls_do_command(_self, command)
    void* _self;
    char_u* command;
    METHOD(PopupList, do_command);
{
    int item_count = self->filter->op->get_item_count(self->filter);
    int idx_model_current = self->filter->op->get_model_index(self->filter, self->current);

    int horz_step = self->position.width / 2; /* TODO: horz_step could be configurable */
    if (horz_step < PULS_MIN_WIDTH / 2)
	horz_step = PULS_MIN_WIDTH / 2;

    if (EQUALS(command, "next-item"))
    {
	if (self->current < item_count-1)
	{
	    self->current += 1;
	    self->need_redraw |= PULS_REDRAW_CURRENT;
	    if (self->current - self->first >= self->position.height)
	    {
		self->first = self->current - self->position.height + 1;
		self->need_redraw |= PULS_REDRAW_ALL;
	    }
	}
	return 1;
    }
    else if (EQUALS(command, "prev-item")) {
	if (self->current > 0)
	{
	    self->current -= 1;
	    self->need_redraw |= PULS_REDRAW_CURRENT;
	    if (self->current < self->first)
	    {
		self->first = self->current;
		self->need_redraw |= PULS_REDRAW_ALL;
	    }
	}
	return 1;
    }
    else if (EQUALS(command, "next-page"))
    {
	if (self->current < item_count-1)
	{
	    self->current += self->position.height;
	    if (self->current >= item_count)
		self->current = item_count-1;
	    self->need_redraw |= PULS_REDRAW_CURRENT;
	    if (self->current - self->first >= self->position.height)
	    {
		self->first += self->position.height;
		if (self->first > self->current)
		    self->first = self->current - self->position.height + 1;
		if (self->first < 0)
		    self->first = 0;
		self->need_redraw |= PULS_REDRAW_ALL;
	    }
	}
	return 1;
    }
    else if (EQUALS(command, "prev-page")) {
	if (self->current > 0)
	{
	    self->current -= self->position.height;
	    if (self->current < 0)
		self->current = 0;
	    self->need_redraw |= PULS_REDRAW_CURRENT;
	    if (self->current < self->first)
	    {
		self->first -= self->position.height;
		if (self->first < 0)
		    self->first = 0;
		self->need_redraw |= PULS_REDRAW_ALL;
	    }
	}
	return 1;
    }
    else if (EQUALS(command, "go-home")) {
	if (self->current != 0)
	{
	    self->current = 0;
	    if (self->current < self->first)
		self->first = 0;
	    self->need_redraw |= PULS_REDRAW_ALL;
	}
	return 1;
    }
    else if (EQUALS(command, "go-end")) { /* works as G and zb */
	self->current = item_count - 1;
	if (self->current < 0)
	    self->current = 0;
	self->first = self->current - self->position.height + 1;
	if (self->first < 0)
	    self->first = 0;
	self->need_redraw |= PULS_REDRAW_ALL;
	return 1;
    }
    else if (EQUALS(command, "shift-left")) {
	if (self->leftcolumn > 0)
	{
	    self->leftcolumn -= horz_step;
	    if (self->leftcolumn < 0)
		self->leftcolumn = 0;
	    self->need_redraw |= PULS_REDRAW_ALL;
	}
	return 1;
    }
    else if (EQUALS(command, "shift-right")) {
	/* TODO: limit with max_display_width */
	self->leftcolumn += horz_step;
	self->need_redraw |= PULS_REDRAW_ALL;
	return 1;
    }
    else if (EQUALS(command, "toggle-marked")) {
	int marked = !self->model->op->has_flag(self->model, idx_model_current, ITEM_MARKED);
	self->model->op->set_marked(self->model, idx_model_current, marked);
	self->need_redraw |= PULS_REDRAW_CURRENT;
	return 1;
    }
    else if (EQUALS(command, "auto-resize")) {
	self->op->reposition(self);
	self->need_redraw |= PULS_REDRAW_CLEAR;
	return 1;
    }
    else if (EQUALS(command, "input-bs")) {
	if (self->line_edit)
	{
	    if (self->line_edit->op->backspace(self->line_edit))
		self->line_edit->change_obsrvrs.op->notify(&self->line_edit->change_obsrvrs, NULL);
	}
	return 1;
    }
    else if (EQUALS(command, "input-clear")) {
	if (self->line_edit)
	{
	    if (self->line_edit->len)
	    {
		self->line_edit->op->set_text(self->line_edit, VSTR(""));
		self->line_edit->change_obsrvrs.op->notify(&self->line_edit->change_obsrvrs, NULL);
	    }
	}
	return 1;
    }
    else if (EQUALS(command, "isearch-next") || EQUALS(command, "isearch-prev")) {
	if (self->isearch && *self->isearch->text)
	{
	    int dir;
	    int cur = self->current;
	    dir = EQUALS(command, "isearch-next") ? 1 : -1;
	    self->isearch->start = self->current + dir;
	    if (! self->op->do_isearch(self, dir))
		self->op->set_current(self, cur);
	}
	return 1;
    }
    else if (EQUALS(command, "isearch-next-matcher")) {
	TextMatcher_T* matcher;
	char_u* newname;

#define PFACT self->isearch_matcher_factory
	newname = PFACT->op->next_matcher(PFACT, self->isearch_matcher_name);
	_str_assign(&self->isearch_matcher_name, newname);
	matcher = PFACT->op->create_matcher(PFACT, self->isearch_matcher_name);
#undef PFACT
	if (matcher)
	{
	    self->isearch->op->set_matcher(self->isearch, matcher); /* deletes the old matcher */
	    self->hl_isearch->op->set_matcher(self->hl_isearch, matcher);
	    self->need_redraw |= PULS_REDRAW_ALL;
	}
	return 1;
    }
    else if (EQUALS(command, "filter-next-matcher")) {
	TextMatcher_T* matcher;
	char_u* newname;

#define PFACT self->filter_matcher_factory
	newname = PFACT->op->next_matcher(PFACT, self->filter_matcher_name);
	_str_assign(&self->filter_matcher_name, newname);
	matcher = PFACT->op->create_matcher(PFACT, self->filter_matcher_name);
#undef PFACT
	if (matcher)
	{
	    self->filter->op->set_matcher(self->filter, matcher); /* deletes the old matcher */
	    self->hl_filter->op->set_matcher(self->hl_filter, matcher);
	    self->op->on_filter_change(self, NULL); /* to trigger filtering */
	    self->need_redraw |= PULS_REDRAW_ALL;
	}
	return 1;
    }
    else if (EQUALS(command, "filter-toggle-titles")) {
	if (self->model->has_title_items)
	{
	    self->filter->keep_titles = !self->filter->keep_titles;
	    self->op->on_filter_change(self, NULL); /* to trigger filtering */
	    self->need_redraw |= PULS_REDRAW_ALL;
	}
	return 1;
    }
    else if (EQUALS(command, "select-parent"))
    {
	if (self->model)
	{
	    int rv = self->model->op->select_parent(self->model);
	    LOG(("Select parent"));
	    if (rv >= 0)
	    {
		/* reposition() has to be done before set_current */
		self->op->reposition(self);
		self->first = 0;
		self->op->set_current(self, rv);
		self->need_redraw |= PULS_REDRAW_ALL | PULS_REDRAW_CLEAR;
	    }
	}
	return 1;
    }

    return 0;
    END_METHOD;
}

/*
 * Add character 'c' to buffer "buf".
 * Translates special keys, NUL, CSI, K_SPECIAL and multibyte characters.
 * Copied from getchar.c (add_char_buf)
 */
    static char_u*
key_to_str(c, buf)
    int		c;
    char_u*	buf;
{
#ifdef FEAT_MBYTE
    char_u	bytes[MB_MAXBYTES + 1]; /* XXX: small, but remove from stack anyway */
    int		len;
    int		i;
#endif

#ifdef FEAT_MBYTE
    if (IS_SPECIAL(c))
	len = 1;
    else
	/* XXX: can mb_char2bytes produce K_SPECIAL, K_CSI or IS_SPECIAL() ? */
	len = (*mb_char2bytes)(c, bytes);
    for (i = 0; i < len; ++i)
    {
	if (!IS_SPECIAL(c))
	    c = bytes[i];
#endif

	if (IS_SPECIAL(c) || c == K_SPECIAL || c == NUL)
	{
	    /* translate special key code into three byte sequence */
	    *buf++ = K_SPECIAL;
	    *buf++ = K_SECOND(c);
	    *buf++ = K_THIRD(c);
	}
#ifdef FEAT_GUI
	else if (c == CSI)
	{
	    /* Translate a CSI to a CSI - KS_EXTRA - KE_CSI sequence */
	    *buf++ = CSI;
	    *buf++ = KS_EXTRA;
	    *buf++ = (int)KE_CSI;
	}
#endif
	else
	{
	    *buf++ = c;
	}
#ifdef FEAT_MBYTE
    }
#endif
    *buf = NUL;
    return buf;
}

    static int
_getkey()
{
    int key;
    ++no_mapping;
    ++allow_keys;
    key = safe_vgetc();
    --no_mapping;
    --allow_keys;
    return key;
}

    static int
_nextkey()
{
    int avail;
    ++no_mapping;
    ++allow_keys;
    avail = vpeekc();
    --no_mapping;
    --allow_keys;
    return avail;
}

    static void
_forced_redraw()
{
    /* from ex_redraw */
    int r = RedrawingDisabled;
    int p = p_lz;
    RedrawingDisabled = 0;
    p_lz = FALSE;
    update_topline();
    update_screen(CLEAR);
    RedrawingDisabled = r;
    p_lz = p;
    msg_didout = FALSE;
    msg_col = 0;
    out_flush();
}

    static void
_puls_switch_mode(_self, modename)
    void* _self;
    char_u* modename;
    METHOD(PopupList, switch_mode);
{
    SimpleKeymap_T* modemap;
    int has_input = 0;

    modemap = self->km_normal;
    if (!modename || !*modename || EQUALS(modename, self->km_normal->name))
	modemap = self->km_normal;
    else if (EQUALS(modename, self->km_filter->name))
    {
	modemap = self->km_filter;
	has_input = 1;
    }
    else if (EQUALS(modename, self->km_search->name))
    {
	modemap = self->km_search;
	has_input = 1;
    }
    else if (EQUALS(modename, self->km_shortcut->name))
    {
	if (self->model->has_shortcuts)
	    modemap = self->km_shortcut;
	else
	    modemap = self->km_normal;
    }

    if (modemap == self->modemap)
	return;

    /* exit current mode */
    self->line_edit->change_obsrvrs.op->remove_obj(&self->line_edit->change_obsrvrs, self);

    /* enter new mode */
    self->modemap = modemap;
    if (self->border)
	self->border->op->set_input_active(self->border, has_input);
    if (modemap == self->km_filter)
    {
	self->line_edit->change_obsrvrs.op->add(&self->line_edit->change_obsrvrs,
		self, _puls_on_filter_change);
	self->line_edit->max_len = MAX_FILTER_SIZE; /* TODO: use set_max_len() to trim the current value */
	self->line_edit->op->set_text(self->line_edit, self->filter->text);
    }
    else if (modemap == self->km_search)
    {
	self->isearch->start = self->current;
	self->line_edit->change_obsrvrs.op->add(&self->line_edit->change_obsrvrs,
		self, _puls_on_isearch_change);
	self->line_edit->max_len = MAX_FILTER_SIZE; /* TODO: use set_max_len() to trim the current value */
	self->line_edit->op->set_text(self->line_edit, self->isearch->text);
    }
    END_METHOD;
}

    static int
_puls_process_command(_self, command)
    void* _self;
    char_u* command;
    METHOD(PopupList, process_command);
{
    ItemProvider_T *pmodel;
    WindowBorder_T *pborder;
    pborder = self->border;
    pmodel = self->model;

    /* TODO: find better names for actions
     *    accept -> itemselected/itemactivated
     *    quit   -> cancel
     * TODO: change the names of the result returned from the PULS
     *    accept -> ok         after itemclicked
     *    quit   -> cancel     after escape
     *    abort                when terminated with ctrl-c or similar
     *    done                 the action was already executed by the provider
     * */
    if (EQUALS(command, cmd_quit))
	return PULS_LOOP_BREAK;
    else if (EQUALS(command, cmd_accept))
    {
	int cont;
	int idx_model = self->filter->op->get_model_index(self->filter, self->current);

	if (pmodel->op->has_flag(pmodel, idx_model, ITEM_DISABLED | ITEM_SEPARATOR))
	    return PULS_LOOP_CONTINUE;

	/* select_item(), cont:
	 *	 0 - let the popuplist() caller handle it
	 *	-1 - exit the loop; return the result 'done' (executed by the handler)
	 *	 1 - remain in the event loop; update the items, they may have changed
	 *	 2 - same as 1 & clear the filter
	 */
	cont = pmodel->op->select_item(pmodel, idx_model);
	if (cont == 0)
	    return PULS_LOOP_BREAK;
	else if (cont < 0)
	{
	    /* TODO: change command to "done" */
	    return PULS_LOOP_BREAK;
	}
	else
	{
	    if (cont & 2)
	    {
		self->filter->op->set_text(self->filter, VSTR(""));
		self->line_edit->op->set_text(self->line_edit, VSTR(""));
	    }
	    self->need_redraw |= PULS_REDRAW_ALL | PULS_REDRAW_RESIZE;
	    self->op->set_current(self, 0);
	    return PULS_LOOP_CONTINUE;
	}
    }
    else if (STARTSWITH(command, "accept:"))
    {
	int idx_model = self->filter->op->get_model_index(self->filter, self->current);

	if (pmodel->op->has_flag(pmodel, idx_model, ITEM_DISABLED | ITEM_SEPARATOR))
	    return PULS_LOOP_CONTINUE;

	return PULS_LOOP_BREAK;
    }
    else if (STARTSWITH(command, "done:"))
    {
	return PULS_LOOP_BREAK;
    }
    else if (STARTSWITH(command, "modeswitch:"))
    {
	LOG(("%s", command));
	command += 11; /* skip past ':' */
	self->op->switch_mode(self, command);
	self->need_redraw |= PULS_REDRAW_FRAME;
	return PULS_LOOP_CONTINUE;
    }
    else if (self->op->do_command(self, command))
    {
	return PULS_LOOP_CONTINUE;
    }
    else
    {
	char_u* next_command;
	next_command = pmodel->op->handle_command(pmodel, self, command);
    }

    return PULS_LOOP_CONTINUE;
    END_METHOD;
}


    static int
_puls_test_loop(pplist, rettv)
    PopupList_T* pplist;
    typval_T*    rettv;
{
#define MAX_KEY_SIZE	6*3+1
#define MAX_SEQ_LEN	8*MAX_KEY_SIZE
#define BUF_LEN		32
    char_u *sequence;	/* current input sequence. */
    char_u* buf;	/* temp buffer */
    char_u *ps;
    char_u* command;
    int rv, seq_len, key, found, prev_found, hh, de;
    int avail, sleep_count;
    int ambig_timeout;
    CommandQueue_T* curcmds;
    SimpleKeymap_T *modemap;
    ItemProvider_T *pmodel;
    WindowBorder_T *pborder;
    ItemFilter_T   *pfilter;
    ISearch_T      *psearch;
    dict_T  *dstate;

    buf = (char_u*) alloc(BUF_LEN);
    sequence = (char_u*) alloc(MAX_SEQ_LEN);

    pborder = pplist->border;
    pmodel = pplist->model;
    pfilter = pplist->filter;
    psearch = pplist->isearch;

    /* FIXME: create_matcher could return NULL and filtering will crash ! */
    pfilter->op->set_matcher(pfilter,
	    pplist->filter_matcher_factory->op->create_matcher(
		pplist->filter_matcher_factory, pplist->filter_matcher_name));
    psearch->op->set_matcher(psearch,
	    pplist->isearch_matcher_factory->op->create_matcher(
		pplist->isearch_matcher_factory, pplist->isearch_matcher_name));

    if (pfilter->op->is_active(pfilter))
	pplist->current = pplist->op->refilter(pplist, pplist->current, 1);

    /* make sure the current item is displayed */
    pplist->op->set_current(pplist, pplist->current);
    hh = pplist->position.height / 2;
    if (pplist->current > hh)
    {
	de = pfilter->op->get_item_count(pfilter) - pplist->current;
	if (de < hh)
	    hh = pplist->position.height - de;
	pplist->first = pplist->current - hh;
	if (pplist->first < 0)
	    pplist->first = 0;
    }

    ps = sequence;
    *ps = NUL;
    pplist->need_redraw = PULS_REDRAW_ALL;
    found = KM_NOTFOUND;
    sleep_count = 0;
    ambig_timeout = 0;
    for (;;)
    {
	modemap = pplist->modemap;
	if (pborder)
	{
	    int nf = pfilter->op->get_item_count(pfilter);
	    int nt = pmodel->op->get_item_count(pmodel);
	    char* pending;
	    pending = (found & KM_PREFIX) ? (char*)sequence : NULL;
	    if (nf == nt)
		vim_snprintf((char*)buf, BUF_LEN, "%d/%d%s%s", pplist->current + 1, nt,
			pending ? " " : "", pending ? pending : "");
	    else
		vim_snprintf((char*)buf, BUF_LEN, "%d/%d(%d)%s%s", pplist->current + 1, nf, nt,
			pending ? " " : "", pending ? pending : "");
	    pborder->op->set_info(pborder, buf);
	    if (pplist->need_redraw)
	    {
		if (modemap && pborder->input_active)
		{
		    TextMatcher_T* ptm = NULL;
		    if (modemap == pplist->km_filter)
			ptm = pfilter->matcher;
		    else if (modemap == pplist->km_search)
			ptm = pplist->isearch->matcher;
		    if (ptm)
			sprintf((char*)buf, "%c%c", ptm->mode_char, modemap->mode_char);
		    else
			sprintf((char*)buf, " %c", modemap->mode_char);
		}
		else if (modemap == pplist->km_shortcut)
		    sprintf((char*)buf, "&&");
		else
		    buf[0] = NUL;
		pborder->op->set_mode_text(pborder, buf);
	    }
	}
	if (pplist->need_redraw & PULS_REDRAW_CLEAR)
	{
	    _forced_redraw();
	    pplist->op->redraw(pplist);
	}
	else if (pplist->need_redraw & PULS_REDRAW_RESIZE)
	{
	    _forced_redraw();
	    pplist->op->reposition(pplist);
	    pplist->op->redraw(pplist);
	}
	else if (pplist->need_redraw & PULS_REDRAW_ALL)
	{
	    pplist->op->redraw(pplist);
	}
	else if (pborder)
	{
	    pborder->op->draw_bottom(pborder);
	}
	pplist->op->move_cursor(pplist);

	/* Command processing:
	 *    - queued kbd commands are processed immediately,
	 *    - queued 'macro' commands are processed when there is no input,
	 *    - keys are read when there are no commands in the keyboard command queue.
	 */
	key = NUL;
	avail = (_nextkey() != NUL);
	if (pplist->cmds_keyboard->op->head(pplist->cmds_keyboard))
	{
	    if (got_int)
		break;
	}
	else if (pplist->cmds_macro->op->head(pplist->cmds_macro) && !avail)
	{/* pass */}
	else
	{
	    if (found == KM_AMBIGUOUS)
	    {
		if (got_int)
		    break;

		/* wait for next character or a timeout */
		avail = (_nextkey() != NUL);
		if (!avail && sleep_count < ambig_timeout)
		{
		    do_sleep(20);
		    sleep_count += 20;
		    continue;
		}
		if (!avail)
		{
		    LOG(("found = KM_AMBIGUOUS, end of sleep %d", sleep_count));
		    prev_found = KM_PREFIX;
		    found = KM_MATCH;
		    LOG(("   found -> KM_MATCH, '%s'", sequence));
		}
		else
		{
		    LOG(("found = KM_AMBIGUOUS, GOT SOMETHING! slept: %d, key: %d", sleep_count, avail));
		    key = _getkey();
		    ps = key_to_str(key, ps);
		    prev_found = KM_PREFIX;
		    found = KM_NOTFOUND;
		    /* TODO: if the sequence fails at this time, we should push back key */
		    LOG(("   found -> KM_NOTFOUND, '%s', will check", sequence));
		}
	    }
	    else
	    {
		key = _getkey();
		if (got_int)
		    break;
		ps = key_to_str(key, ps);
	    }

	    seq_len = ps - sequence;
	    if (seq_len < 1)
		continue;

	    if (pmodel->has_shortcuts && modemap == pplist->km_shortcut
		    && !pfilter->op->is_active(pfilter)
		    && vim_iswordp(sequence))
	    {
		int isshrt, next, unique;
		char_u* p = sequence;
		ADVANCE_CHAR_P(p);
		if (*p == NUL) {
		    LOG(("   handle shortcut, '%s'", sequence));
		    isshrt = pmodel->op->find_shortcut(pmodel, sequence, pplist->current+1, &next, &unique);
		    if (isshrt)
		    {
			pplist->op->set_current(pplist, next);
			if (unique)
			    pplist->cmds_keyboard->op->add(pplist->cmds_keyboard, cmd_accept);
		    }
		    ps = sequence;
		    found = KM_NOTFOUND;
		    continue;
		}
	    }

	    prev_found = found;
	    if (found != KM_MATCH)
	    {
		LOG(("checking '%s' '%s'", modemap->name, sequence));
		found = modemap->op->find_key(modemap, sequence);
		if (found == KM_AMBIGUOUS)
		{
		    LOG(("found -> KM_AMBIGUOUS, sleep_count=0"));
		    sleep_count = 0;
		    /* use timeoutlen or ttimeoutlen */
		    ambig_timeout = (p_ttm < 0 ? p_tm : p_ttm);
		    /* XXX: In console a timeout for <esc> is applied twice.
		     * We should detect that vim already timed out on <esc>.
		     * Eg. we could measure the time spent in safe_vgetc() and
		     * subtract from ambig_timeout.
		    if (*sequence == 0x1e && *(sequence+1) == NUL)
		    {
			ambig_timeout = 0;
		    }
		    */
		    continue;
		}
	    }

	    if (found == KM_PREFIX)
	    {
		LOG(("   handle KM_PREFIX, '%s'", sequence));
		if (seq_len > MAX_SEQ_LEN - MAX_KEY_SIZE)
		{
		    ps = sequence;
		    *ps = NUL;
		    found = KM_NOTFOUND;
		}
		continue;
	    }
	    if (found == KM_NOTFOUND)
	    {
		LOG(("   handle KM_NOTFOUND, '%s'", sequence));
		/* TODO: has_insert is just a quick solution; there may be other insertion points
		 * in the future, not just line_edit */
		if (modemap->has_insert && prev_found != KM_PREFIX && !IS_SPECIAL(key))
		{
		    if (pplist->line_edit->op->add_text(pplist->line_edit, sequence))
			pplist->line_edit->change_obsrvrs.op->notify(&pplist->line_edit->change_obsrvrs, NULL);
		}
		ps = sequence;
		*ps = NUL;
		continue;
	    }
	    /* KM_MATCH */
	    LOG(("   handle KM_MATCH, '%s'", sequence));

	    command = modemap->op->get_command(modemap, sequence, 0 /* don't create a copy */ );
	    if (command)
		pplist->cmds_keyboard->op->add(pplist->cmds_keyboard, command);

	    ps = sequence;
	    *ps = NUL;
	    found = KM_NOTFOUND;
	}

	if (pplist->cmds_keyboard->op->head(pplist->cmds_keyboard))
	    curcmds = pplist->cmds_keyboard;
	else if (pplist->cmds_macro->op->head(pplist->cmds_macro))
	    curcmds = pplist->cmds_macro;
	else
	    curcmds = NULL;
	if (curcmds)
	{
	    rv = pplist->op->process_command(pplist, curcmds->op->head(curcmds));
	    if (rv == PULS_LOOP_BREAK)
		break;
	    curcmds->op->pop(curcmds);
	    curcmds = NULL;
	}
    }

    vim_free(sequence);
    vim_free(buf);

    /* TODO: consider that there could be multiple overlapping boxes */
    _forced_redraw();

    dstate = dict_alloc();

    rv = OK;
    if (!dstate)
	rv = FAIL;
    else
    {
	rettv->vval.v_dict = dstate;
	rettv->v_type = VAR_DICT;
	++dstate->dv_refcount;

	if (curcmds)
	{
	    command = curcmds->op->head(curcmds);
	    LOG(("Terminated by command '%s'", command));
	}
	else
	{
	    command = cmd_quit;
	    LOG(("Terminated by 'sigint'"));
	}

	dict_add_nr_str(dstate, "status", 0, command);
	if (pplist->modemap)
	    dict_add_nr_str(dstate, "mode", 0, pplist->modemap->name);

	if (EQUALS(command, "accept") || STARTSWITH(command, "accept:"))
	{
	    pplist->op->prepare_result(pplist, dstate);
	    pplist->op->save_state(pplist, dstate);
	}
	else if (EQUALS(command, cmd_quit) || STARTSWITH(command, "done:"))
	{
	    dict_add_nr_str(dstate, "current", pplist->current, NULL);
	    pplist->op->save_state(pplist, dstate);
	}
    }

    return rv;
}

#if defined(INCLUDE_TESTS)
#include "puls_test.c"
#endif

/*
 *    popuplist({items} [, {title} [, {options} ]])
 *
 *    Process the {items} in a popup window. {items} is a list or a string with
 *    the name of an internal list (eg. buffers). An optional window title
 *    can be set with {title} or with an entry in the {options} dictionary.
 *    The following entries are supported:
 *	options.commads
 *	    The commands that the list can handle.
 *	options.keymap
 *	    The commands can be assigned to keysequences.
 *	options.pos
 *	    The placement of the popup window.
 *	options.mode
 *	    The popuplist mode to start with.
 *	// options.filter
 *	//    The filtering algorithm.
 *
 *  When the list processing is done, the function returns a result in a
 *  dictionary:
 *	rv.status	'accept' or 'cancel'  XXX: ?
 *	rv.mode		the name of the currently active popuplist mode
 *  When rv.status is 'accept':
 *	rv.current	currently selected item
 *	rv.marked	list of marked item indices
 *
 *  More information is in |popuplst.txt|.
 *
 *  Use examples:
 *    let alist = ["1", "2", "three"]
 *    let rv = popuplist(alist, "Some list")
 *    let rv = popuplist(alist, "Some list", { 'pos': '11' })
 *    let rv = popuplist("buffers")
 *    let rv = popuplist("nmenu")
 *
 */
    int
puls_test(argvars, rettv)
    typval_T	*argvars;
    typval_T	*rettv;
{
    BoxAligner_T* aligner = NULL;
    PopupList_T* pplist = NULL;
    ItemProvider_T* model = NULL;
    dictitem_T *option;
    char_u* special_items = NULL;
    char_u* title = NULL;
    list_T* items = NULL;
    dict_T* options = NULL;
    int rv;
    int default_split_columns = 0;
    int default_current = -1;

    /*_init_vtables();*/
    _update_hl_attrs();

    LOG(("---------- New listbox -----------"));

    /* step 1: parse parameters; options dictionary is always last. */
    if (argvars[0].v_type == VAR_LIST)
	items = argvars[0].vval.v_list;
    else if (argvars[0].v_type == VAR_STRING)
	special_items = argvars[0].vval.v_string;
    else
    {
	return FAIL;
	/* TODO: errmsg param 0 must be LIST or STRING */
    }

    if (argvars[1].v_type != VAR_UNKNOWN)
    {
	if (argvars[1].v_type == VAR_STRING)
	    title = argvars[1].vval.v_string;
	else
	{
	    return FAIL;
	    /* TODO: errmsg param 1 must be STRING  */
	}

	if (argvars[2].v_type == VAR_DICT)
	    options = argvars[2].vval.v_dict;
	else if (argvars[2].v_type != VAR_UNKNOWN)
	{
	    return FAIL;
	    /* TODO: errmsg param 2 must be DICT  */
	}
    }

    /* step 2: create an item provider */
    if (!items && special_items)
    {
#ifdef FEAT_POPUPLIST_BUFFERS
	if (EQUALS(special_items, "buffers"))
	{
	    BufferItemProvider_T* bmodel = new_BufferItemProvider();
	    LOG(("Buffers"));
	    bmodel->op->list_buffers(bmodel);
	    model = (ItemProvider_T*) bmodel;
	    default_split_columns = 1;
	}
#endif
#ifdef FEAT_POPUPLIST_MENUS
	if (EQUALS(special_items, "menu") || EQUALS(special_items+1, "menu"))
	{
	    MenuItemProvider_T* mmodel = new_MenuItemProvider();
	    LOG(("Menu"));
	    if (!mmodel->op->parse_mode(mmodel, special_items))
	    {
		CLASS_DELETE(mmodel);
		/* TODO: errmsg: invalid menu mode '%special_items' */
		return FAIL;
	    }
	    mmodel->op->find_menu(mmodel, title);
	    title = NULL;
	    mmodel->op->list_items(mmodel, NULL);
	    model = (ItemProvider_T*) mmodel;
	    default_split_columns = 1;
	}
#endif
#ifdef FEAT_QUICKFIX
	if (EQUALS(special_items, "quickfix") || EQUALS(special_items, "copen"))
	{
	    QuickfixItemProvider_T* qmodel;
	    LOG(("Quick Fix - %s", special_items));
	    if (EQUALS(special_items, "copen"))
	    {
		if (ql_info.qf_listcount < 1)
		{
		    /* TODO: errmsg: no error list */
		    return FAIL;
		}
	    }
	    qmodel = new_QuickfixItemProvider();
	    qmodel->qfinfo = &ql_info;
	    default_current = qmodel->op->list_items(qmodel);
	    model = (ItemProvider_T*) qmodel;
	}
	else if (EQUALS(special_items, "lopen"))
	{
	    QuickfixItemProvider_T* qmodel = new_QuickfixItemProvider();
	    LOG(("Quick Fix - lopen"));
	    if (curwin->w_llist_ref)
		qmodel->qfinfo = curwin->w_llist_ref;
	    else if (curwin->w_llist)
		qmodel->qfinfo = curwin->w_llist;
	    else
	    {
		CLASS_DELETE(qmodel);
		/* TODO: errmsg: no location list associated with the current window */
		return FAIL;
	    }
	    default_current = qmodel->op->list_items(qmodel);
	    model = (ItemProvider_T*) qmodel;
	}
#endif
#if defined(INCLUDE_TESTS)
	if (EQUALS(special_items, str_pulstest))
	{
	    _test_list_helper();
	    _test_command_t();
	    special_items = str_pulslog;
	}
#endif
#if defined(INCLUDE_TESTS) || defined(DEBUG)
	if (EQUALS(special_items, str_pulslog))
	{
	    VimlistItemProvider_T* vlmodel;
	    LOG(("PULS LOG"));
	    if (PULSLOG.lv_refcount < 1)
		PULSLOG.lv_refcount = 1;
	    vlmodel = new_VimlistItemProvider();
	    vlmodel->op->set_list(vlmodel, &PULSLOG);
	    model = (ItemProvider_T*) vlmodel;
	}
#endif

	if (! model)
	{
	    /* TODO: errmsg: invalid item provider '%special_items' */
	    return FAIL;
	}
    }
    else if (items)
    {
	VimlistItemProvider_T* vlmodel = new_VimlistItemProvider();
	vlmodel->op->set_list(vlmodel, items);
	model = (ItemProvider_T*) vlmodel;
    }

    if (! model)
    {
	/* TODO: errmsg: no items defined */
	return FAIL;
    }

    /* step 3: pepare to execute and apply options */

    aligner = new_BoxAligner();
    aligner->op->set_limits(aligner, 0, 0, Columns - 1, Rows - 3);

    pplist = new_PopupList();
    pplist->op->set_model(pplist, model);
    model->op->default_keymap(model, pplist);
    if (title)
       model->op->set_title(model, title);

    pplist->aligner = aligner;
    /* TODO: column_split should be set with a call to the provider */
    /*       eg. model->op->prepare_popuplist(model, pplist) */
    pplist->column_split = default_split_columns;
    pplist->current = default_current;

    if (options)
    {
	pplist->op->read_options(pplist, options);

	option = dict_find(options, VSTR("commands"), -1L);
	if (model && option && option->di_tv.v_type == VAR_DICT)
	{
	    /* XXX: for now, assume the options are const while the popup is running.
	     * Note that a callback could modify the options!
	     */
	    model->commands = option->di_tv.vval.v_dict;
	}

	model->op->read_options(model, options);
    }

    pplist->op->reposition(pplist);
    model->op->on_start(model);

    if (did_emsg)
	rv = FAIL;
    else
	rv = _puls_test_loop(pplist, rettv);

    CLASS_DELETE(pplist);
    CLASS_DELETE(aligner);
    CLASS_DELETE(model);

    return rv;
}

#endif
