
#define ID_LENGTH 66 // [i32]-[i32]
#define MAX_RADIX_NODES 100
#define MAX_RADIX_DATA 10



typedef struct RadixData {
	char* key;
	char* value;
	struct RadixData* next;
} RadixData;

typedef struct RadixNode {
	char* key;								  // consists part or all of a ID
	struct RadixNode* children[11];		// a linked list of children
	int next_child_index;

	// struct RadixNode* parent;

	RadixData* data;					   	// if stream is leaf, 
} RadixNode;

// typedef struct RadixTrie {
//   RadixNode* root;							// this is a dummy root. We store data from its children.
// } RadixTrie;



RadixNode* rn_create(char* prefix);
RadixData* rd_create(char* key, char* value);
RadixNode* rn_insert_in_children_of(RadixNode* root, char* key);
void rn_insert(RadixNode* root, char* id, char* key, char* value);
char* rn_get_latest_key(RadixNode* root);


void rn_print(RadixNode* rn);
bool check_stream_id(char* result, char* id);
char* rn_partially_generate_key(RadixNode* root, char* key);
char* rn_generate_key(RadixNode* root);
int rn_traverse(RadixNode* root, char* start, char* end, RadixNode *acc[MAX_RADIX_NODES], char* acc_id[MAX_RADIX_NODES]);
char* format_radix_data(RadixData* rd_head);
char* format_radix(RadixNode* rn[], char* acc_id[], int length, char* result);
void increment_seq_part(char* dest, char* id);