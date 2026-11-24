#ifndef LABEL_IME_H
#define LABEL_IME_H
int ime_open(void);
void ime_close(void);
int ime_key(int key);
int ime_space(void);
int ime_backspace(void);
int ime_enter(void);
int ime_choose(int index);
int ime_candidate_count(void);
const char *ime_candidate(int index);
const char *ime_buffer(void);
const char *ime_bopomofo(void);
const char *ime_commit(void);
#endif
