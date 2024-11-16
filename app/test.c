// #include <stdbool.h>
// #include <stdlib.h>
// #include <string.h>
// #include <stdio.h>
// #include "radix-trie.h"
// #include "format.h"
// #include "timer.h"

// #define pass (void)0
// #define MAX_RADIX_NODES 100


// int main() {
//   RadixNode* root = rn_create("");
//   RadixNode* n = rn_insert_in_children_of(root, "1234");
//   RadixData* rd_1 = rd_create("a", "aa");
//   n->data = rd_1;
//   // rn_insert_in_children_of(root, "1244");
//   // rn_insert_in_children_of(root, "1245");
//   // RadixNode* n = rn_insert_in_children_of(root, "1246");


//   char result[BUFFER_SIZE] = {0};
//   RadixNode* acc_rn[MAX_RADIX_NODES] = {0};
// 	char* acc_id[MAX_RADIX_NODES] = {0};
// 	int num_rn = rn_traverse(root, "1234", "1245", acc_rn, acc_id);
//   format_radix(acc_rn, acc_id, num_rn, result);

//   // rn_print(root);
//   printf("\n!!!result: %s\n", result);


//   // RadixData* rd_1 = rd_create("a", "aa");
//   // RadixData* rd_2 = rd_create("b", "bb");
//   // rd_1->next = rd_2;

//   // char* rd_data = format_radix_data(rd_1);
//   // printf("data: %s\n", rd_data);


//   // rn_insert_in_children_of(root, "100");

//   // rn_insert_in_children_of(root, "124-0");
//   // rn_insert_in_children_of(root, "12-0");

//   // rn_print(root);
//   // printf("\n%s\n", n->key);

//   // char t[100] = "hello";
//   // char* result = substr(t, 1, strlen(t));
//   // printf("%s\n", result);
// }