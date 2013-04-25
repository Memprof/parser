#ifndef BUILTIN_OBJ
#define BUILTIN_OBJ

void obj_init();
void obj_set(int uid);
void obj_parse(struct s* s);
void obj_show();
void obj_modifier(int m);

#endif
