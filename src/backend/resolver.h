#ifndef KILITE_RESOLVER_H
#define KILITE_RESOLVER_H

void open_std_libs(void);
void close_std_libs(void);
void *import_resolver(const char *name);

#endif /* KILITE_RESOLVER_H */
