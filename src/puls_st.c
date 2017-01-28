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
 * puls_st.c: Structures for the Popup list (PULS)
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
  typedef void (*Destroy_Fn)(void* _self);
  typedef int (*ItemComparator_Fn)(void* _self, void* a, void* b);
  typedef int (*ItemMatcher_Fn)(void* _self, void* item);

  // Comparator with double interface: either use fn_compare+extra or reimplement compare().
  class ItemComparator [icmprtr]
  {
    ItemComparator_Fn fn_compare;
    void* extra;      // optional extra data
    int   reverse;
    void  init();
    // void  destroy();
    int   compare(void* a, void* b);
  };

  // Matcher with double interface: either use fn_match+extra or reimplement match().
  class ItemMatcher [imtchr]
  {
    ItemMatcher_Fn fn_match;
    void* extra;      // optional extra data
    int   reverse;
    void  init();
    int   match(void* item);
  };
*/

    static void
_icmprtr_init(_self)
    void* _self;
    METHOD(ItemComparator, init);
{
    self->fn_compare = NULL;
    self->reverse = 0;
    self->extra = NULL;
    END_METHOD;
}


    static int
_icmprtr_compare(_self, a, b)
    void* _self;
    void* a;
    void* b;
    METHOD(ItemComparator, compare);
{
    int rv = 0;
    if (self->fn_compare)
	rv = (*self->fn_compare)(_self, a, b);
    return self->reverse ? (-rv) : rv;
    END_METHOD;
}

    static void
_imtchr_init(_self)
    void* _self;
    METHOD(ItemMatcher, init);
{
    self->fn_match = NULL;
    self->reverse = 0;
    self->extra = NULL;
    END_METHOD;
}

    static int
_imtchr_match(_self, item)
    void* _self;
    void* item;
    METHOD(ItemMatcher, match);
{
    int rv = 1;
    if (self->fn_match)
	rv = (*self->fn_match)(_self, item);
    return self->reverse ? (!rv) : rv;
    END_METHOD;
}

/* [ooc]
 *
  class ListHelper [lsthlpr]
  {
    void**  first;         // points to the pointer to the first item of the list
    void**  last;          // ponts to the pointer to the last item of the list (optional)
    Destroy_Fn fn_destroy; // a function to destroy the items in delete_xxx
    short   offs_next;     // offsetof(next) in item
    // short   offs_prev;     // offsetof(prev) in item (optional)
    void    init();
    // void    destroy();
    void    add_head(void* item);
    void    add_tail(void* item);
    void*   remove(void* item);
    void*   remove_head();
    // void*   find(ItemMatcher* cond);
    // void*   remove_first(ItemMatcher* cond);
    // int     delete_first(ItemMatcher* cond, Destroy_Fn fn_destroy);
    int     _rem_del_all(ItemMatcher* cond, int dodel);
    int     remove_all(ItemMatcher* cond);
    int     delete_all(ItemMatcher* cond);
  };
*/

    static void
_lsthlpr_init(_self)
    void* _self;
    METHOD(ListHelper, init);
{
    self->first = NULL;
    self->last = NULL;
    self->fn_destroy = NULL;
    self->offs_next = -1;
    /*self->offs_prev = -1;*/
    END_METHOD;
}

    static void
_lsthlpr_add_head(_self, item)
    void* _self;
    void* item;
    METHOD(ListHelper, add_head);
{
    /* ASSERT(self->first && self->offs_next >= 0) */
    void *pit = *self->first;
    *self->first = item;
    *(void**)((char*)item + self->offs_next) = pit;            /* item->next = pit */
    if (self->last && ! *self->last)
	*self->last = pit;
    END_METHOD;
}

    static void
