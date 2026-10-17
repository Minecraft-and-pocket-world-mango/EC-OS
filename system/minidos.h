#ifndef EC_MINIDOS_H
#define EC_MINIDOS_H

void minidos_open(void);
int  minidos_is_open(void);
void minidos_key(int key);
void minidos_exec(const char *cmd);

#endif
