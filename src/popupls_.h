/* vim: set ft=c sw=4 ts=8 et fileencoding=utf-8 :vim */
/* NOTE: This file is auto-generated. DO NOT EDIT. The changes will be lost. */

#ifndef ushort
#define ushort unsigned short
#endif
#ifndef ulong
#define ulong  unsigned long
#endif
#ifndef uint
#define uint   unsigned int
#endif
#ifndef offsetof
#define offsetof(st, m) \
     ((size_t) ( (char *)&((st *)(0))->m - (char *)0 ))
#endif

#define SGARR_MIN_SEGMENT_ITEM_COUNT 16
#define ITEM_SHARED 0x01
#define ITEM_MARKED 0x02
#define ITEM_TITLE 0x04
#define ITEM_DISABLED 0x08
#define ITEM_SEPARATOR 0x10
#define ITEM_CONTEXT_FORWARD 0x20
#define ITEM_CONTEXT_BACK 0x40
#define TMWME_MAX_WORDS 16
#define MAX_FILTER_SIZE 127
#define KM_NOTFOUND 0
#define KM_PREFIX 1
#define KM_MATCH 2
#define KM_AMBIGUOUS (KM_PREFIX | KM_MATCH)
#define WINBORDER_TOP 0
#define WINBORDER_RIGHT 1
#define WINBORDER_BOTTOM 2
#define WINBORDER_LEFT 3
#define SCROLLBAR_BAR 0
#define SCROLLBAR_THUMB 1
#define PULS_DEF_HEIGHT 10
#define PULS_DEF_WIDTH 20
#define PULS_MIN_WIDTH 10
#define PULS_MIN_HEIGHT 1
#define PULS_REDRAW_CURRENT 0x01
#define PULS_REDRAW_FRAME 0x02
#define PULS_REDRAW_ALL 0x0f
#define PULS_REDRAW_CLEAR 0x10
#define PULS_REDRAW_RESIZE 0x20
#define PULS_LOOP_BREAK 0
#define PULS_LOOP_CONTINUE 1
#define BUFSORT_NR 'n'
#define BUFSORT_NAME 'm'
#define BUFSORT_MRU 'r'
#define BUFSORT_EXT 'x'
#define BUFSORT_PATH 'p'
#define PLWR_MAX_FIXED_TABS 8

typedef struct _Object_o _Object_o;
typedef struct _ItemComparator_o _ItemComparator_o;
typedef struct _ItemMatcher_o _ItemMatcher_o;
typedef struct _ListHelper_o _ListHelper_o;
typedef struct _DictIterator_o _DictIterator_o;
typedef struct _SegmentedGrowArray_o _SegmentedGrowArray_o;
typedef struct _SegmentedArrayIterator_o _SegmentedArrayIterator_o;
typedef struct _NotificationCallback_o _NotificationCallback_o;
typedef struct _NotificationList_o _NotificationList_o;
typedef struct _CommandQueueItem_o _CommandQueueItem_o;
typedef struct _CommandQueue_o _CommandQueue_o;
typedef struct _PopupItem_o _PopupItem_o;
typedef struct _ItemProvider_o _ItemProvider_o;
typedef struct _VimlistItemProvider_o _VimlistItemProvider_o;
typedef struct _Box_o _Box_o;
typedef struct _LineEdit_o _LineEdit_o;
typedef struct _TextMatcher_o _TextMatcher_o;
typedef struct _TextMatcherRegexp_o _TextMatcherRegexp_o;
typedef struct _TmWordMatchExpr_o _TmWordMatchExpr_o;
typedef struct _TextMatcherWords_o _TextMatcherWords_o;
typedef struct _TextMatcherCmdT_o _TextMatcherCmdT_o;
typedef struct _TextMatcherFactoryEntry_o _TextMatcherFactoryEntry_o;
typedef struct _TextMatcherFactory_o _TextMatcherFactory_o;
typedef struct _ISearch_o _ISearch_o;
typedef struct _FltComparator_Score_o _FltComparator_Score_o;
typedef struct _FltComparator_TitleScore_o _FltComparator_TitleScore_o;
typedef struct _ItemFilter_o _ItemFilter_o;
typedef struct _BoxAligner_o _BoxAligner_o;
typedef struct _SimpleKeymap_o _SimpleKeymap_o;
typedef struct _WindowBorder_o _WindowBorder_o;
typedef struct _PopupList_o _PopupList_o;
#ifdef FEAT_POPUPLIST_BUFFERS
typedef struct _BufferItemProvider_o _BufferItemProvider_o;
#endif
#ifdef FEAT_POPUPLIST_MENUS
typedef struct _MenuItemProvider_o _MenuItemProvider_o;
#endif
typedef struct _QuickfixItemProvider_o _QuickfixItemProvider_o;
typedef struct _LineWriter_o _LineWriter_o;
typedef struct _Highlighter_o _Highlighter_o;
typedef struct _ShortcutHighlighter_o _ShortcutHighlighter_o;
typedef struct _TextMatchHighlighter_o _TextMatchHighlighter_o;
typedef struct _LineHighlightWriter_o _LineHighlightWriter_o;