_lsthlpr_add_tail(_self, item)
    void* _self;
    void* item;
    METHOD(ListHelper, add_tail);
{
    /* ASSERT(self->first && self->offs_next >= 0) */
    void *pit;
    short offnext = self->offs_next;

    if (! *self->first)
    {
	*self->first = item;
	if (self->last)
	    *self->last = item;
    }
    else
    {
	if (self->last)
	{
	    pit = *self->last;
	    *self->last = item;
	}
	else
	{
	    pit = *self->first;
	    while (*(void**)((char*)pit + offnext) != NULL) /* while pit->next != NULL */
	        pit = *(void**)((char*)pit + offnext);      /* pit = pit->next */
	}
	*(void**)((char*)pit + offnext) = item;         /* pit->next = item */
    }
    *(void**)((char*)item + offnext) = NULL;            /* item->next = NULL */
    END_METHOD;
}

    static void*
_lsthlpr_remove(_self, item)
    void* _self;
    void* item;
    METHOD(ListHelper, remove);
{
    /* ASSERT(self->first && self->offs_next >= 0) */
    void *pit, *pnext;
    short offnext = self->offs_next;
    if (*self->first == NULL)
	return NULL;
    else if (*self->first == item)
    {
	pit = *self->first;
	*self->first = *(void**)((char*)self->first + offnext); /* first = first->next */
	if (self->last && ! *self->first)
	    *self->last = NULL;
	return pit;
    }
    else
    {
	pit = *self->first;
	while (*(void**)((char*)pit + offnext) != NULL)   /* while pit->next != NULL */
	{
	    pnext = *(void**)((char*)pit + offnext);
	    if (pnext == item)  /* if pit->next == item */
	    {
		/* pit->next = pit->next->next  ... = item->next*/
		*(void**)((char*)pit + offnext) = *(void**)((char*)pnext + offnext);
		if (self->last && *self->last == item)
		    *self->last = pit;
		return pnext;
	    }
	    pit = pnext;        /* pit = pit->next */
	}
    }
    return NULL;
    END_METHOD;
}

    static void*
_lsthlpr_remove_head(_self)
    void* _self;
    METHOD(ListHelper, remove_head);
{
    void* pit;
    /* ASSERT(self->first && self->offs_next >= 0) */
    if (*self->first)
    {
	pit = *self->first;
	*self->first = *(void**)((char*)pit + self->offs_next); /* first = first->next */
	if (self->last && ! *self->first)
	    *self->last = NULL;
	return pit;
    }
    return NULL;
    END_METHOD;
}

    static int
_lsthlpr__rem_del_all(_self, cond, dodel)
    void* _self;
    ItemMatcher_T* cond;
    int dodel;
    METHOD(ListHelper, _rem_del_all);
{
    /* ASSERT(self->first && self->offs_next >= 0) */
    void *pit, *pnext, *pdel;
    short offnext = self->offs_next;
    int count = 0;
    Destroy_Fn destroy = dodel ? self->fn_destroy : NULL;
    pit = *self->first;
    while (pit && (!cond || cond->op->match(cond, pit)))
    {
	pdel = pit;
	pit = *(void**)((char*)pit + offnext);    /* pit = pit->next */
	if (self->last && *self->last == pdel)
	    *self->last = NULL;
	if (dodel)
	{
	    if (destroy)
		(*destroy)(pdel);
	    vim_free(pdel);
	}
	++count;
    }
    *self->first = pit;
    pnext = pit ? *(void**)((char*)pit + offnext) : NULL;
    while (pnext)
    {
	if (!cond || cond->op->match(cond, pnext))
	{
	    pdel = pnext;
	    /* pit->next = pit->next->next */
	    *(void**)((char*)pit + offnext) = *(void**)((char*)pnext + offnext);
	    if (self->last && *self->last == pdel)
		*self->last = pit;
	    if (dodel)
	    {
		if (destroy)
		    (*destroy)(pdel);
		vim_free(pdel);
	    }
	    ++count;
	}
	else
	    pit = pnext;
	pnext = *(void**)((char*)pit + offnext);
    }

    return count;
    END_METHOD;
}

    static int
_lsthlpr_remove_all(_self, cond)
    void* _self;
    ItemMatcher_T* cond;
    METHOD(ListHelper, remove_all);
{
    return _lsthlpr__rem_del_all(_self, cond, 0);
    END_METHOD;
}

    static int
