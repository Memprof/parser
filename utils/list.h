/*
Copyright (C) 2013  
Baptiste Lepers <baptiste.lepers@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2, as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
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