/* ########## class Object ########## */
typedef struct _Object_c {
    /* methods from Object */
    void (*destroy)(void* _self);
} _Object_VT;

typedef struct _Object_o {
    _Object_VT* op;
} Object_T;


typedef void (*Destroy_Fn)(void* _self);

typedef int (*ItemComparator_Fn)(void* _self, void* a, void* b);

typedef int (*ItemMatcher_Fn)(void* _self, void* item);

/* ########## class ItemComparator(Object) ########## */
typedef struct _ItemComparator_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from ItemComparator */
    int (*compare)(void* _self, void* a, void* b);
} _ItemComparator_VT;

typedef struct _ItemComparator_o {
    _ItemComparator_VT* op;
    ItemComparator_Fn fn_compare;
    void* extra;
    int reverse;
} ItemComparator_T;


/* ########## class ItemMatcher(Object) ########## */
typedef struct _ItemMatcher_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from ItemMatcher */
    int (*match)(void* _self, void* item);
} _ItemMatcher_VT;

typedef struct _ItemMatcher_o {
    _ItemMatcher_VT* op;
    ItemMatcher_Fn fn_match;
    void* extra;
    int reverse;
} ItemMatcher_T;


/* ########## class ListHelper(Object) ########## */
typedef struct _ListHelper_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from ListHelper */
    void (*add_head)(void* _self, void* item);
    void (*add_tail)(void* _self, void* item);
    void* (*remove)(void* _self, void* item);
    void* (*remove_head)(void* _self);
    int (*_rem_del_all)(void* _self, struct _ItemMatcher_o* cond, int dodel);
    int (*remove_all)(void* _self, struct _ItemMatcher_o* cond);
    int (*delete_all)(void* _self, struct _ItemMatcher_o* cond);
} _ListHelper_VT;

typedef struct _ListHelper_o {
    _ListHelper_VT* op;
    void** first;
    void** last;
    Destroy_Fn fn_destroy;
    short offs_next;
} ListHelper_T;


/* ########## class DictIterator(Object) ########## */
typedef struct _DictIterator_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from DictIterator */
    dictitem_T* (*begin)(void* _self, dict_T* dict);
    dictitem_T* (*next)(void* _self);
} _DictIterator_VT;

typedef struct _DictIterator_o {
    _DictIterator_VT* op;
    dict_T* dict;
    hashitem_T* current;
    int todo;
} DictIterator_T;


/* ########## class SegmentedGrowArray(Object) ########## */
typedef struct _SegmentedGrowArray_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from SegmentedGrowArray */
    void (*clear)(void* _self);
    void (*clear_contents)(void* _self);
    void (*truncate)(void* _self);
    int (*grow)(void* _self, int count);
    void* (*get_new_item)(void* _self);
    void* (*get_item)(void* _self, int index);
    void (*sort)(void* _self, struct _ItemComparator_o* cmp);
    void (*_qsort)(void* _self, int low, int high, struct _ItemComparator_o* cmp);
} _SegmentedGrowArray_VT;

typedef struct _SegmentedGrowArray_o {
    _SegmentedGrowArray_VT* op;
    Destroy_Fn fn_destroy;
    int item_size;
    int len;
    int segment_len;
    int index_size;
    int index_len;
    void** index;
} SegmentedGrowArray_T;


/* ########## class SegmentedArrayIterator(Object) ########## */
typedef struct _SegmentedArrayIterator_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from SegmentedArrayIterator */
    void* (*begin)(void* _self, struct _SegmentedGrowArray_o* container);
    void* (*next)(void* _self);
} _SegmentedArrayIterator_VT;

typedef struct _SegmentedArrayIterator_o {
    _SegmentedArrayIterator_VT* op;
    struct _SegmentedGrowArray_o* array;
    int iitem;
} SegmentedArrayIterator_T;


typedef int (*MethodCallback_Fn)(void* _self, void* _data);

/* ########## struct NotificationCallback ########## */
typedef struct _NotificationCallback_o {
    struct _NotificationCallback_o* next;
    void* instance_self;
    MethodCallback_Fn callback;
} NotificationCallback_T;


/* ########## class NotificationList(Object) ########## */
typedef struct _NotificationList_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from NotificationList */
    void (*notify)(void* _self, void* _data);
    void (*add)(void* _self, void* instance, MethodCallback_Fn callback);
    void (*remove_obj)(void* _self, void* instance);
    void (*remove_cb)(void* _self, MethodCallback_Fn callback);
} _NotificationList_VT;

typedef struct _NotificationList_o {
    _NotificationList_VT* op;
    struct _NotificationCallback_o* observers;
    struct _ListHelper_o lst_observers;
} NotificationList_T;


/* ########## class CommandQueueItem(Object) ########## */
typedef struct _CommandQueueItem_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from CommandQueueItem */
} _CommandQueueItem_VT;

