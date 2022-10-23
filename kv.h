#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define is_key_allocated(kv) ((kv)->mm & 1)
#define is_value_allocated(kv) ((kv)->mm & 2)

#define set_key_allocated(kv) {(kv)->mm |= 1;}
#define set_value_allocated(kv) {(kv)->mm |= 2;}

/*
 * key-value store
 * DO NOT USE THIS DIRECTLY, USE kvs INSTEAD.
 */
typedef struct kv {
    char *key;
    char *value;

    // memory management flags: 最低位: key_allocated, 次低位: value_allocated
    char mm;

    struct kv *next;
} kv;

// mm: memory management flags
kv *kv_new(char *key, char *value, char mm);

// kv_new_cp do kv_new and always copy objects.
// 推荐用这个，始终复制内存，交给 kv 自动管理。
kv *kv_new_cp(char *key, char *value);

void kv_free(kv *kv);

void kv_free_all(kv *kv);

kv *kv_get(kv *kv, char *key);

kv *kv_set(kv *kv, char *key, char *value);

kv *kv_unset(kv *kv, char *key);

/* 
 * kvs is a list of kv 
 */
typedef struct kvs {
    struct kv *head;
} kvs;

kvs *kvs_new();

void kvs_free(kvs *kvs);

char *kvs_get(kvs *kvs, char *key);

void kvs_set(kvs *kvs, char *key, char *value);

void kvs_unset(kvs *kvs, char *key);

void kvs_print(kvs *kvs);