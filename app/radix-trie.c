#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include "radix-trie.h"
#include "format.h"
#include "timer.h"

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

  for (int i = 0; i < 11; i++) {
    rn->children[i] = NULL;
  }

  rn->next_child_index = 0;
  rn->data = NULL;

  return rn;
}

RadixData* rd_create(char* key, char* value) {
  RadixData* rd = calloc(1, sizeof(RadixData));
  rd->key = calloc(strlen(key) + 1, sizeof(char));
  strcpy(rd->key, key);

  rd->value = calloc(strlen(value) + 1, sizeof(char));
  strcpy(rd->value, value);

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

char* rn_get_latest_key(RadixNode* root) {
  char* buffer = calloc(ID_LENGTH, sizeof(char));
  RadixNode* cur = root;
  while(cur->next_child_index > 0) {
    cur = cur->children[cur->next_child_index - 1];
    strcat(buffer, cur->key);
  }
  return buffer;
}

// assumes id is valid
unsigned long get_time_part(char* id) {
  char* c_time = substr(id, 0, strstr(id, "-") - id);
  unsigned long time = (unsigned long) atoll(c_time);
  free(c_time);
  return time;
}

// assumes id is valid
unsigned long get_seq_part(char* id) {
  return (unsigned long) atoi(strstr(id, "-") + 1);
}

void increment_seq_part(char* dest, char* id) {
  unsigned long time_part = get_time_part(id);
  unsigned long seq_part = get_seq_part(id);

  if (seq_part == ULONG_MAX) {
    sprintf(dest, "%lu-%lu", time_part + 1, 0);
    return;
  }

  sprintf(dest, "%lu-%lu", time_part, seq_part + 1);
}

// assumes time part: xxxx-*
char* rn_partially_generate_key(RadixNode* root, char* key) {
  // compare the front len-2 characters. if equal, *++. if bigger

  char* result = calloc(strlen(key) + 2, sizeof(char));

  char* latest_key = rn_get_latest_key(root);
  if (latest_key == NULL || strlen(latest_key) == 0) {
    strcpy(result, key);
    result[strlen(key) - 1] = get_time_part(key) == 0 ? '1' : '0';
    latest_key != NULL ? free(latest_key) : pass;
    return result;
  }

  unsigned long latest_key_time = get_time_part(latest_key);
  unsigned long key_time = get_time_part(key);
  if (latest_key_time == key_time) {
    increment_seq_part(result, latest_key);

    latest_key != NULL ? free(latest_key) : pass;
    return result;
  }

  if (latest_key_time > key_time) {

    latest_key != NULL ? free(latest_key) : pass;
    return NULL;
  }

  if (latest_key_time < key_time) {
    strcpy(result, key);
    result[strlen(result) - 1] = get_time_part(key) == 0 ? '1' : '0';
    latest_key != NULL ? free(latest_key) : pass;
    return result;
  }

}

char* rn_generate_key(RadixNode* root) {
  unsigned long cur_time = get_time_in_ms();
  char buffer[35] = {0};
  sprintf(buffer, "%lu-*", cur_time);
  char* result = rn_partially_generate_key(root, buffer);
  return result;
}

void rn_print(RadixNode* rn) {
  printf("(%s)", rn->key);
  for (int i = 0; i < rn->next_child_index; i++) {
    printf(" [");
    rn_print(rn->children[i]);
    printf("]");
  }
}

void rn_traverse_in_children_of(RadixNode* root, RadixNode* acc[MAX_RADIX_NODES], char* acc_id[MAX_RADIX_NODES],
                                 int* acc_index, char* buffer, char* start, char* end) {
  char cur[ID_LENGTH] = {0};
  sprintf(cur, "%s%s", buffer, root->key);
  // printf("cur: %s, %d, %d\n", cur, strncmp(start, cur, strlen(cur)) > 0, strncmp(end, cur, strlen(cur)) < 0);

  if (strlen(root->key) > 0 && (strncmp(start, cur, strlen(cur)) > 0 || strncmp(end, cur, strlen(cur)) < 0)) {
    return;
  }

  // printf("-pass\n");

  if (root->next_child_index == 0) {
    // printf("--adding");
    acc_id[*acc_index] = calloc(strlen(cur) + 1, sizeof(char));
    strcpy(acc_id[*acc_index], cur);

    acc[*acc_index] = root;

    (*acc_index)++;
    return;
  }
  

  // printf("-acc_index: %d\n", *acc_index);


  for (int i = 0; i < root->next_child_index; i++) {
    rn_traverse_in_children_of(root->children[i], acc, acc_id, acc_index, cur, start, end);
  }
}

// return number of items found
int rn_traverse(RadixNode* root, char* start, char* end, RadixNode *acc[MAX_RADIX_NODES], char* acc_id[MAX_RADIX_NODES]) {
  memset(acc, '\0', MAX_RADIX_NODES);
  memset(acc_id, '\0', MAX_RADIX_NODES);

  char buffer[ID_LENGTH] = {0};
  int index = 0;
  // printf("start\n");
  rn_traverse_in_children_of(root, acc, acc_id, &index, buffer, start, end);
  printf("End of traverse. items found: %d\n", index);

  return index;

}


// Functions related to ID

// true if id is correct, false o/w
bool check_stream_id(char* result, char* id) {
  // only check when partial or explicit generation
	if (strstr(id, "*") == NULL && strstr(id, "-") == NULL) {
		get_simple_error(result, "ERR", "The ID specified in XADD must include '-' or '*'");
		return false;
	}

  // only check when partial generation
  if (strstr(id,"-*") != NULL && strstr(id,"-*") == id) {
    get_simple_error(result, "ERR", "The ID specified in XADD must have a time part");
		return false;
  }

  // only check at explicit generation
	if (strstr(id,"*") == NULL && strcmp(id, "0-0") <= 0) {
		get_simple_error(result, "ERR", "The ID specified in XADD must be greater than 0-0");
		return false;
	}

	return true;
}

void free_strings(char** src, int length) {
	for (int i = 0; i < length; i++) {
		src[i] != NULL ? free(src[i]) : pass;
	}
	src != NULL ? free(src) : pass;
}	

// traverses through the ll
// returns a resp list, with radix data as its entries
char* format_radix_data(RadixData* rd_head) {
  char* acc[2 * MAX_RADIX_DATA] = {0};
  int index = 0;

  while (rd_head != NULL) {
    acc[index] = calloc(strlen(rd_head->key) + 10, sizeof(char));
    get_bulk_string(acc[index++], rd_head->key);

    acc[index] = calloc(strlen(rd_head->value) + 10, sizeof(char));
    get_bulk_string(acc[index++], rd_head->value);

    rd_head = rd_head->next;
  }
  // printf("inside format radix data\n");

  char* result = calloc(MAX_ARGUMENT_LENGTH, sizeof(char));

  get_resp_array_pointer(result, acc, index);
  // printf("after resp array pointer\n");


  for (int i = 0; i < index; i++) {
    free(acc[i]);
  }

  return result;
}

char* format_radix(RadixNode* rn[], char* acc_id[], int length, char* result) {

  char** result_arr = calloc(length, sizeof(char*));
  for (int i = 0; i < length; i++) {    
    char* rd = format_radix_data(rn[i]->data);
    // printf("- rd: %s\n", rd);

    char* input[2] = {0};

    input[0] = calloc(ID_LENGTH + 10, sizeof(char));
    get_bulk_string(input[0], acc_id[i]);

    input[1] = rd;

    result_arr[i] = calloc(MAX_ARGUMENT_LENGTH, sizeof(char));
    get_resp_array_pointer(result_arr[i], input, 2);

    free(input[0]);
    free(rd);

  }

  get_resp_array_pointer(result, result_arr, length);
  free_strings(result_arr, length);
  
}
