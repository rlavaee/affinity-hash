 /* file minunit_example.c */

#include <stdio.h>
#include "minunit.h"
#include "hash.c"

int tests_run = 0;

int foo = 7;
int bar = 4;

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

static char * test_table() {
  struct st_table *tablep = new_tablep();
  object x = 4;
  set(tablep, 3, &x);
  mu_assert("Get didn't work.", *get(tablep, 3) == 4);


  return 0;
}

static char * test_foo() {
  mu_assert("error, foo != 7", foo == 7);
  return 0;
}

static char * test_bar() {
  mu_assert("error, bar != 5", bar == 5);
  return 0;
}

static char * all_tests() {
  mu_run_test(test_entry);
  mu_run_test(test_table);
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
  printf("Tests run: %d\n", tests_run);

  return result != 0;
}