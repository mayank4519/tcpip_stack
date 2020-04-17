#include "glthreads.h"

void glthread_node_init(gl_thread_t *glthread) {
  glthread->left = NULL;
  glthread->right = NULL;
}

void glthread_add_next(gl_thread_t *list, gl_thread_t *glnode) {
  if(list->right == NULL) {
    list->right = glnode;
    glnode->left = list;
    return;
  }
  gl_thread_t *node = list->right;
  list->right = glnode;
  glnode->left = list;
  glnode->right = node;
  node->left = glnode;
}

/*void _remove_glthread(gl_thread_node_t *glnode) {
  if(glnode->left == NULL) {
    if (glnode->right != NULL) {
      glnode->right->left = NULL;
      glnode->right = NULL;
      return;
    }
    return;
  }

  if (glnode->right == NULL) {
    glnode->left->right = NULL;
    glnode->left = NULL;
    return;
  }
  
  glnode->right->left = glnode->left->right;
  glnode->left->right = glnode->right->left;
  glnode->left = NULL;
  glnode->right = NULL;
  return;
}

void glthread_remove(gl_thread_t *list, gl_thread_node_t *glnode) {
   gl_thread_node_t *head = list->head;
   if(head == glnode)
     list->head = head->right;
   _remove_glthread(glnode);
}*/