_lsthlpr_delete_all(_self, cond)
    void* _self;
    ItemMatcher_T* cond;
    METHOD(ListHelper, delete_all);
{
    return _lsthlpr__rem_del_all(_self, cond, 1);
    END_METHOD;
}



/* [ooc]
 *
  class DictIterator [dicti]
  {
    dict_T*	dict;
    hashitem_T*	current;
    int		todo;
    void	init();
    void	destroy();
    dictitem_T* begin(dict_T* dict);
    dictitem_T* next();
  }
*/

    static void
_dicti_init(_self)
    void* _self;
    METHOD(DictIterator, init);
{
    self->dict = NULL;
    self->current = NULL;
    END_METHOD;
}

    static void
_dicti_destroy(_self)
    void* _self;
    METHOD(DictIterator, destroy);
{
    self->dict = NULL;
    self->current = NULL;
    END_DESTROY(DictIterator);
}

    static dictitem_T*
_dicti_begin(_self, dict)
    void* _self;
    dict_T* dict;
    METHOD(DictIterator, begin);
{
    self->dict = dict;
    self->todo = (int)dict->dv_hashtab.ht_used;
    self->current = dict->dv_hashtab.ht_array;
    if (self->todo < 1)
	return NULL;
    if (HASHITEM_EMPTY(self->current))
	return self->op->next(self);

    --self->todo;
    return HI2DI(self->current);
    END_METHOD;
}

    static dictitem_T*
_dicti_next(_self)
    void* _self;
    METHOD(DictIterator, next);
{
    if (self->todo < 1)
	return NULL;
    ++self->current;
    while (HASHITEM_EMPTY(self->current))
	++self->current;

    --self->todo;
    return HI2DI(self->current);
    END_METHOD;
}

/* [ooc]
 *
  // A compromise between a list and a grow-array.  An array of items with
  // identical size.  New segments are added to grow the array.  No
  // reallocation is done for the existing items and their addresses remain
  // fixed.
  const SGARR_MIN_SEGMENT_ITEM_COUNT = 16;
  class SegmentedGrowArray [sgarr]
  {
    // TODO: make all the fields 'private' and add a function to configure the array
    //    configure(item_size[bytes], preferred_segment_size[bytes], fn_destroy);
    // TODO: A segment should contain at least SGARR_MIN_SEGMENT_ITEM_COUNT items.
    Destroy_Fn fn_destroy;      // a function that will destroy each item
    int	    item_size;		// size of every item
    int	    len;		// actual number of items used
    int	    segment_len;	// number of items in a segment
    int	    index_size;		// number of segment pointers (some may be unused)
    int	    index_len;		// number of segments allocated
    void**  index;
    void    init();
    void    destroy();
    void    clear();		// clears the items in the array and frees the space used by the items
    void    clear_contents();	// clears the items in the array, but keeps the space
    void    truncate();		// frees the unused blocks allocated for the items
    int	    grow(int count);
    void*   get_new_item();
    void*   get_item(int index);
    void    sort(ItemComparator* cmp);
    void    _qsort(int low, int high, ItemComparator* cmp);
  };
*/

    static void
_sgarr_init(_self)
    void* _self;
    METHOD(SegmentedGrowArray, init);
{
    self->index = NULL;
    self->fn_destroy = NULL;
    self->len = 0;
    self->item_size = 0;
    self->index_size = 0;
    self->index_len = 0;
    self->segment_len = 0;
    END_METHOD;
}

    static void
_sgarr_clear_contents(_self)
    void* _self;
    METHOD(SegmentedGrowArray, clear_contents);
{
    int i, j, len;
    void* pseg;
    if (self->fn_destroy && self->index && self->len > 0)
    {
	len = self->len;
	for(i = 0; i < self->index_len && len > 0; i++)
	{
	    pseg = self->index[i];
	    for(j = 0; j < self->segment_len && len > 0; j++)
	    {
		(*self->fn_destroy)(pseg);
		pseg = (char*)pseg + self->item_size;
		--len;
	    }
	}
    }
    self->len = 0;
    END_METHOD;
}

    static void