typedef struct _CommandQueueItem_o {
    _CommandQueueItem_VT* op;
    struct _CommandQueueItem_o* next;
    char_u* command;
} CommandQueueItem_T;


/* ########## class CommandQueue(Object) ########## */
typedef struct _CommandQueue_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from CommandQueue */
    void (*add)(void* _self, char_u* command);
    void (*pop)(void* _self);
    char_u* (*head)(void* _self);
} _CommandQueue_VT;

typedef struct _CommandQueue_o {
    _CommandQueue_VT* op;
    struct _CommandQueueItem_o* _cmd_list_first;
    struct _CommandQueueItem_o* _cmd_list_last;
    struct _ListHelper_o* _commands;
} CommandQueue_T;


/* ########## struct PopupItem ########## */
typedef struct _PopupItem_o {
    void* data;
    char_u* text;
    ushort flags;
    ushort filter_start;
    ushort filter_length;
    ushort filter_parent_score;
    ulong filter_score;
} PopupItem_T;


/* ########## class ItemProvider ########## */
typedef struct _ItemProvider_c {
    /* methods from ItemProvider */
    void (*destroy)(void* _self);
    void (*read_options)(void* _self, dict_T* options);
    void (*on_start)(void* _self);
    void (*clear_items)(void* _self);
    void (*sync_items)(void* _self);
    int (*get_item_count)(void* _self);
    PopupItem_T* (*get_item)(void* _self, int item);
    PopupItem_T* (*append_pchar_item)(void* _self, char_u* text, int shared);
    char_u* (*get_display_text)(void* _self, int item);
    char_u* (*get_filter_text)(void* _self, int item);
    char_u* (*get_path_text)(void* _self);
    char_u* (*get_title)(void* _self);
    void (*set_title)(void* _self, char_u* title);
    void (*set_marked)(void* _self, int item, int marked);
    uint (*has_flag)(void* _self, int item, uint flag);
    int (*select_item)(void* _self, int item);
    int (*select_parent)(void* _self);
    int (*find_shortcut)(void* _self, char_u* qchar_mb, int startidx, int* index, int* unique);
    int (*sort_items)(void* _self, struct _ItemComparator_o* cmp);
    void (*update_result)(void* _self, dict_T* status);
    char_u* (*handle_command)(void* _self, struct _PopupList_o* puls, char_u* command);
    int (*vim_cb_command)(void* _self, struct _PopupList_o* puls, char_u* command, typval_T* rettv);
    void (*_process_vim_cb_result)(void* _self, struct _PopupList_o* puls, dict_T* options);
    void (*default_keymap)(void* _self, struct _PopupList_o* puls);
} _ItemProvider_VT;

typedef struct _ItemProvider_o {
    _ItemProvider_VT* op;
    struct _SegmentedGrowArray_o* items;
    dict_T* commands;
    char_u* title;
    int has_title_items;
    int has_shortcuts;
    int out_of_sync;
    struct _NotificationList_o title_obsrvrs;
} ItemProvider_T;


/* ########## class VimlistItemProvider(ItemProvider) ########## */
typedef struct _VimlistItemProvider_c {
    /* methods from ItemProvider */
    void (*destroy)(void* _self);
    void (*read_options)(void* _self, dict_T* options);
    void (*on_start)(void* _self);
    void (*clear_items)(void* _self);
    void (*sync_items)(void* _self);
    int (*get_item_count)(void* _self);
    PopupItem_T* (*get_item)(void* _self, int item);
    PopupItem_T* (*append_pchar_item)(void* _self, char_u* text, int shared);
    char_u* (*get_display_text)(void* _self, int item);
    char_u* (*get_filter_text)(void* _self, int item);
    char_u* (*get_path_text)(void* _self);
    char_u* (*get_title)(void* _self);
    void (*set_title)(void* _self, char_u* title);
    void (*set_marked)(void* _self, int item, int marked);
    uint (*has_flag)(void* _self, int item, uint flag);
    int (*select_item)(void* _self, int item);
    int (*select_parent)(void* _self);
    int (*find_shortcut)(void* _self, char_u* qchar_mb, int startidx, int* index, int* unique);
    int (*sort_items)(void* _self, struct _ItemComparator_o* cmp);
    void (*update_result)(void* _self, dict_T* status);
    char_u* (*handle_command)(void* _self, struct _PopupList_o* puls, char_u* command);
    int (*vim_cb_command)(void* _self, struct _PopupList_o* puls, char_u* command, typval_T* rettv);
    void (*_process_vim_cb_result)(void* _self, struct _PopupList_o* puls, dict_T* options);
    void (*default_keymap)(void* _self, struct _PopupList_o* puls);
    /* methods from VimlistItemProvider */
    struct _PopupItem_o* (*_cache_list_item)(void* _self, listitem_T* item);
    void (*set_list)(void* _self, list_T* vimlist);
    void (*update_titles)(void* _self);
} _VimlistItemProvider_VT;

