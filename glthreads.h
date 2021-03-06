#ifndef __GLTHREADS__
#define __GLTHREADS__

#include <stddef.h>
typedef struct gl_thread_ {

  struct gl_thread_ *left;
  struct gl_thread_ *right;
}gl_thread_t;

void glthread_add_next(gl_thread_t *list, gl_thread_t *glnode);
void glthread_node_init(gl_thread_t *glthread);
void remove_glthread(gl_thread_t *glnode);

#define BASE(start) ((start)->right)

#define IS_GLTHREAD_LIST_EMPTY(glthreadptr) \
   ((glthreadptr)->left == 0 && (glthreadptr)->right == 0)

#define ITERATE_GLTHREAD_BEGIN(start, curr) \
{ 					    \
  gl_thread_t* temp = NULL;		    \
  curr = BASE(start);			    \
  for(;curr != NULL; curr = temp) {         \
    temp = (curr)->right;			    \

#define ITERATE_GLTHREAD_END(start, curr)   \
	}}

#define offsetof(struct_name, field_name) 		\
  (char*)&(((struct_name*)NULL)->field_name)

#define GLTHREAD_TO_STRUCT(fn_name, structure_name, field_name)                        \
    static inline structure_name * fn_name(gl_thread_t *glthreadptr){                   \
        return (structure_name *)((char *)(glthreadptr) - (char *)&(((structure_name *)0)->field_name)); \
    }

#endif /*__GL_THREADS__*/

