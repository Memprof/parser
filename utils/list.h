#ifndef SYMB_LIST_H
#define SYMB_LIST_H 1

// Do magic! Creates a unique name using the line number
#define LINE_NAME( prefix ) JOIN( prefix, __LINE__ )
#define JOIN( symbol1, symbol2 ) _DO_JOIN( symbol1, symbol2 )
#define _DO_JOIN( symbol1, symbol2 ) symbol1##symbol2

struct list {
   void *data;
   struct list* next;
};


#define list_add(p, ddata)                 \
   ({                                     \
   struct list *l = malloc(sizeof(*l));   \
   l->data = (void*)ddata;                \
   l->next = p;                           \
   l;                                     \
   })

#define list_next(l)                       \
      ({                                   \
        struct list* LINE_NAME(___ll) = l; \
        l = l->next;                       \
        free(LINE_NAME(___ll));            \
        l;                                 \
      })

#define list_foreach(l, n) \
      struct list* LINE_NAME(___ll) = l; \
      for(;LINE_NAME(___ll) && (n = LINE_NAME(___ll)->data); LINE_NAME(___ll) = LINE_NAME(___ll)->next)




#endif

