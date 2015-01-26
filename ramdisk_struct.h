#define CLUSTER_SIZE 512
#define MAX_NAME_SIZE 20
#define BUFFER_SIZE (CLUSTER_SIZE - (sizeof(char)) - 3*sizeof(long) - MAX_NAME_SIZE)

typedef struct rmdk_inputs_t {
	char* root_dir_path;
	int fs_size;
	char* fs_load_file;
} RamdiskInputs;

/*
	type: 'D' -> directory
		: 'F' -> file
		: 'E' -> empty

	continuous: 'Y' -> its a continuation of another cluster
			  : 'N' -> its a new cluster
*/
typedef struct cluster_t {
	char type;
	char name[MAX_NAME_SIZE];
	long sibling_cluster;
	long child_cluster;
	long next_cluster;
	char buffer[BUFFER_SIZE];
} Cluster;

struct root_t {
	Cluster* root_cluster;
};
typedef struct root_t* Root;
