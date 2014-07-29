 /* file minunit_example.c */

#include <stdio.h>
#include "minunit.h"
#include "hash.c"

int assertions_run = 0;
int tests_run = 0;

static char * test_entry() {
  struct st_entry entry1;
  struct st_entry entry2;

  entry1.key = 1;
  object x1 = 11;
  entry1.valuep = &x1;
  entry1.nextp = &entry2;
  entry1.backp = &entry2;
  entry1.forep = NULL;

  entry2.key = 2;
  object x2 = 22;
  entry2.valuep = &x1;
  entry2.nextp = &entry1;
  entry2.backp = NULL;
  entry2.forep = &entry1;

  return 0;
}

static char * test_add_delete() {
  struct st_table *tablep = new_tablep();

  object x = 2;
  set(tablep, 1, &x);

  object y = 4;
  set(tablep, 2, &y);

  object *result1 = get(tablep, 1);
  object *result2 = get(tablep, 2);

  mu_assert("get(tablep, 1) didn't work.", *result1 == 2);
  mu_assert("get(tablep, 2) didn't work.", *result2 == 4);

  delete(tablep, 1);
  object *result3 = get(tablep, 1);

  mu_assert("delete(tablep, 1) didn't work.",
            result3 == NULL);

  delete(tablep, 2);
  object *result4 = get(tablep, 2);

  mu_assert("delete(tablep, 2) didn't work.",
            result4 == NULL);

  free_table(tablep);

  return 0;
}

static char * test_expand_table_1() {
  struct st_table *tablep = new_tablep();
  size_t num_bins1 = tablep->num_bins;

  object x = 2;
  set(tablep, 10, &x);

  object *result1 = get(tablep, 10);
  mu_assert("get(tablep, 10) didn't work.", *result1 == 2);

  expand_table(tablep);

  mu_assert("expand_table(tablep) did not change num_bins.",
            tablep->num_bins != num_bins1);
  mu_assert("get(tablep, 10) didn't work.", *result1 == 2);

  free_table(tablep);

  return 0;
}

static char * test_expand_table_2() {
  struct st_table *tablep = new_tablep();
  size_t num_bins1 = tablep->num_bins;

  object i;

  object *xs = malloc((num_bins1+1) * sizeof(object));

  for (i = 0; i <= num_bins1; i++) {
    xs[i] = 2*i;
    set(tablep, i, &xs[i]);
  }

  mu_assert("num_bins not correct",
            tablep->num_bins > num_bins1);

  return 0;
}

static char * all_tests() {
  mu_run_test(test_entry);
  mu_run_test(test_add_delete);
  mu_run_test(test_expand_table_1);
  mu_run_test(test_expand_table_2);

  return 0;
}

int main(int argc, char **argv) {
  char *result = all_tests();
  if (result != 0) {
    printf("%s\n", result);
  }
  else {
    printf("ALL TESTS PASSED\n");
  }
  printf("Assertions run: %d\n", assertions_run);
  printf("Tests run: %d\n", tests_run);

  return result != 0;
}