_sgarr_clear(_self)
    void* _self;
    METHOD(SegmentedGrowArray, clear);
{
    int i;

    if (self->index)
    {
	if (self->len)
	    self->op->clear_contents(self);

	for (i = 0; i < self->index_len; i++)
	    vim_free(self->index[i]);
	vim_free(self->index);
    }
    self->index = NULL;
    self->index_size = 0;
    self->index_len = 0;
    self->segment_len = 0;
    END_METHOD;
}

    static void
_sgarr_truncate(_self)
    void* _self;
    METHOD(SegmentedGrowArray, truncate);
{
    int req_idxlen;

    if (self->len < 1 || self->item_size < 1)
    {
	self->op->clear(self);
	return;
    }

    if (self->len < 1 && self->segment_len < 1)
	self->segment_len = 1024 / self->item_size;

    req_idxlen = (self->len + self->segment_len - 1) / self->segment_len;
    while (self->index_len > req_idxlen)
    {
	--self->index_len;
	vim_free(self->index[self->index_len]);
	self->index[self->index_len] = NULL;
    }
    END_METHOD;
}

    static void
_sgarr_destroy(_self)
    void* _self;
    METHOD(SegmentedGrowArray, destroy);
{
    if (self->index)
	self->op->clear(self);
    END_DESTROY(SegmentedGrowArray);
}

    static int
_sgarr_grow(_self, count)
    void* _self;
    int count;
    METHOD(SegmentedGrowArray, grow);
{
    int size, newlen, new_idxlen;
    void* pseg;
    if (count < 0)
	return FAIL;

    if (self->len < 1 && self->segment_len < 1)
	self->segment_len = 1024 / self->item_size;

    size = self->index_len * self->segment_len;
    newlen = self->len + count;
    if (newlen <= size)
    {
	self->len = newlen;
	return OK;
    }

    new_idxlen = (newlen + self->segment_len - 1) / self->segment_len;
    if (new_idxlen > self->index_size)
    {
	/* reallocate the index */
	self->index_size *= 2;
	if (self->index_size < new_idxlen)
	    self->index_size = (new_idxlen * 2 / 16 + 1) * 16;
	if (! self->index)
	    self->index = (void**) alloc(self->index_size * sizeof(void*));
	else
	    self->index = (void**) vim_realloc(self->index, self->index_size * sizeof(void*));
    }

    /* allocate the necessary segments */
    while (self->index_len < new_idxlen)
    {
	pseg = (void*) alloc(1024);
	if (! pseg)
	    return FAIL;
	self->index[self->index_len] = pseg;
	++self->index_len;
    }

    self->len = newlen;
    return OK;
    END_METHOD;
}

    static void*
_sgarr_get_new_item(_self)
    void* _self;
    METHOD(SegmentedGrowArray, get_new_item);
{
    void* pseg;
    int ii;
    if (self->op->grow(self, 1) != OK)
	return NULL;
    ii = self->len - 1;
    pseg = self->index[ii / self->segment_len];
    ii = ii % self->segment_len;

    return (void*)((char*)pseg + ii * self->item_size);
    END_METHOD;
}

    static void*
_sgarr_get_item(_self, index)
    void* _self;
    int index;
    METHOD(SegmentedGrowArray, get_item);
{
    void* pseg;
    int ii;
    if (! self->index || index < 0 || index >= self->len)
	return NULL;
    pseg = self->index[index / self->segment_len];
    if (!pseg)
	return NULL;
    ii = index % self->segment_len;

    return (void*)((char*)pseg + ii * self->item_size);
    END_METHOD;
}

    static void
_sgarr_sort(_self, cmp)
    void* _self;
    ItemComparator_T* cmp;
    METHOD(SegmentedGrowArray, sort);
{
    if (self->len > 1)
	self->op->_qsort(self, 0, self->len-1, cmp);
    END_METHOD;
}

/* FIXME: if cmp doesn't give consistent results (is not a well-ordering), _qsort will crash.
 * Example: Because of a bug in _flcmpttsc_compare a comparison was done
 * between filter_parent_score and filter_score. This caused the stack to grow
 * to 7k items and _qsort crashed. */
    static void
