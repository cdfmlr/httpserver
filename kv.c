#include "kv.h"

// 尝试让 GitHub Copilot 生成这个破模块还搞出好多 bug 来。
// 今天有点累了，我也就浅改一下了，能跑就行，
// 这些代码着实像某种哺乳动物消化的副产物，，
// 反正这无序单链表，现实中肯定是废物。。
// 有时间再重写吧。

// kv operations: private

kv *kv_new(char *key, char *value, char mm) {
    kv *new = malloc(sizeof(kv));
    new->key = key;
    new->value = value;
    new->next = NULL;
    return new;
}

inline kv *kv_new_cp(char *key, char *value) {
    char mm = 3;

    char *k = malloc(strlen(key) * sizeof(char) + 1);
    strcpy(k, key);
    char *v = malloc(strlen(value) * sizeof(char) + 1);
    strcpy(v, value);

    kv *new = kv_new(k, v, mm);
    //    printf("kv_new_cp: [<%s>: <%s>]\n", k, v);

    set_key_allocated(new);

    return new;
}

void kv_free(kv *kv) {
    if (kv == NULL) return;

    if (is_key_allocated(kv)) {
        free(kv->key);
    }
    if (is_value_allocated(kv)) {
        free(kv->value);
    }

    kv->next = NULL;

    free(kv);
}

void kv_free_all(kv *kv) {
    struct kv *next;
    while (kv) {
        next = kv->next;
        kv_free(kv);
        kv = next;
    }
}

kv *kv_get(kv *kv, char *key) {
    while (kv) {
        if (strcmp(kv->key, key) == 0) {
            return kv;
        }
        kv = kv->next;
    }
    return NULL;
}

kv *kv_set(kv *kv, char *key, char *value) {
    struct kv *found = kv_get(kv, key);
    if (found) {
        // ERROR // free(found->value);  // see comments in kv_free
        if (is_value_allocated(found)) {
            free(found->value);
        }
        found->value = value;
    } else {
        struct kv *new = kv_new_cp(key, value);
        new->next = kv;
        kv = new;
    }
    return kv;
}

// return the new head (given kv is the old head)
kv *kv_unset(kv *kv, char *key) {
    // NULL
    if (kv == NULL) return NULL;

    // head is the one
    if (strcmp(kv->key, key) == 0) {
        struct kv *next = kv->next;
        kv_free(kv);
        return next;
    }

    // find the one and remove it, return head
    struct kv *head = kv;

    struct kv *prev = kv;
    struct kv *curr = kv->next;
    while (curr) {
        if (strcmp(curr->key, key) == 0) {
            prev->next = curr->next;
            kv_free(curr);
            return head;
        }
        prev = curr;
        curr = curr->next;
    }

    return NULL;
}

// kvs operations: public

kvs *kvs_new() {
    kvs *new = malloc(sizeof(kvs));
    new->head = NULL;
    return new;
}

void kvs_free(kvs *kvs) {
    if (kvs == NULL) return;

    kv_free_all(kvs->head);
    kvs->head = NULL;
    // printf("::");kvs_print(kvs);fflush(stdout);
    free(kvs);
}

char *kvs_get(kvs *kvs, char *key) {
    struct kv *found = kv_get(kvs->head, key);
    if (found) {
        return found->value;
    } else {
        return NULL;
    }
}

void kvs_set(kvs *kvs, char *key, char *value) {
    kvs->head = kv_set(kvs->head, key, value);
}

void kvs_unset(kvs *kvs, char *key) {
    kvs->head = kv_unset(kvs->head, key);
}

void foreach(kvs *kvs, void (*f)(char *key, char *value)) {
    struct kv *kv = kvs->head;
    while (kv) {
        f(kv->key, kv->value);
        kv = kv->next;
    }
}

void _print_kv(char *key, char *value) {
    printf("[%s: %s] ", key, value);
}

void kvs_print(kvs *kvs) {
    foreach(kvs, _print_kv);
}
