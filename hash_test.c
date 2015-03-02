 /* file minunit_example.c */

#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#include "minunit.h"
#include "hamt.c"

int assertions_run = 0;
int tests_run = 0;

static char *test_new_table() {
    ah_table *my_tablep = init_table();
    mu_assert("Allocated table", my_tablep != NULL);

    return 0;
}

static char *test_ins_find() {
    srand(time(NULL));

    ah_table *table = init_table();
    static const long size = 200000;
    long val[size] = {0}, key;    
    
    for(key = 0; key < size; key++) val[key] = rand(); 

    for(key = 0; key < size; key++)
        insert(table, key, &val[key]);

    for(key = 0; key < size; key++)
        mu_assert("Value matched expected", *search(table,key) == val[key]);

    return 0;
}

// Ensures all accesses to map can be properly reached(when tracing is on).
static char *test_retrieve_map() {
    if(!trace_start) return 0;

    ah_access *current = trace_start->next;

    while((current = current->next))
        mu_assert("Internal node search works", 
            retrieve_map_for_hash_val(current->table, current->hash, current->offset) != NULL);

    return 0;
}

static char *all_tests() {
    mu_run_test(test_new_table);
    mu_run_test(test_ins_find);
    mu_run_test(test_retrieve_map);

    return 0;
}

int main(int argc, char **argv) {
    char *result = all_tests();
    
    if (result != 0) {
        printf("Failed: %s\n", result);
    } else {
        printf("ALL TESTS PASSED\n");
    }
    
    printf("Assertions run: %d\n", assertions_run);
    printf("Tests run: %d\n", tests_run);
    
    return result != 0;
}
