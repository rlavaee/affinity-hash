/* Code complements of http://www.jera.com/techinfo/jtns/jtn002.html#License */

#define mu_assert(message, test) do {                           \
								   assertions_run++;            \
                                   if (!(test)) return message; \
                                 } while (0)
#define mu_run_test(test) do {                           \
                            char *message = test();      \
                            tests_run++;                 \
                            if (message) return message; \
                          } while (0)
extern int tests_run;
