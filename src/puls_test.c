/* vi:set ts=8 sts=4 sw=4 noet list:vi */

#include <time.h>

int64_t timespecDiff(struct timespec *timeA_p, struct timespec *timeB_p)
{
  return ((timeA_p->tv_sec * 1000000000) + timeA_p->tv_nsec) -
           ((timeB_p->tv_sec * 1000000000) + timeB_p->tv_nsec);
}

static void _do_test_command_t_speed(char* haystack, char* needle, int count)
{
    int l, i, limit;
    struct timespec start, end;
    double elapsed;
    LOG(("%s", haystack));
    LOG(("%s", needle));
    TextMatcher_T* matcher = (TextMatcher_T*)new_TextMatcherCmdT();
    matcher->op->set_search_str(matcher, needle);
    for (l = 0; l < 5; l++)
    {
	limit = STRLEN(haystack) * l / 4;
	if (limit < 1) limit = -1;
	i = count;
	((TextMatcherCmdT_T*)matcher)->last_retry_offset = limit;
	ulong score = matcher->op->match(matcher, haystack);
	LOG(("  limit: %d, score: %d", limit, score));

	clock_gettime(CLOCK_MONOTONIC, &start);
	while (i > 0)
	{
	    matcher->op->match(matcher, haystack);
	    --i;
	}
	clock_gettime(CLOCK_MONOTONIC, &end);
	elapsed = timespecDiff(&end, &start) * 1e-9;
	LOG(("  clock: %.3lf", elapsed));
    }
    CLASS_DELETE(matcher);
}

static void _do_test_command_t(char* haystack, char* needle)
{
    char_u result[255];
    char_u *p;
    int i;
    vim_memset(result, 0, 255);
    LOG(("%s", haystack));
    LOG(("   find: %s", needle));
    TextMatcherCmdT_T* matcher = new_TextMatcherCmdT();
    matcher->op->set_search_str(matcher, needle);
    ulong score = matcher->op->match(matcher, haystack);
    LOG(("  score: %d", score));
    if (score > 0)
    {
	p = result;
	for(i = 0; i < matcher->_need_len; i++)
	{
	    STRNCPY(p, matcher->_hays_best_positions[i], matcher->_need_char_lens[i]);
	    p += matcher->_need_char_lens[i];
	    *p = '|';
	    ++p;
	}
	*p = 0;
	LOG(("  matched: %s", result));

	p = result;
	for(i = 0; i < matcher->_need_len; i++)
	{
	    STRNCPY(p, matcher->_hays_best_positions[i], matcher->_need_char_lens[i]);
	    p += matcher->_need_char_lens[i];
	}
	*p = 0;
	LOG(("  correct: %s", 0 == strcmp(result, needle) ? "yes" : "NO"));
    }
    CLASS_DELETE(matcher);
}

static void _test_command_t()
{
    LOG(("   TEST COMMAND-T"));
    char hays[] = "the quick brown fox jumps over a lazy dog";
    _do_test_command_t(hays, "t");
    _do_test_command_t(hays, "th");
    _do_test_command_t(hays, "ju");
    _do_test_command_t(hays, "tqbf");
    _do_test_command_t(hays, "qubf");

    LOG(("   TEST COMMAND-T SPEED"));
    char haystack[] = "the quick brown fox jumps over a lazy dog. the quick brown fox jumps over a lazy dog.";
    char needle_a[] = "tqbfjold";
    char needle_b[] = "qubrofo";
    int count = 10000;
    _do_test_command_t_speed (haystack, needle_a, count);
    _do_test_command_t_speed (haystack, needle_b, count);
    _do_test_command_t_speed(hays, "t", count);
    _do_test_command_t_speed(hays, "th", count);
    _do_test_command_t_speed(hays, "ju", count);
    _do_test_command_t_speed(hays, "tqbf", count);
    _do_test_command_t_speed(hays, "qubf", count);
}

typedef struct _test_int_list_item {
    struct _test_int_list_item* next;
    int value;
} Test_IntListItem;

Test_IntListItem* _test_new_IntListItem(int val)
{
    Test_IntListItem* pit = (Test_IntListItem*) alloc(sizeof(Test_IntListItem));
    pit->next = NULL;
    pit->value = val;
    return pit;
}

