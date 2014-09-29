 /* file minunit_example.c */

#include <stdio.h>
#include "minunit.h"
#include "hash.c"

int assertions_run = 0;
int tests_run = 0;

static char * test_new_table() {
  struct st_table *my_tablep = new_table();
  mu_assert("Message", 1 /*boolean*/);
  free_table(my_tablep);
  return 0;
}

static char * all_tests() {
  mu_run_test(test_new_table);
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
