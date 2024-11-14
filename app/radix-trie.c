#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "radix-trie.h"

#define pass (void)0

// Assumes IDs are always incremental. So new prefix >= any radix node

int min(int a, int b) {
  return a < b ? a : b;
}

bool is_leaf(RadixNode* rn) {
  return rn->children == NULL;
}

// returns n if the two strings differ at index n
// returns 0 if any of them are NULL or totally different
// eg. returns 0 if two strings are totally different
int find_differ_index(char* a, char* b) {
  if (a == NULL || b == NULL) {
    return 0;
  }

  int min_len = min(strlen(a), strlen(b));

  for (int i = 0; i < min_len; i++) {
    if (a[i] != b[i]) {
      return i;
    }
  }

  return min_len;
}

// end is exclusive 
char* substr(const char* src, int start, int end) {
  if (src == NULL || start < 0 || end > strlen(src) || start > end ) {
    return NULL;
  }

  char* dest = calloc(end - start + 1, sizeof(char));
  for (int i = start; i < end; i++) {
    dest[i - start] = src[i];
  }
  dest[end-start] = '\0';
  return dest;
}

void add_children(RadixNode* parent, RadixNode* child) {
  if (parent == NULL) {
    printf("!!! no parent in add children!\n");
  }

  parent->children[parent->next_child_index++] = child;
}

RadixNode* rn_create(char* prefix) {
  RadixNode* rn = calloc(1, sizeof(RadixNode));

  rn->key = calloc(strlen(prefix) + 1, sizeof(char));
  strcpy(rn->key, prefix);

  for (int i = 0; i < 10; i++) {
    rn->children[i] = NULL;
  }

  rn->next_child_index = 0;
  rn->data = NULL;

  return rn;
}

RadixData* rd_create(char* key, char* value) {
  RadixData* rd = calloc(1, sizeof(RadixData));
  rd->key = calloc(strlen(key) + 1, sizeof(char));
  rd->value = calloc(strlen(value) + 1, sizeof(char));
  rd->next = NULL;

  return rd;
}

RadixData* rd_insert_ll(RadixData* head, RadixData* target) {
  if (head == NULL) {
    return target;
  }
  RadixData* temp = head;
  while(temp->next != NULL) {
    temp = temp->next;
  }
  temp->next = target;
  return head;
}

// returns the RadixNode representing key, so you can make modifications to it.
// expects key is completely different to root.
RadixNode* rn_insert_in_children_of(RadixNode* root, char* key) {
  for (int i = 0; i < root->next_child_index; i++) {
    RadixNode* child = root->children[i];
    int differ_index = find_differ_index(child->key, key);
    if (differ_index == 0) {
      continue;
    }

    // child is the prefix of key
    // insert the rest of the key into the children of child
    if (differ_index == strlen(child->key)) {
      char* right = substr(key, differ_index, strlen(key));
      RadixNode* right_rn = rn_insert_in_children_of(child, right);
      free(right);
      return right_rn;
    }

    // child shares the same prefix as key
    if (differ_index > 0) {
      char* prefix = substr(key, 0, differ_index);
      char* left = substr(child->key, differ_index, strlen(child->key));
      char* right = substr(key, differ_index, strlen(key));
      RadixNode* prefix_rn = rn_create(prefix);
      RadixNode* right_rn = rn_create(right);
      strcpy(child->key, left);
      add_children(prefix_rn, child);
      add_children(prefix_rn, right_rn);
      root->children[i] = prefix_rn;
      
      free(prefix);
      free(left);
      free(right);
      return right_rn;
    }
  }

  // key is completely different with all children. create a new node.
  RadixNode* new_rn = rn_create(key);
  add_children(root, new_rn);
  return new_rn;

}

void rn_insert(RadixNode* root, char* id, char* key, char* value) {
  RadixNode* rn = rn_insert_in_children_of(root, id);
  RadixData* rd = rd_create(key, value);
  rn->data = rd_insert_ll(rn->data, rd);
}

void rn_print(RadixNode* rn) {
  printf("(%s)", rn->key);
  for (int i = 0; i < rn->next_child_index; i++) {
    printf(" [");
    rn_print(rn->children[i]);
    printf("]");
  }
}