static int _test_list_items(Test_IntListItem* head, int* values, int len, char_u* name, int notifyok)
{
    int i;
    int llen = 0;
    int good;
    Test_IntListItem *pit = head;
    if (len < 0)
	len = 0;
    while (pit != NULL && llen < len)
    {
	if (pit->value != values[llen])
	    break;
	++llen;
	pit = pit->next;
    }
    good = (len == llen && pit == NULL);
    if (!good || notifyok)
	LOG(("   %4s: %s", (good) ? "ok" : "FAIL", name));
    if (! good)
    {
	char buf[128];
	char *ps = buf;
	*ps = NUL;
	Test_IntListItem *pit = head;
	while (pit != NULL)
	{
	    ps += sprintf(ps, "%2d ", pit->value);
	    pit = pit->next;
	}
	LOG(("      -- got : %s", buf));
	ps = buf;
	*ps = NUL;
	for (i = 0; i < len; i++)
	    ps += sprintf(ps, "%2d ", values[i]);
	LOG(("      -- want: %s", buf));
    }
    return good;
}

static void _test_list_helper()
{
    LOG(("   TEST ListHelper_T"));
    /* TODO: write the tests for the ListHelper_T */
    int stdlist[] = { 1, 2, 3, 3, 4, 5 };
    int lenstdlist = sizeof(stdlist) / sizeof(stdlist[0]);
    int ti, i, rv, ni;
    Test_IntListItem* pit;

    for (ti = 0; ti < 2; ti++)
    {
	Test_IntListItem* first;
	Test_IntListItem* last;
	ListHelper_T* helper;
	helper = new_ListHelper();
	helper->first = (void**) &first;
	switch(ti) {
	    case 0:
		LOG(("   TEST ListHelper_T with head pointer."));
		/*helper->last = NULL;*/
		break;
	    case 1:
		LOG(("   TEST ListHelper_T with head and tail pointers."));
		helper->last = (void**) &last;
		break;
	}
	helper->offs_next = offsetof(Test_IntListItem, next);

	/* TEST: test add_tail */
	first = NULL; last = NULL;
	for (i = 0; i < lenstdlist; i++)
	{
	    helper->op->add_tail(helper, _test_new_IntListItem(stdlist[i]));
	    rv = _test_list_items(first, stdlist, i+1, " iter add_tail", 0);
	}
	rv = _test_list_items(first, stdlist, lenstdlist, "add_tail", 1);

	/* TEST: test add_head */
	first = NULL; last = NULL;
	for (i = lenstdlist-1; i >= 0; i--)
	{
	    helper->op->add_head(helper, _test_new_IntListItem(stdlist[i]));
	    rv = _test_list_items(first, &stdlist[i], lenstdlist-i, " iter add_head", 0);
	}
	rv = _test_list_items(first, stdlist, lenstdlist, "add_head", 1);

	/* TEST: test remove_head */
	first = NULL; last = NULL;
	for (i = 0; i < lenstdlist; i++)
	    helper->op->add_tail(helper, _test_new_IntListItem(stdlist[i]));
	ni = 3;
	for (i = 0; i < ni; i++)
	{
	    pit = helper->op->remove_head(helper);
	    rv = _test_list_items(first, stdlist+i+1, lenstdlist-i-1, " iter remove_head", 0);
	}
	rv = _test_list_items(first, stdlist+ni, lenstdlist-ni, "remove_head 3", 1);

	/* TEST: test remove_head ALL*/
	first = NULL; last = NULL;
	for (i = 0; i < lenstdlist; i++)
	    helper->op->add_tail(helper, _test_new_IntListItem(stdlist[i]));
	ni = lenstdlist + 1;
	for (i = 0; i < ni; i++)
	{
	    pit = helper->op->remove_head(helper);
	    rv = _test_list_items(first, stdlist+i+1, lenstdlist-i-1, " iter remove_head ALL", 0);
	}
	rv = _test_list_items(first, NULL, 0, "remove_head ALL", 1);
	if (rv)
	{
	    if (helper->first)
		LOG(("       %-4s: first==NULL", first == NULL ? "ok" : "FAIL"));
	    if (helper->last)
		LOG(("       %-4s: last==NULL", last == NULL ? "ok" : "FAIL"));
	}

	/* TODO: test remove_item */
	/* TODO: test remove_all */
    }
}
