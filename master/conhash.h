#ifndef CONHASH_H
#define CONHASH_H

#define A 54059 /* a prime */
#define B 76963 /* another prime */
#define MAX_KEY 86969 /* yet another prime, also the max key */
#define FIRSTH 37 /* also prime */

unsigned hash_str(const char* s);

#endif