typedef struct _VimlistItemProvider_o {
    _VimlistItemProvider_VT* op;
    struct _SegmentedGrowArray_o* items;
    dict_T* commands;
    char_u* title;
    int has_title_items;
    int has_shortcuts;
    int out_of_sync;
    struct _NotificationList_o title_obsrvrs;
    list_T* vimlist;
    int _refcount;
    char _list_lock;
    int skip_leading;
    char_u* title_expr;
} VimlistItemProvider_T;


/* ########## struct Box ########## */
typedef struct _Box_o {
    int left;
    int top;
    int width;
    int height;
} Box_T;


/* ########## class LineEdit ########## */
typedef struct _LineEdit_c {
    /* methods from LineEdit */
    void (*destroy)(void* _self);
    void (*set_text)(void* _self, char_u* ptext);
    int (*add_text)(void* _self, char_u* ptext);
    int (*backspace)(void* _self);
} _LineEdit_VT;

typedef struct _LineEdit_o {
    _LineEdit_VT* op;
    char_u* text;
    int size;
    int len;
    int max_len;
    struct _Box_o position;
    struct _NotificationList_o change_obsrvrs;
} LineEdit_T;


/* ########## class TextMatcher(Object) ########## */
typedef struct _TextMatcher_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from TextMatcher */
    void (*set_search_str)(void* _self, char_u* needle);
    ulong (*match)(void* _self, char_u* haystack);
    void (*init_highlight)(void* _self, char_u* haystack);
    int (*get_match_at)(void* _self, char_u* haystack);
} _TextMatcher_VT;

typedef struct _TextMatcher_o {
    _TextMatcher_VT* op;
    char_u mode_char;
    char_u* _needle;
    int _need_strlen;
    ulong empty_score;
} TextMatcher_T;


/* ########## class TextMatcherRegexp(TextMatcher) ########## */
typedef struct _TextMatcherRegexp_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from TextMatcher */
    void (*set_search_str)(void* _self, char_u* needle);
    ulong (*match)(void* _self, char_u* haystack);
    void (*init_highlight)(void* _self, char_u* haystack);
    int (*get_match_at)(void* _self, char_u* haystack);
    /* methods from TextMatcherRegexp */
} _TextMatcherRegexp_VT;

typedef struct _TextMatcherRegexp_o {
    _TextMatcherRegexp_VT* op;
    char_u mode_char;
    char_u* _needle;
    int _need_strlen;
    ulong empty_score;
    regmatch_T _regmatch;
    int found;
} TextMatcherRegexp_T;


/* ########## struct TmWordMatchExpr ########## */
typedef struct _TmWordMatchExpr_o {
    struct _TmWordMatchExpr_o* next;
    char_u** not_words;
    int not_count;
    char_u** yes_words;
    int yes_count;
} TmWordMatchExpr_T;


/* ########## class TextMatcherWords(TextMatcher) ########## */
typedef struct _TextMatcherWords_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from TextMatcher */
    void (*set_search_str)(void* _self, char_u* needle);
    ulong (*match)(void* _self, char_u* haystack);
    void (*init_highlight)(void* _self, char_u* haystack);
    int (*get_match_at)(void* _self, char_u* haystack);
    /* methods from TextMatcherWords */
    void (*clear_words)(void* _self);
} _TextMatcherWords_VT;

typedef struct _TextMatcherWords_o {
    _TextMatcherWords_VT* op;
    char_u mode_char;
    char_u* _needle;
    int _need_strlen;
    ulong empty_score;
    char_u* _str_words;
    struct _TmWordMatchExpr_o* expressions;
    struct _ListHelper_o* lst_expr;
} TextMatcherWords_T;


/* ########## class TextMatcherCmdT(TextMatcher) ########## */
typedef struct _TextMatcherCmdT_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from TextMatcher */
    void (*set_search_str)(void* _self, char_u* needle);
    ulong (*match)(void* _self, char_u* haystack);
    void (*init_highlight)(void* _self, char_u* haystack);
    int (*get_match_at)(void* _self, char_u* haystack);
    /* methods from TextMatcherCmdT */
    void (*_clear)(void* _self);
    ulong (*_calc_pos_score)(void* _self, char_u* haystack, char_u** positions, int npos);
} _TextMatcherCmdT_VT;

typedef struct _TextMatcherCmdT_o {
    _TextMatcherCmdT_VT* op;
    char_u mode_char;
    char_u* _needle;
    int _need_strlen;
    ulong empty_score;
    int last_retry_offset;
    int _need_len;
    char_u** _need_chars;
    short* _need_char_lens;
    char_u** _hays_positions;
    char_u** _hays_best_positions;
} TextMatcherCmdT_T;


typedef void* (*NewObject_Fn)(void);

/* ########## class TextMatcherFactoryEntry(Object) ########## */
typedef struct _TextMatcherFactoryEntry_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from TextMatcherFactoryEntry */
    void (*set)(void* _self, char_u* name, NewObject_Fn fn_new);
} _TextMatcherFactoryEntry_VT;

