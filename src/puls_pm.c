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
 * puls_pm.c: Provider that adds Vim menu items to the Popup list (PULS).
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
  class MenuItemProvider(ItemProvider) [mnupr, variant FEAT_POPUPLIST_MENUS]
  {
    vimmenu_T*  top_menu;  // the initial menu to be displayed; gui_find_menu
    vimmenu_T*  cur_menu;  // the currently displayed menu
    int		mode;      // the mode where the menu is executed (one of MODE_INDEX_*)
    int		flat_view; // show items from all submenus
    void	init();
    // void	destroy();
    // find the root menu to display
    void	find_menu(char_u* menu_path);
    void	update_title();
    int		parse_mode(char_u* command);
    int		list_items(void* selected);
    int		_list_items_r(vimmenu_T* menu, void* selected, int* count, int level);
    int		select_item(int item);
    int		select_parent();
    // void	read_options(dict_T* options);
    void	default_keymap(PopupList* puls);
    char_u*	handle_command(PopupList* puls, char_u* command);
  };
*/

static char_u g_separator[] = "-";

    static void
_mnupr_init(_self)
    void* _self;
    METHOD(MenuItemProvider, init);
{
    self->top_menu = root_menu;
    self->cur_menu = self->top_menu;
    self->mode = MENU_INDEX_NORMAL;
    self->flat_view = 0;
    self->has_title_items = 0;
    self->has_shortcuts = 1;
    END_METHOD;
}

    static void
_mnupr_find_menu(_self, menu_path)
    void*   _self;
    char_u* menu_path;
    METHOD(MenuItemProvider, find_menu);
{
    vimmenu_T* pm;

    if (!menu_path || !*menu_path)
	pm = root_menu;
    else
    {
	pm = gui_find_menu(menu_path); /* XXX: this may create a Vim error !!! */
	if (pm)
	    pm = pm->children;
	else
	    pm = root_menu;
    }

    self->top_menu = pm;
    self->cur_menu = self->top_menu;
    self->op->update_title(self);
    END_METHOD;
}

    static void
_mnupr_update_title(_self)
    void*   _self;
    METHOD(MenuItemProvider, update_title);
{
    char_u* title;
    if (self->cur_menu == root_menu || !self->cur_menu->parent || self->cur_menu->parent == root_menu)
	self->op->set_title(self, VSTR("Menu"));
    else
    {
	title = self->cur_menu->parent->dname;
	if (title && *title == ']')
	    ++title;
	self->op->set_title(self, title);
    }
    END_METHOD;
}

    static int
_mnupr_parse_mode(_self, command)
    void* _self;
    char_u* command;
    METHOD(MenuItemProvider, parse_mode);
{
    if (EQUALS(command, "menu"))
    {
	self->mode = MENU_INDEX_NORMAL;
	return 1;
    }
    if (!EQUALS(command+1, "menu"))
	return 0;

    /* based on get_menu_cmd_modes */
    switch(command[0])
    {
	case 'v':
	    self->mode = MENU_INDEX_VISUAL;
	    return 1;
	case 's':
	    self->mode = MENU_INDEX_SELECT;
	    return 1;
	case 'o':
	    self->mode = MENU_INDEX_OP_PENDING;
	    return 1;
	case 'i':
	    self->mode = MENU_INDEX_INSERT;
	    return 1;
	case 't':
	    self->mode = MENU_INDEX_TIP;
	    return 1;
	case 'c':
	    self->mode = MENU_INDEX_CMDLINE;
	    return 1;
	case 'n':
	    self->mode = MENU_INDEX_NORMAL;
	    return 1;
    }
    return 0;
    END_METHOD;
}

    static int
_mnupr__list_items_r(_self, menu, selected, count, level)
    void* _self;
    vimmenu_T* menu;
    void* selected;
    int*  count;
    int   level;
    METHOD(MenuItemProvider, _list_items_r);
{
    vimmenu_T   *pm, *ppar;
    vimmenu_T** parents;
    PopupItem_T* pit;
    int len, d, isel, is_submenu;
    int loops, iloop, item_types; /* 0x01-normal, 0x02-submenu, 0x04-disabled */
    int mode_flag = (1 << self->mode);
    static char submenu_icon = '+';

    if (self->flat_view)
    {
	loops = 2;
	item_types = 0x01;
	parents = (vimmenu_T**) alloc(sizeof(vimmenu_T*) * 10); /* max supported menu depth */
    }
    else
    {
	loops = 1;
	item_types = 0x07;
	parents = NULL;
    }

    isel = -1;
    for (iloop = 0; iloop < loops; ++iloop)
    {
	if (iloop == 1)
	    item_types = 0x02;
	for (pm = menu; pm != NULL; pm = pm->next)
	{
	    /* menu_is_hidden is static; the condition ']' is copied from there */
	    if (!pm->dname || *pm->dname == ']' || menu_is_popup(pm->dname)  || menu_is_toolbar(pm->dname))
		continue;
	    if (menu_is_separator(pm->dname))
	    {
		if (!(item_types & 0x04))
		    continue;
		pit = self->op->append_pchar_item(self, g_separator, ITEM_SHARED);
		if (pit)
		    pit->flags |= ITEM_SEPARATOR;
	    }
	    else
	    {
		is_submenu = (pm->children != NULL);
		if (!is_submenu && !(item_types & 0x01))
		    continue;
		if (is_submenu && !(item_types & 0x02))
		    continue;

		if (!self->flat_view)
		    len = vim_snprintf((char *)IObuff, IOSIZE, "%c %s",
			    is_submenu ? submenu_icon : ' ', pm->name);
		else
		{
		    if (is_submenu && level > 0)
		    {
			len = vim_snprintf((char *)IObuff, IOSIZE, "%c ",
				is_submenu ? submenu_icon : ' ');
			d = level;
			if (d > 9)
			{
			    d = 9;
			    len += vim_snprintf((char *)(IObuff + len), IOSIZE - len, "...");
			}
			parents[d] = pm;
			ppar = pm->parent;
			while (ppar && d > 0)
			{
			    --d;
			    parents[d] = ppar;
			    ppar = ppar->parent;
			}
			while (d <= level)
			{
			    len += vim_snprintf((char *)(IObuff + len), IOSIZE - len, "%s%s",
				    parents[d]->dname, (d == level) ? "" : ".");
			    ++d;
			}
		    }
		    else
		    {
			len = vim_snprintf((char *)IObuff, IOSIZE, "%c %s",
				is_submenu ? submenu_icon : ' ',
				pm->dname);
		    }
		}
		pit = self->op->append_pchar_item(self, vim_strsave(IObuff), !ITEM_SHARED);
		if (pit)
		{
		    pit->data = (void*)pm;
		    if (!(pm->modes & mode_flag) || !(pm->enabled & mode_flag))
		    {
			pit->flags |= ITEM_DISABLED;
		    }

		    if (self->flat_view && pm->children)
			pit->flags |= ITEM_TITLE;

		    if (pm == selected && isel < 0)
			isel = *count;
		    ++(*count);
		}
		if (self->flat_view && pm->children)
		{
		    int subsel = self->op->_list_items_r(self, pm->children, selected, count, level+1);
		    if (isel < 0)
			isel = subsel;
		}
	    }
	}
    }

    vim_free(parents);
    return isel;
    END_METHOD;
}

    static int