_sgarr__qsort (_self, low, high, cmp)
    void* _self;
    int low;
    int high;
    ItemComparator_T* cmp;
    METHOD(SegmentedGrowArray, _qsort);
{
    typedef struct _stackitem_ {
	int low;
	int high;
	int l;
	int r;
	int todo;
    } StackItem_T;
    StackItem_T* stack;
    StackItem_T* ps;
    int stackitem;
    int item_size, l, r, mid, ni, select;
    void *pl, *pr, *pmid;
    char *tmp, *pivot;
    int i, j, step; /* shell sort */

#define QS_PART	    0x01
#define QS_LEFT	    0x02
#define QS_RIGHT    0x04
#define QS_SIMPLE_SIZE 16
#define QS_STACK_SIZE  64
#define QS_PUSH(ilow, ihigh) { \
    ++stackitem; \
    stack[stackitem].low = ilow; \
    stack[stackitem].high = ihigh; \
    stack[stackitem].todo = 0xff; \
    }
#define QS_POP() --stackitem
#define QS_TOP() stack[stackitem]
#define QS_CALL_SSORT(ilow, ihigh, iret) { \
    l = ilow; /* ssret = iret; r = ihigh; ni = r - l + 1; */ \
    goto simple_sort; \
    }

    if (high <= low)
       	return;

    item_size = self->item_size;
    tmp = (void*) alloc(item_size);
    pivot = (void*) alloc(item_size);
    stack = (StackItem_T*) alloc(sizeof(StackItem_T) * QS_STACK_SIZE);
    stackitem = -1;
    QS_PUSH(low, high);

    while(stackitem >= 0)
    {
	ps = &QS_TOP();

	if (ps->todo & QS_PART)
	{
	    /*LOG(("qs %2d: PART %3d - %3d", stackitem, ps->low, ps->high));*/
	    ps->todo &= ~QS_PART;
	    l = ps->low;
	    r = ps->high;
	    mid = (l+r) / 2;
	    pmid = self->op->get_item(self, mid);
	    memcpy(pivot, pmid, item_size);

	    while (l < r)
	    {
		pl = self->op->get_item(self, l);
		while (cmp->op->compare(cmp, pl, pivot) < 0)
		{
		    ++l;
		    pl = self->op->get_item(self, l);
		}
		pr = self->op->get_item(self, r);
		while (cmp->op->compare(cmp, pivot, pr) < 0)
		{
		    --r;
		    pr = self->op->get_item(self, r);
		}
		if (l <= r)
		{
		    if (l != r)
		    {
			memcpy(tmp, pl, item_size);
			memcpy(pl, pr, item_size);
			memcpy(pr, tmp, item_size);
		    }
		    ++l;
		    --r;
		}
	    }
	    ps->l = l;
	    ps->r = r;
	}

	select = ps->todo & (QS_LEFT | QS_RIGHT);
	if (select == (QS_LEFT | QS_RIGHT))
	{
	    /* select the smaller interval */
	    if (ps->r - ps->low < ps->high - ps->l)
		select = QS_LEFT;
	    else
		select = QS_RIGHT;
	}
	else if (select && stackitem > 0)
	{
	    /* The smaller interval has been sorted. If the parent has both
	     * intervals 'done', then when the remaining (larger) interval is
	     * sorted, the parent will also be sorted. We can thus replace the
	     * parent info with current info on the stack. In fact we can
	     * replace all such consequtive parents. */
	    i = stackitem;
	    while (i > 0 && (stack[i-1].todo & (QS_LEFT | QS_RIGHT)) == 0)
		i--;
	    if (i != stackitem)
	    {
		stack[i] = stack[stackitem];
		stackitem = i;
		ps = &QS_TOP();
	    }
	}

	if (select == QS_LEFT)
	{
	    /*LOG(("qs %2d: LEFT %3d - %3d", stackitem, ps->low, ps->high));*/
	    ps->todo &= (~QS_LEFT & 0xff);
	    ni = ps->r - ps->low + 1;
	    if (ni > 1)
	    {
		if (ni > QS_SIMPLE_SIZE)
		{
		    QS_PUSH(ps->low, ps->r);
		}
		else 
		    QS_CALL_SSORT(ps->low, ps->r, 1);
	    }
	}
	else if (select == QS_RIGHT)
	{
	    /*LOG(("qs %2d: RGHT %3d - %3d", stackitem, ps->low, ps->high));*/
	    ps->todo &= (~QS_RIGHT & 0xff);
	    ni = ps->high - ps->l + 1;
	    if (ni > 1)
	    {
		if (ni > QS_SIMPLE_SIZE)
		{
		    QS_PUSH(ps->l, ps->high);
		}
		else
		    QS_CALL_SSORT(ps->l, ps->high, 2);
	    }
	}
	else
	{
	    QS_POP();
	}

ss_ret:
	continue;

simple_sort: /* 'params': ni, l */
	/* shell sort */
	/*LOG(("qs %2d: SHEL %3d - %3d", stackitem, l, l+ni-1));*/
	step = (ni * 5 + 5) / 10; /* round(ni/2) */
	while (step > 0)
	{
	    for (i = step; i < ni; i++)
	    {
		pmid = self->op->get_item(self, l+i);  /* item to move */
		pr = pmid; /* insertion point */
		memcpy(tmp, pmid, item_size); 
		j = i;
		pl = self->op->get_item(self, l+j-step);
		while (cmp->op->compare(cmp, pl, tmp) > 0)
		{
		    memcpy(pr, pl, item_size);
		    pr = pl;
		    j -= step;
		    if (j >= step)
			pl = self->op->get_item(self, l+j-step);
		    else
			break;
		}
		memcpy(pr, tmp, item_size);
	    }
	    step = (step * 10 + 11) / 22; /* round(step/2.2) */
	}
	goto ss_ret;
    }
    vim_free(tmp);
    vim_free(pivot);
    vim_free(stack);
    END_METHOD;
}

    SegmentedGrowArray_T*