typedef struct _TextMatcherFactoryEntry_o {
    _TextMatcherFactoryEntry_VT* op;
    struct _TextMatcherFactoryEntry_o* next;
    char_u* name;
    NewObject_Fn fn_new;
} TextMatcherFactoryEntry_T;


/* ########## class TextMatcherFactory ########## */
typedef struct _TextMatcherFactory_c {
    /* methods from TextMatcherFactory */
    void (*destroy)(void* _self);
    struct _TextMatcher_o* (*create_matcher)(void* _self, char_u* name);
    char_u* (*next_matcher)(void* _self, char_u* name);
} _TextMatcherFactory_VT;

typedef struct _TextMatcherFactory_o {
    _TextMatcherFactory_VT* op;
    struct _TextMatcherFactoryEntry_o* _entries;
    struct _ListHelper_o* _lst_entries;
} TextMatcherFactory_T;


/* ########## class ISearch ########## */
typedef struct _ISearch_c {
    /* methods from ISearch */
    void (*destroy)(void* _self);
    void (*set_matcher)(void* _self, struct _TextMatcher_o* pmatcher);
    void (*set_text)(void* _self, char_u* ptext);
    int (*match)(void* _self, char_u* ptext);
} _ISearch_VT;

typedef struct _ISearch_o {
    _ISearch_VT* op;
    char_u text[MAX_FILTER_SIZE + 1];
    int start;
    struct _TextMatcher_o* matcher;
} ISearch_T;


/* ########## class FltComparator_Score(ItemComparator) ########## */
typedef struct _FltComparator_Score_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from ItemComparator */
    int (*compare)(void* _self, void* a, void* b);
    /* methods from FltComparator_Score */
} _FltComparator_Score_VT;

typedef struct _FltComparator_Score_o {
    _FltComparator_Score_VT* op;
    ItemComparator_Fn fn_compare;
    void* extra;
    int reverse;
    struct _ItemProvider_o* model;
} FltComparator_Score_T;


/* ########## class FltComparator_TitleScore(ItemComparator) ########## */
typedef struct _FltComparator_TitleScore_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from ItemComparator */
    int (*compare)(void* _self, void* a, void* b);
    /* methods from FltComparator_TitleScore */
} _FltComparator_TitleScore_VT;

typedef struct _FltComparator_TitleScore_o {
    _FltComparator_TitleScore_VT* op;
    ItemComparator_Fn fn_compare;
    void* extra;
    int reverse;
    struct _ItemProvider_o* model;
} FltComparator_TitleScore_T;


/* ########## class ItemFilter ########## */
typedef struct _ItemFilter_c {
    /* methods from ItemFilter */
    void (*destroy)(void* _self);
    void (*set_matcher)(void* _self, struct _TextMatcher_o* pmatcher);
    void (*set_text)(void* _self, char_u* ptext);
    void (*filter_items)(void* _self);
    int (*get_item_count)(void* _self);
    int (*is_active)(void* _self);
    int (*get_model_index)(void* _self, int index);
    int (*get_index_of)(void* _self, int model_index);
} _ItemFilter_VT;

typedef struct _ItemFilter_o {
    _ItemFilter_VT* op;
    struct _ItemProvider_o* model;
    char_u text[MAX_FILTER_SIZE + 1];
    struct _TextMatcher_o* matcher;
    struct _SegmentedGrowArray_o* items;
    int keep_titles;
} ItemFilter_T;


/* ########## class BoxAligner(Object) ########## */
typedef struct _BoxAligner_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from BoxAligner */
    void (*set_limits)(void* _self, int left, int top, int right, int bottom);
    void (*parse_screen_pos)(void* _self, char_u* posdef);
    void (*set_align_params)(void* _self, dict_T* params);
    void (*align)(void* _self, struct _Box_o* box, struct _WindowBorder_o* border);
} _BoxAligner_VT;

typedef struct _BoxAligner_o {
    _BoxAligner_VT* op;
    struct _Box_o limits;
    char popup[2];
    char screen[2];
    int screen_x;
    int screen_y;
} BoxAligner_T;


/* ########## class SimpleKeymap(Object) ########## */
typedef struct _SimpleKeymap_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from SimpleKeymap */
    void (*set_name)(void* _self, char_u* name);
    char_u* (*encode_key)(void* _self, char_u* sequence);
    void (*set_vim_key)(void* _self, char_u* sequence, char_u* command);
    void (*set_key)(void* _self, char_u* sequence, char_u* command);
    char_u* (*get_command)(void* _self, char_u* sequence, int copy);
    int (*find_key)(void* _self, char_u* sequence);
    void (*clear_all_keys)(void* _self);
} _SimpleKeymap_VT;

typedef struct _SimpleKeymap_o {
    _SimpleKeymap_VT* op;
    char_u* name;
    dict_T* key2cmd;
    int has_insert;
    char_u mode_char;
} SimpleKeymap_T;