_mnupr_list_items(_self, selected)
    void* _self;
    void* selected;
    METHOD(MenuItemProvider, list_items);
{
    int isel, count, level;
    self->op->clear_items(self);
    count = 0;
    level = 0;
    isel = self->op->_list_items_r(self, self->cur_menu, selected, &count, level);
    return isel;
    END_METHOD;
}

    static int
_mnupr_select_item(_self, item)
    void* _self;
    int item;
    METHOD(MenuItemProvider, select_item);
{
    PopupItem_T* pit;
    vimmenu_T* pm;
    pit = self->op->get_item(self, item);
    if (!pit || !pit->data)
	return 1; /* continue event loop because the selected item is invalid */

    pm = (vimmenu_T*) pit->data;
    if (pm->children)
    {
	/* When a submenu is selected we could create a new popuplist and start
	 * a new event loop. When the child puls would finish, we would return 
	 * either 1 to continue (show parent), or -1 if the child already executed
	 * a command. If the child puls would be cancelled, we would have to return
	 * another value (eg. -2). The main loop would also have to redraw the whole
	 * window stack (when we return 1) which is not implemented, yet.
	 * ATM we replace the contents of the current puls and continue. */
	self->cur_menu = pm->children;
	self->op->update_title(self);
	self->op->list_items(self, NULL);
	return 2;
    }
    else
    {
	int mode = self->mode;

	if (mode != MENU_INDEX_INVALID && pm->strings[mode] != NULL)
	{
	    /* If called as 'vmenu', restore the Visual mode (noremap, silent) */
	    if (mode == MENU_INDEX_VISUAL)
		exec_normal_cmd(VSTR("gv"), REMAP_NONE, 1);

	    /* This works both in console and in gui */
	    exec_normal_cmd(pm->strings[mode], pm->noremap[mode], pm->silent[mode]);
	}

	return -1;
    }
    END_METHOD;
}

    static int
_mnupr_select_parent(_self)
    void* _self;
    METHOD(MenuItemProvider, select_parent);
{
    vimmenu_T *cur, *parent;
    int icur;

    if (!self->cur_menu || !self->cur_menu->parent)
	return -1; /* no parent, ignore */
    if (self->cur_menu == self->top_menu)
	return -1; /* not allowed to go beyond the parent */

    parent = self->cur_menu->parent;
    cur = parent; /* we want to highlight this one in the parent menu */
    if (parent->parent)
	parent = parent->parent->children; /* the first menu entry in the parent menu */
    else
	parent = self->top_menu;
    self->cur_menu = parent;
    self->op->update_title(self);
    icur = self->op->list_items(self, cur);
    if (icur < 0)
	icur = 0;

    return icur;
    END_METHOD;
}

    static char_u*
_mnupr_handle_command(_self, puls, command)
    void*	    _self;
    PopupList_T*    puls;
    char_u*	    command;
    METHOD(MenuItemProvider, handle_command);
{
    if (EQUALS(command, "menu-toggle-flat-view"))
    {
	self->flat_view = !self->flat_view;
	self->has_title_items = self->flat_view;
	self->has_shortcuts = !self->flat_view;
	self->op->update_title(self);
	self->op->list_items(self, NULL);
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
_mnupr_default_keymap(_self, puls)
    void* _self;
    PopupList_T* puls;
    METHOD(MenuItemProvider, default_keymap);
{
    SimpleKeymap_T* modemap;

    /* TODO: next mode depens on self->flat_view */
    modemap = puls->km_normal;
    modemap->op->set_key(modemap, VSTR("*"), VSTR("menu-toggle-flat-view|auto-resize|modeswitch:filter"));
    modemap = puls->km_shortcut;
    modemap->op->set_key(modemap, VSTR("*"), VSTR("menu-toggle-flat-view|auto-resize|modeswitch:filter"));
    END_METHOD;
}