new_SegmentedGrowArrayP(int item_size, Destroy_Fn fn_destroy)
{
    SegmentedGrowArray_T* _sgarr = (SegmentedGrowArray_T*) alloc(sizeof(SegmentedGrowArray_T));
    if (! _sgarr)
        return NULL;
    init_SegmentedGrowArray(_sgarr);
    _sgarr->item_size = item_size;
    _sgarr->fn_destroy = fn_destroy;
    return _sgarr;
}

/* [ooc]
 *
  class SegmentedArrayIterator [itsgarr]
  {
    SegmentedGrowArray* array;
    int	    iitem;
    void    init();
    void*   begin(SegmentedGrowArray* container);
    void*   next();
  };
*/

    static void
_itsgarr_init(_self)
    void* _self;
    METHOD(SegmentedArrayIterator, init);
{
    self->array = NULL;
    self->iitem = 0;
    END_METHOD;
}

    static void*
_itsgarr_begin(_self, container)
    void* _self;
    SegmentedGrowArray_T* container;
    METHOD(SegmentedArrayIterator, begin);
{
    void* pseg;
    self->array = (SegmentedGrowArray_T*) container;
    if (!self->array || !self->array->index || self->array->len < 1)
	return NULL;
    self->iitem = 0;
    pseg = self->array->index[0];
    return pseg;
    END_METHOD;
}

    static void*
_itsgarr_next(_self)
    void* _self;
    METHOD(SegmentedArrayIterator, next);
{
    void* pseg;
    if (!self->array || !self->array->index || self->iitem >= self->array->len - 1)
	return NULL;

    ++self->iitem;
    pseg = self->array->index[self->iitem / self->array->segment_len];
    if (!pseg)
	return NULL;

    pseg = (char*)pseg + (self->iitem % self->array->segment_len) * self->array->item_size;
    return pseg;
    END_METHOD;
}