/* ########## class WindowBorder(Object) ########## */
typedef struct _WindowBorder_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from WindowBorder */
    void (*set_title)(void* _self, char_u* title);
    void (*set_mode_text)(void* _self, char_u* mode);
    void (*set_input_active)(void* _self, int active);
    void (*set_info)(void* _self, char_u* text);
    void (*prepare_scrollbar)(void* _self, int item_count);
    int (*get_scrollbar_kind)(void* _self, int line, int current);
    void (*draw_top)(void* _self);
    void (*draw_item_left)(void* _self, int line, int current);
    void (*draw_item_right)(void* _self, int line, int current);
    void (*draw_bottom)(void* _self);
} _WindowBorder_VT;

typedef struct _WindowBorder_o {
    _WindowBorder_VT* op;
    struct _Box_o* inner_box;
    int item_count;
    int active[4];
    int border_chars[8];
    int scrollbar_chars[2];
    int scrollbar_attr[2];
    int scrollbar_thumb;
    int scrollbar_pos;
    int border_attr;
    char_u* title;
    char_u* mode;
    char_u* info;
    int input_active;
    struct _LineEdit_o* line_edit;
} WindowBorder_T;


/* ########## class PopupList ########## */
typedef struct _PopupList_c {
    /* methods from PopupList */
    void (*destroy)(void* _self);
    void (*set_model)(void* _self, struct _ItemProvider_o* model);
    void (*read_options)(void* _self, dict_T* options);
    void (*default_keymap)(void* _self);
    void (*map_keys)(void* _self, char_u* kmap_name, dict_T* kmap);
    int (*calc_size)(void* _self, int limit_width, int limit_height);
    void (*reposition)(void* _self);
    void (*update_hl_chain)(void* _self);
    void (*redraw)(void* _self);
    int (*refilter)(void* _self, int track_item, int always_track);
    void (*move_cursor)(void* _self);
    int (*do_command)(void* _self, char_u* command);
    void (*switch_mode)(void* _self, char_u* modename);
    void (*prepare_result)(void* _self, dict_T* result);
    void (*save_state)(void* _self, dict_T* result);
    void (*set_title)(void* _self, char_u* title);
    void (*set_current)(void* _self, int index);
    int (*do_isearch)(void* _self, int dir);
    int (*on_filter_change)(void* _self, void* data);
    int (*on_isearch_change)(void* _self, void* data);
    int (*on_model_title_changed)(void* _self, void* data);
    int (*process_command)(void* _self, char_u* command);
} _PopupList_VT;

typedef struct _PopupList_o {
    _PopupList_VT* op;
    struct _ItemProvider_o* model;
    struct _ItemFilter_o* filter;
    struct _ISearch_o* isearch;
    char_u* filter_matcher_name;
    char_u* isearch_matcher_name;
    struct _TextMatcherFactory_o* filter_matcher_factory;
    struct _TextMatcherFactory_o* isearch_matcher_factory;
    struct _SimpleKeymap_o* km_normal;
    struct _SimpleKeymap_o* km_filter;
    struct _SimpleKeymap_o* km_search;
    struct _SimpleKeymap_o* km_shortcut;
    struct _SimpleKeymap_o* modemap;
    struct _CommandQueue_o* cmds_keyboard;
    struct _CommandQueue_o* cmds_macro;
    struct _BoxAligner_o* aligner;
    struct _WindowBorder_o* border;
    struct _LineEdit_o* line_edit;
    struct _ShortcutHighlighter_o* hl_menu;
    struct _TextMatcher_o* user_matcher;
    struct _TextMatchHighlighter_o* hl_user;
    struct _TextMatchHighlighter_o* hl_filter;
    struct _TextMatchHighlighter_o* hl_isearch;
    struct _Highlighter_o* hl_chain;
    struct _Box_o position;
    int current;
    int first;
    int leftcolumn;
    int column_split;
    int col0_width;
    int col1_width;
    int split_width;
    int need_redraw;
} PopupList_T;


/* ########## class BufferItemProvider(ItemProvider) ########## */
#ifdef FEAT_POPUPLIST_BUFFERS
typedef struct _BufferItemProvider_c {
    /* methods from ItemProvider */
    void (*destroy)(void* _self);
    void (*read_options)(void* _self, dict_T* options);
    void (*on_start)(void* _self);
    void (*clear_items)(void* _self);
    void (*sync_items)(void* _self);
    int (*get_item_count)(void* _self);
    PopupItem_T* (*get_item)(void* _self, int item);
    PopupItem_T* (*append_pchar_item)(void* _self, char_u* text, int shared);
    char_u* (*get_display_text)(void* _self, int item);
    char_u* (*get_filter_text)(void* _self, int item);
    char_u* (*get_path_text)(void* _self);
    char_u* (*get_title)(void* _self);
    void (*set_title)(void* _self, char_u* title);
    void (*set_marked)(void* _self, int item, int marked);
    uint (*has_flag)(void* _self, int item, uint flag);
    int (*select_item)(void* _self, int item);
    int (*select_parent)(void* _self);
    int (*find_shortcut)(void* _self, char_u* qchar_mb, int startidx, int* index, int* unique);
    int (*sort_items)(void* _self, struct _ItemComparator_o* cmp);
    void (*update_result)(void* _self, dict_T* status);
    char_u* (*handle_command)(void* _self, struct _PopupList_o* puls, char_u* command);
    int (*vim_cb_command)(void* _self, struct _PopupList_o* puls, char_u* command, typval_T* rettv);
    void (*_process_vim_cb_result)(void* _self, struct _PopupList_o* puls, dict_T* options);
    void (*default_keymap)(void* _self, struct _PopupList_o* puls);
    /* methods from BufferItemProvider */
    void (*list_buffers)(void* _self);
    int (*sort_buffers)(void* _self);
    int (*_index_to_bufnr)(void* _self, int index);
} _BufferItemProvider_VT;

