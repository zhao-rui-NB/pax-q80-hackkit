/* Small adapter to the device-built libchewing 0.5.1. */
#include <dlfcn.h>
#include "ime.h"

typedef struct ChewingContext ChewingContext;
static void *library;
static ChewingContext *context;
static int candidates_visible;
static ChewingContext *(*create)(const char *, const char *,
                                 void (*)(void *, int, const char *, ...), void *);
static void (*destroy)(ChewingContext *);
static int (*type_key)(ChewingContext *, int);
static int (*space)(ChewingContext *);
static int (*backspace)(ChewingContext *);
static int (*enter)(ChewingContext *);
static int (*candidate_count)(const ChewingContext *);
static const char *(*candidate)(ChewingContext *, int);
static int (*choose)(ChewingContext *, int);
static int (*open_candidates)(ChewingContext *);
static int (*close_candidates)(ChewingContext *);
static const char *(*buffer)(const ChewingContext *);
static const char *(*bopomofo)(const ChewingContext *);
static const char *(*commit)(const ChewingContext *);
#define LOAD(target, name) do { \
    *(void **)(&target) = dlsym(library, name); \
    if (!target) goto fail; \
} while (0)

static void show_candidates(void)
{
    /* Chewing only populates its candidate list after an explicit open. */
    if (context && !bopomofo(context)[0] && buffer(context)[0] &&
        open_candidates(context) == 0)
        candidates_visible = candidate_count(context) > 0;
}

int ime_open(void)
{
    if (context) return 0;
    candidates_visible = 0;
    library = dlopen("/data/app/label_maker/libchewing.so", RTLD_NOW);
    if (!library) return -1;
    LOAD(create, "chewing_new2");
    LOAD(destroy, "chewing_delete");
    LOAD(type_key, "chewing_handle_Default");
    LOAD(space, "chewing_handle_Space");
    LOAD(backspace, "chewing_handle_Backspace");
    LOAD(enter, "chewing_handle_Enter");
    LOAD(candidate_count, "chewing_cand_TotalChoice");
    LOAD(candidate, "chewing_cand_string_by_index_static");
    LOAD(choose, "chewing_cand_choose_by_index");
    LOAD(open_candidates, "chewing_cand_open");
    LOAD(close_candidates, "chewing_cand_close");
    LOAD(buffer, "chewing_buffer_String_static");
    LOAD(bopomofo, "chewing_bopomofo_String_static");
    LOAD(commit, "chewing_commit_String_static");
    context = create("/data/app/label_maker/data",
                     "/data/app/label_maker/userphrase.dat", 0, 0);
    if (context) return 0;
fail:
    ime_close();
    return -1;
}
void ime_close(void)
{
    if (context) destroy(context);
    context = 0;
    candidates_visible = 0;
    if (library) dlclose(library);
    library = 0;
}
int ime_key(int key)
{
    int result;
    if (!context) return -1;
    if (candidates_visible) { close_candidates(context); candidates_visible = 0; }
    result = type_key(context, key);
    show_candidates();
    return result;
}
int ime_space(void)
{
    int result;
    if (!context) return -1;
    if (candidates_visible) return ime_choose(0);
    result = space(context);
    show_candidates();
    return result;
}
int ime_backspace(void)
{
    int result;
    if (!context) return -1;
    if (candidates_visible) { close_candidates(context); candidates_visible = 0; }
    result = backspace(context);
    show_candidates();
    return result;
}
int ime_enter(void)
{
    if (!context) return -1;
    if (candidates_visible) { close_candidates(context); candidates_visible = 0; }
    return enter(context);
}
int ime_choose(int index)
{
    int result;
    if (!context || !candidates_visible || index < 0 || index >= candidate_count(context)) return -1;
    result = choose(context, index);
    close_candidates(context);
    candidates_visible = 0;
    return result;
}
int ime_candidate_count(void) { return context && candidates_visible ? candidate_count(context) : 0; }
const char *ime_candidate(int index)
{
    const char *text = context && candidates_visible ? candidate(context, index) : 0;
    return text ? text : "";
}
const char *ime_buffer(void)
{
    const char *text = context ? buffer(context) : 0;
    return text ? text : "";
}
const char *ime_bopomofo(void)
{
    const char *text = context ? bopomofo(context) : 0;
    return text ? text : "";
}
const char *ime_commit(void)
{
    const char *text = context ? commit(context) : 0;
    return text ? text : "";
}
