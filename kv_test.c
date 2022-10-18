#include "kv.h"
#include <stdio.h>

int is_one_of_123(kv *);

int kvs_test();

int main() {
    // it's illegal to free a stack value
    // char *key = "key";
    // free(key);
    // return 0;

    kvs_test();
    return 0;
}

int
kvs_test() {
    // set and get one key
    printf("set & get key1: value1\n");

    kvs *kvs = kvs_new();
    kvs_set(kvs, "key1", "value1");
    char *v = kvs_get(kvs, "key1");

    kvs_print(kvs);
    if (strcmp(v, "value1") != 0) {
        printf("kvs_test: kvs_get failed: got %s, expected %s\n", v, "value1");
        return -1;
    }

    // set other keys
    printf("\nset & get key2: value2, key3: value3\n");

    kvs_set(kvs, "key2", "value2");
    kvs_set(kvs, "key3", "value3");

    kvs_print(kvs);

    kv *kv = kvs->head;
    for (int i = 0; i < 3; i++) {
        if (is_one_of_123(kv) < 0) {
            return -1;
        }
        kv = kv->next;
    }

    // got key2
    printf("\nget key2\n");

    char *v2 = kvs_get(kvs, "key2");
    if (strcmp(v2, "value2") != 0) {
        printf("kvs_test: kvs_get failed: got %s, expected %s\n", v2, "value2");
        return -1;
    } else {
        printf("got: <%s>", v2);
    }

    // unset a key
    printf("\nunset key2\n");

    kvs_unset(kvs, "key2");
    kvs_print(kvs);

    kv = kvs->head;
    for (int i = 0; i < 2; i++) {
        if (kv->key[3] == '2') {
            printf("kvs_test: kvs_unset failed: got %s, expected %s\n", kv->key, "key1 or key3");
            return -1;
        }
        kv = kv->next;
    }
    if (kv != NULL) {
        printf("kvs_test: kvs_unset failed: len > 2\n");
        return -1;
    }

    // reset a key
    printf("\nreset key2\n");

    kvs_set(kvs, "key1", "updated_value1");

    kvs_print(kvs);

    v = kvs_get(kvs, "key1");
    if (strcmp(v, "updated_value1") != 0) {
        printf("kvs_test: kvs_set failed: got %s, expected %s\n", v, "updated_value1");
        return -1;
    }

    // free
    printf("\nfree\n");

    kvs_free(kvs);

    // unusable now: Segmentation fault: 11
    // kvs_print(kvs);
    // printf("%p: %p", kvs, kvs->head);

    // ✅ pointer being freed was not allocated
    // free(kvs);
    // printf("%p: %p", kvs, kvs->head);

    return 0;
}

int
is_one_of_123(kv *kv) {
    if (strcmp(kv->key, "key1") == 0) {
        if (strcmp(kv->value, "value1") != 0) {
            printf("kvs_test: kvs_get failed: got %s, expected %s\n",
                   kv->value,
                   "value1");
            return -1;
        }
    } else if (strcmp(kv->key, "key2") == 0) {
        if (strcmp(kv->value, "value2") != 0) {
            printf("kvs_test: kvs_get failed: got %s, expected %s\n",
                   kv->value,
                   "value2");
            return -1;
        }
    } else if (strcmp(kv->key, "key3") == 0) {
        if (strcmp(kv->value, "value3") != 0) {
            printf("kvs_test: kvs_get failed: got %s, expected %s\n",
                   kv->value,
                   "value3");
            return -1;
        }
    } else {
        printf("kvs_test: kvs_get failed: got %s, expected %s\n",
               kv->key,
               "key1, key2, or key3");
        return -1;
    }
    return 0;
}