typedef struct _BufferItemProvider_o {
    _BufferItemProvider_VT* op;
    struct _SegmentedGrowArray_o* items;
    dict_T* commands;
    char_u* title;
    int has_title_items;
    int has_shortcuts;
    int out_of_sync;
    struct _NotificationList_o title_obsrvrs;
    char sorted_by;
    int show_unlisted;
    list_T* mru_list;
} BufferItemProvider_T;

#endif /* FEAT_POPUPLIST_BUFFERS */

/* ########## class MenuItemProvider(ItemProvider) ########## */
#ifdef FEAT_POPUPLIST_MENUS
typedef struct _MenuItemProvider_c {
    /* methods from ItemProvider */
    void (*destroy)(void* _self);
    void (*read_options)(void* _self, dict_T* options);
    void (*on_start)(void* _self);
    void (*clear_items)(void* _self);
    void (*sync_items)(void* _self);
    int (*get_item_count)(void* _self);
    PopupItem_T* (*get_item)(void* _self, int item);
    PopupItem_T* (*append_pchar_item)(void* _self, char_u* text, int shared);
    char_u* (*get_display_text)(void* _self, int item);
    char_u* (*get_filter_text)(void* _self, int item);
    char_u* (*get_path_text)(void* _self);
    char_u* (*get_title)(void* _self);
    void (*set_title)(void* _self, char_u* title);
    void (*set_marked)(void* _self, int item, int marked);
    uint (*has_flag)(void* _self, int item, uint flag);
    int (*select_item)(void* _self, int item);
    int (*select_parent)(void* _self);
    int (*find_shortcut)(void* _self, char_u* qchar_mb, int startidx, int* index, int* unique);
    int (*sort_items)(void* _self, struct _ItemComparator_o* cmp);
    void (*update_result)(void* _self, dict_T* status);
    char_u* (*handle_command)(void* _self, struct _PopupList_o* puls, char_u* command);
    int (*vim_cb_command)(void* _self, struct _PopupList_o* puls, char_u* command, typval_T* rettv);
    void (*_process_vim_cb_result)(void* _self, struct _PopupList_o* puls, dict_T* options);
    void (*default_keymap)(void* _self, struct _PopupList_o* puls);
    /* methods from MenuItemProvider */
    void (*find_menu)(void* _self, char_u* menu_path);
    void (*update_title)(void* _self);
    int (*parse_mode)(void* _self, char_u* command);
    int (*list_items)(void* _self, void* selected);
    int (*_list_items_r)(void* _self, vimmenu_T* menu, void* selected, int* count, int level);
} _MenuItemProvider_VT;

typedef struct _MenuItemProvider_o {
    _MenuItemProvider_VT* op;
    struct _SegmentedGrowArray_o* items;
    dict_T* commands;
    char_u* title;
    int has_title_items;
    int has_shortcuts;
    int out_of_sync;
    struct _NotificationList_o title_obsrvrs;
    vimmenu_T* top_menu;
    vimmenu_T* cur_menu;
    int mode;
    int flat_view;
} MenuItemProvider_T;

#endif /* FEAT_POPUPLIST_MENUS */

/* ########## class QuickfixItemProvider(ItemProvider) ########## */
typedef struct _QuickfixItemProvider_c {
    /* methods from ItemProvider */
    void (*destroy)(void* _self);
    void (*read_options)(void* _self, dict_T* options);
    void (*on_start)(void* _self);
    void (*clear_items)(void* _self);
    void (*sync_items)(void* _self);
    int (*get_item_count)(void* _self);
    PopupItem_T* (*get_item)(void* _self, int item);
    PopupItem_T* (*append_pchar_item)(void* _self, char_u* text, int shared);
    char_u* (*get_display_text)(void* _self, int item);
    char_u* (*get_filter_text)(void* _self, int item);
    char_u* (*get_path_text)(void* _self);
    char_u* (*get_title)(void* _self);
    void (*set_title)(void* _self, char_u* title);
    void (*set_marked)(void* _self, int item, int marked);
    uint (*has_flag)(void* _self, int item, uint flag);
    int (*select_item)(void* _self, int item);
    int (*select_parent)(void* _self);
    int (*find_shortcut)(void* _self, char_u* qchar_mb, int startidx, int* index, int* unique);
    int (*sort_items)(void* _self, struct _ItemComparator_o* cmp);
    void (*update_result)(void* _self, dict_T* status);
    char_u* (*handle_command)(void* _self, struct _PopupList_o* puls, char_u* command);
    int (*vim_cb_command)(void* _self, struct _PopupList_o* puls, char_u* command, typval_T* rettv);
    void (*_process_vim_cb_result)(void* _self, struct _PopupList_o* puls, dict_T* options);
    void (*default_keymap)(void* _self, struct _PopupList_o* puls);
    /* methods from QuickfixItemProvider */
    int (*list_items)(void* _self);
    int (*_prepare_dispbuf)(void* _self, int len);
    void (*_update_title)(void* _self);
    int (*_display_item)(void* _self, struct _PopupItem_o* pitem);
} _QuickfixItemProvider_VT;

