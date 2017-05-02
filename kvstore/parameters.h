#ifndef PARAMETERS_H
#define PARAMETERS_H

/* Maximum in-memory + disk capacity */
long long static NODE_CAPACITY = 1024 * 1024 * 1024; 

/* Maximum in-memory capacity
   Assume that storage space for a single user cannot exceed this max capacity. */
long long static MEMORY_CAPACITY = 500 * 1024 * 1024;

long long static CHUNK_SIZE = 64 * 1024 * 1024;

int static CP_INTERVAL = 20;

#endif