/* [ooc]
 *
  // Observer pattern
  typedef int (*MethodCallback_Fn)(void* _self, void* _data);
  struct NotificationCallback [ntfcb]
  {
    NotificationCallback* next;
    void*   instance_self;
    MethodCallback_Fn callback;
    void init();
    int  call(void* data);
  };

  class NotificationList [ntlst]
  {
    NotificationCallback* observers;
    ListHelper lst_observers;
    void init();
    void destroy();
    void notify(void* _data);
    void add(void* instance, MethodCallback_Fn callback);
    void remove_obj(void* instance);
    void remove_cb(MethodCallback_Fn callback);
  };
*/

    static void
_ntfcb_init(_self)
    void* _self;
    METHOD(NotificationCallback, init);
{
    self->next = NULL;
    self->instance_self = NULL;
    self->callback = NULL;
    END_METHOD;
}

    static int
_ntfcb_call(_self, _data)
    void* _self;
    void* _data;
    METHOD(NotificationCallback, call);
{
    if (! self->callback)
	return 0;
    return (*self->callback)(self->instance_self, _data);
    END_METHOD;
}

    static void
_ntlst_init(_self)
    void* _self;
    METHOD(NotificationList, init);
{
    self->observers = NULL;
    self->lst_observers.first = (void**)&self->observers;
    self->lst_observers.offs_next = offsetof(NotificationCallback_T, next);
    /* items don't need destruction, so we don't set lst_observers.fn_destroy */
    END_METHOD;
}

    static void
_ntlst_destroy(_self)
    void* _self;
    METHOD(NotificationList, destroy);
{
    NotificationCallback_T* pit;
    while (self->observers)
    {
	pit = self->observers;
	self->observers = self->observers->next;
	vim_free(pit);
    }
    END_DESTROY(NotificationList);
}

    static void
_ntlst_notify(_self, _data)
    void* _self;
    void* _data;
    METHOD(NotificationList, notify);
{
    NotificationCallback_T* pit;
    pit = self->observers;
    while (pit)
    {
	_ntfcb_call(pit, _data);
	pit = pit->next;
    }
    END_METHOD;
}

    static void
_ntlst_add(_self, instance, callback)
    void* _self;
    void* instance;
    MethodCallback_Fn callback;
    METHOD(NotificationList, add);
{
    NotificationCallback_T *pnew;
    pnew = new_NotificationCallback();
    pnew->instance_self = instance;
    pnew->callback = callback;
    self->lst_observers.op->add_tail(&self->lst_observers, pnew);
    END_METHOD;
}

    static int
_fn_match_callback_instance(matcher, item)
    ItemMatcher_T* matcher;
    NotificationCallback_T* item;
{
    return (matcher->extra == item->instance_self);
}

    static void
_ntlst_remove_obj(_self, instance)
    void* _self;
    void* instance;
    METHOD(NotificationList, remove_obj);
{
    ItemMatcher_T* pcmp;
    pcmp = new_ItemMatcher();
    pcmp->fn_match = &_fn_match_callback_instance;
    pcmp->extra = instance;
    self->lst_observers.op->delete_all(&self->lst_observers, pcmp);
    CLASS_DELETE(pcmp);
    END_METHOD;
}

typedef struct _callback_holder_t_
{
    MethodCallback_Fn callback;
} _callbackHolder_T;

    static int
_fn_match_callback_callback(matcher, item)
    ItemMatcher_T* matcher;
    NotificationCallback_T* item;
{
    if (!matcher->extra || !item->callback)
	return 0;
    return (((_callbackHolder_T*)matcher->extra)->callback == item->callback);
}

    static void
_ntlst_remove_cb(_self, callback)
    void* _self;
    MethodCallback_Fn callback;
    METHOD(NotificationList, remove_cb);
{
    ItemMatcher_T* pcmp;
    _callbackHolder_T cbholder;
    pcmp = new_ItemMatcher();
    pcmp->fn_match = &_fn_match_callback_callback;
    cbholder.callback = callback;
    pcmp->extra = &cbholder;
    self->lst_observers.op->delete_all(&self->lst_observers, pcmp);
    CLASS_DELETE(pcmp);
    END_METHOD;
}