typedef struct _QuickfixItemProvider_o {
    _QuickfixItemProvider_VT* op;
    struct _SegmentedGrowArray_o* items;
    dict_T* commands;
    char_u* title;
    int has_title_items;
    int has_shortcuts;
    int out_of_sync;
    struct _NotificationList_o title_obsrvrs;
    qf_info_T* qfinfo;
    char_u* _dispbuf;
    int _dispbuf_len;
} QuickfixItemProvider_T;


/* ########## class LineWriter(Object) ########## */
typedef struct _LineWriter_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from LineWriter */
    void (*add_fixed_tab)(void* _self, int col);
    int (*get_tab_size_at)(void* _self, int col);
    void (*set_limits)(void* _self, int min_col, int max_col);
    void (*write_line)(void* _self, char_u* text, int row, int attr, int fillChar);
} _LineWriter_VT;

typedef struct _LineWriter_o {
    _LineWriter_VT* op;
    int fixed_tabs[PLWR_MAX_FIXED_TABS+1];
    int tab_size;
    int offset;
    int min_col;
    int max_col;
} LineWriter_T;


/* ########## class Highlighter(Object) ########## */
typedef struct _Highlighter_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from Highlighter */
    void (*bol_init)(void* _self, char_u* text, void* extra_data);
    int (*calc_attr)(void* _self, char_u* next_char);
} _Highlighter_VT;

typedef struct _Highlighter_o {
    _Highlighter_VT* op;
    struct _Highlighter_o* next;
    int active;
    int state;
    int default_attr;
    int text_attr;
    int text_width;
    char_u* match_end;
} Highlighter_T;


/* ########## class ShortcutHighlighter(Highlighter) ########## */
typedef struct _ShortcutHighlighter_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from Highlighter */
    void (*bol_init)(void* _self, char_u* text, void* extra_data);
    int (*calc_attr)(void* _self, char_u* next_char);
    /* methods from ShortcutHighlighter */
} _ShortcutHighlighter_VT;

typedef struct _ShortcutHighlighter_o {
    _ShortcutHighlighter_VT* op;
    struct _Highlighter_o* next;
    int active;
    int state;
    int default_attr;
    int text_attr;
    int text_width;
    char_u* match_end;
    int shortcut_attr;
} ShortcutHighlighter_T;


/* ########## class TextMatchHighlighter(Highlighter) ########## */
typedef struct _TextMatchHighlighter_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from Highlighter */
    void (*bol_init)(void* _self, char_u* text, void* extra_data);
    int (*calc_attr)(void* _self, char_u* next_char);
    /* methods from TextMatchHighlighter */
    void (*set_matcher)(void* _self, struct _TextMatcher_o* matcher);
} _TextMatchHighlighter_VT;

typedef struct _TextMatchHighlighter_o {
    _TextMatchHighlighter_VT* op;
    struct _Highlighter_o* next;
    int active;
    int state;
    int default_attr;
    int text_attr;
    int text_width;
    char_u* match_end;
    struct _TextMatcher_o* matcher;
    int match_attr;
    char_u* match_start;
} TextMatchHighlighter_T;


/* ########## class LineHighlightWriter(LineWriter) ########## */
typedef struct _LineHighlightWriter_c {
    /* methods from Object */
    void (*destroy)(void* _self);
    /* methods from LineWriter */
    void (*add_fixed_tab)(void* _self, int col);
    int (*get_tab_size_at)(void* _self, int col);
    void (*set_limits)(void* _self, int min_col, int max_col);
    void (*write_line)(void* _self, char_u* text, int row, int attr, int fillChar);
    /* methods from LineHighlightWriter */
    int (*_flush)(void* _self, char_u* text_end, int row, int col, int attr);
} _LineHighlightWriter_VT;

typedef struct _LineHighlightWriter_o {
    _LineHighlightWriter_VT* op;
    int fixed_tabs[PLWR_MAX_FIXED_TABS+1];
    int tab_size;
    int offset;
    int min_col;
    int max_col;
    char_u* _tmpbuf;
    char_u* _tmplimit;
    struct _Highlighter_o* highlighters;
} LineHighlightWriter_T;


