#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ramdisk_struct.h"

void init_ramdisk(int , char* argv[]);
RamdiskInputs* process_inputs(int , char* argv[]);
void init_root_structures(void );
Cluster* find_cluster(long );
void init_clusters(void );
long find_empty_cluster(void );
long create_directory(const char* );
void append_child_to_list(long , int );
long remove_directory(const char* );
void remove_child_from_list(long ,long , long );
long remove_file(const char* );
void remove_file_next_clusters(Cluster* );
char* find_parent_path(const char* );
long find_dir_file_cluster(const char* );
long traverse_children(long , char* );
long create_directory_cluster(const char* );
void remove_child_from_list(long , long , long );
long create_file(const char* );
long create_file_cluster(const char* );
int handle_write_data(const char* , const char* , size_t, off_t );
long create_file_overflow_cluster(void );
size_t handle_read_data(const char* , const char* , size_t , off_t );
size_t find_size_of_cluster(Cluster* );

void fill_system_stat(struct stat *stbuf, Cluster* , long );
void read_directory(const char* , void *buf, fuse_fill_dir_t );
void traverse_directory(long , void *buf, fuse_fill_dir_t );
size_t find_size(Cluster* );
void load_fs_from_file(void );

Root ramdisk_root;
RamdiskInputs* ramdisk_fs_inputs;
long MAX_CLUSTER_NUMBER;
long last_empty_cluster = 0;

RamdiskInputs* process_inputs(int argc, char* argv[]) {

	if(argc < 2) {
		printf("Incorrect usage.\nCorrect usage: ./ramdisk <path_tor_root_dir> <size> <ext_file>[optional]\n");
		exit(0);
	}
	RamdiskInputs* inputs = (RamdiskInputs*) malloc(sizeof(struct rmdk_inputs_t));
	inputs -> root_dir_path = strdup(argv[1]);
	inputs -> fs_size = atoi(argv[2]);
	if(argv[3]!=NULL && strcmp(argv[3], "-d") != 0)
		inputs -> fs_load_file = strdup(argv[3]);

	/*int i = 2;
	for(;i<argc-2;i++){
		argv[i] = argv[i+2];
	}*/

	return inputs;
}

void init_ramdisk(int argc, char* argv[]) {
	ramdisk_fs_inputs = process_inputs(argc, argv);
	init_root_structures();
	create_directory("/");
}

void init_root_structures() {

	void* fs_starter = (void*)(malloc((ramdisk_fs_inputs -> fs_size) * 1024 * 1024));
	ramdisk_root = (Root)(malloc(sizeof(struct root_t)));
	long size = (ramdisk_fs_inputs -> fs_size) * 1024 * 1024;
	size-=sizeof(struct root_t);
	MAX_CLUSTER_NUMBER = (size / sizeof(Cluster));

	ramdisk_root -> root_cluster = (Cluster*)(fs_starter + sizeof(struct root_t));
	init_clusters();
}

void init_clusters() {

	long i = 0;
	Cluster* cluster;
	for(;i<MAX_CLUSTER_NUMBER; i++) {
		cluster = find_cluster(i);
		if(cluster)
			cluster -> type = 'E';
	}
}

/*
	Accepts a cluster number and returns the start of a Cluster
	Input zero for root cluster
*/
Cluster* find_cluster(long cluster_number) {

	//printf("find cluster: %u\n", cluster_number);
	if(cluster_number < MAX_CLUSTER_NUMBER) {
		//printf("cluster_number: %ld\n", cluster_number);
		Cluster* cluster = (Cluster*)((ramdisk_root->root_cluster) + cluster_number);
		return cluster;
	} else {
		return NULL;
	}

	//printf("find cluster end: %u\n", cluster_number);
}

/*
	if returns MAX_CLUSTER_NUMBER, no empty clusters left
*/
long find_empty_cluster() {
	long count;
	int empty_found = 0;

	for(count =0; count<MAX_CLUSTER_NUMBER; count++) {
		Cluster* cluster = find_cluster(count);
		if(cluster->type == 'E'){
			empty_found = 1;
			break;
		}
	}
	if(empty_found > 0)
		return count;
	else
		return MAX_CLUSTER_NUMBER;
}

long create_directory(const char* path) {

	long created_cluster;
	//find parent using path
	created_cluster = create_directory_cluster(path);

	if(strcmp(path, "/") == 0) {
		//printf("created root directory\n");
	}
	else {
		char* parent_path = find_parent_path(path);
		long parent_cluster_number = find_dir_file_cluster(parent_path);
		//printf("create_directory: parent_cluster_number: %d\n", parent_cluster_number);
		if(parent_cluster_number == MAX_CLUSTER_NUMBER) {
			//parent not found
			//reset the cluster formed
			return parent_cluster_number;
		}
		Cluster* parent_cluster = find_cluster(parent_cluster_number);
		//printf("create_directory: parent name: %s\n", parent_cluster->name);
		if(parent_cluster->child_cluster == 0)
			parent_cluster->child_cluster = created_cluster;
		else {
			append_child_to_list(parent_cluster->child_cluster, created_cluster);
		}
	}
	return created_cluster;
}

long remove_directory(const char* path) {

	//printf("removed_directory: %s\n", path);
	long remove_cluster = find_dir_file_cluster(path);
	//printf("remove cluster number: %d\n", remove_cluster);
	Cluster* cluster = find_cluster(remove_cluster);
	if(cluster->child_cluster != 0)
		return MAX_CLUSTER_NUMBER+1;

	cluster->type='E';

	char* parent_path = find_parent_path(path);
	long parent_cluster_number = find_dir_file_cluster(parent_path);
	if(parent_cluster_number == MAX_CLUSTER_NUMBER) {
		//parent not found
		//reset the cluster formed
		//return parent_cluster_number;
		return parent_cluster_number;
	}
	Cluster* parent_cluster = find_cluster(parent_cluster_number);
	//printf("create_directory: parent name: %s\n", parent_cluster->name);
	if(parent_cluster) {
		remove_child_from_list(parent_cluster->child_cluster, remove_cluster, parent_cluster_number);
	}
	return remove_cluster;
}

long remove_file(const char* path) {

	long remove_cluster = find_dir_file_cluster(path);
	Cluster* cluster = find_cluster(remove_cluster);
	if(cluster == NULL)
		return -1;

	//long temp_sibling_cluster = cluster->sibling_cluster;
	remove_file_next_clusters(cluster);

	char* parent_path = find_parent_path(path);
	long parent_cluster_number = find_dir_file_cluster(parent_path);
	if(parent_cluster_number == MAX_CLUSTER_NUMBER) {
		return parent_cluster_number;
	}

	Cluster* parent_cluster = find_cluster(parent_cluster_number);
	if(parent_cluster) {
		remove_child_from_list(parent_cluster->child_cluster, remove_cluster, parent_cluster_number);
	}

	return remove_cluster;
}

void remove_file_next_clusters(Cluster* cluster) {

	cluster->type='E';

	while(cluster->next_cluster != 0) {
		cluster = find_cluster(cluster->next_cluster);
		cluster->type='E';
	}

}

void append_child_to_list(long first_child_cluster_number, int sibling_number) {

	Cluster* cluster = find_cluster(first_child_cluster_number);
	//printf("try to append child to node: %s\n", cluster->name);
	if(cluster->sibling_cluster == 0) {
		//printf("found cluster to append to: %s\n", cluster->name);
		cluster->sibling_cluster = sibling_number;
	} else {
		append_child_to_list(cluster->sibling_cluster, sibling_number);
	}
}

void remove_child_from_list(long first_child_cluster_number, long remove_sibling_number, long parent_cluster_number) {

	Cluster* current, *prev;
	prev = NULL;
	current = find_cluster(first_child_cluster_number);
	//printf("inside rmeove_child: %u\n", first_child_cluster_number);
	long current_child_cluster_number = first_child_cluster_number;

	while(current) {
		//printf("foind current cludter: %s\n", current->name);
		if(current_child_cluster_number == remove_sibling_number)
			break;

		prev = current;
		current_child_cluster_number = current->sibling_cluster;
		if(current_child_cluster_number == 0){
			current = NULL;
			break;
		}
		current = find_cluster(current_child_cluster_number);
	}

	if(!prev) {
		current->type = 'E';
		Cluster* parent = find_cluster(parent_cluster_number);
		parent->child_cluster = current->sibling_cluster;
		//printf("actual deletion for prev null\n");
	} else if(!current) {
		//printf("Child not found to delete\n");
	} else {
		current->type = 'E';
		prev->sibling_cluster = current->sibling_cluster;
		//printf("actual deletion for else\n");
	}
}

char* find_parent_path(const char* path) {

	char* name = (char*)(malloc((sizeof(char) * (strlen(path)) + 1)));
	if(strcmp("/", path) == 0)
		strcpy(name, path);
	else {

		char* temp = (char*)(malloc((sizeof(char) * (strlen(path)) + 1)));
		strcpy(temp, path);

		int i=strlen(temp);
		for(; i>=0; i--) {
			if(temp[i] == '/') {
				if(i == 0)
					strcpy(temp, "/");
				else
					temp[i] = '\0';
				break;
			}

		}
		strcpy(name, temp);
		free(temp);
	}
	//printf("parent path %s\n", name);
	return name;
}

long find_dir_file_cluster(const char* path) {

	//printf("find_dir_file_cluster: path: %s\n", path);
	if(strcmp(path, "/") == 0)
		return 0;
	else {
		int found_flag = 0;
		char* temp = (char*)malloc(sizeof(char) * (strlen(path) + 1));
		//char* temp_tok = (char*)malloc(sizeof(char) * (strlen(path) + 1));
		char* tok = (char*)malloc(sizeof(char) * (strlen(path) + 1));
		strcpy(temp, path);

		long parent_cluster_number = 0;

		tok = strtok(temp, "/");
		while(tok) {
			long cluster_number = (find_cluster(parent_cluster_number))->child_cluster;
			//printf("input to traverse children: cluster number %u and name: %s\n", cluster_number, tok);
			long found_cluster = traverse_children(cluster_number, tok);
			if(found_cluster == MAX_CLUSTER_NUMBER) {
				//not found
				found_flag = -1;
				break;
			}
			else {
				parent_cluster_number = found_cluster;
			}
			tok = strtok(NULL, "/");
		}

		if(found_flag >= 0) {
			return parent_cluster_number;
		}
		return MAX_CLUSTER_NUMBER;
	}

}

//cluster_number: first child of that parent,
//traverse thru the list and compare the name
//return 1, if found other wise 0
long traverse_children(long cluster_number, char* child_name) {

	//printf("traverse_children: child_name: %s\n", child_name);

	Cluster* cluster = find_cluster(cluster_number);
	/*printf("cluster name: %s\n", cluster->name);
	if(strcmp(cluster->name, child_name) == 0) {
		return cluster_number;
	} else {
		printf("cluster->sibling_cluster: %d\n", cluster->sibling_cluster);
		traverse_children(cluster->sibling_cluster, child_name);
	}*/
	while(cluster) {
		//printf("cluster name: %s  clustre no: %d\n", cluster->name, cluster_number);
		if(strcmp(cluster->name, child_name) == 0) {
			return cluster_number;
		} else {
			if(cluster->sibling_cluster == 0) {
				return MAX_CLUSTER_NUMBER;
			}
			cluster_number = cluster->sibling_cluster;
			cluster = find_cluster(cluster_number);
		}
	}
	return MAX_CLUSTER_NUMBER;

}

/*
	if returns MAX_CLUSTER_NUMBER, directory not created
	since no space left
*/
long create_directory_cluster(const char* dir_path) {

	long dir_cluster_no = find_empty_cluster();
	//printf("create_directory_cluster: found empty cluster %u\n", dir_cluster_no);
	if(dir_cluster_no == MAX_CLUSTER_NUMBER)
		return dir_cluster_no;

	char* name = (char*)(malloc((sizeof(char) * (strlen(dir_path)) + 1)));

	if(strcmp("/", dir_path) == 0)
		strcpy(name, dir_path);
	else {
		char* temp = (char*)malloc(sizeof(char) * (strlen(dir_path) + 1));
		char* tok = (char*)malloc(sizeof(char) * (strlen(dir_path) + 1));
		char* temp_tok = (char*)malloc(sizeof(char) * (strlen(dir_path) + 1));
		strcpy(temp, dir_path);

		tok = strtok(temp, "/");
		while(tok){
			strcpy(temp_tok, tok);
			tok = strtok(NULL, "/");
		}
		strcpy(name, temp_tok);
		free(temp); free(tok); free(temp_tok);
	}
	//printf("dir: name %s\n", name);

	Cluster* new_cluster = find_cluster(dir_cluster_no);
	new_cluster -> type = 'D';
	new_cluster -> sibling_cluster = 0;
	new_cluster -> child_cluster = 0;
	new_cluster -> next_cluster = 0;
	strcpy(new_cluster->name, name);

	return dir_cluster_no;
}

long create_file(const char* path) {

	long created_cluster;
	//find parent using path
	created_cluster = create_file_cluster(path);
	if(created_cluster == MAX_CLUSTER_NUMBER)
		return MAX_CLUSTER_NUMBER + 1;

	char* parent_path = find_parent_path(path);
	long parent_cluster_number = find_dir_file_cluster(parent_path);
	//printf("create_file: parent_cluster_number: %d\n", parent_cluster_number);
	if(parent_cluster_number == MAX_CLUSTER_NUMBER) {
		//parent not found
		//reset the cluster formed
		return parent_cluster_number;
	}
	Cluster* parent_cluster = find_cluster(parent_cluster_number);
	//printf("create_file: parent name: %s\n", parent_cluster->name);
	if(parent_cluster->child_cluster == 0)
		parent_cluster->child_cluster = created_cluster;
	else {
		append_child_to_list(parent_cluster->child_cluster, created_cluster);
	}

	return created_cluster;
}

int handle_write_data(const char* path, const char* buf, size_t size, off_t offset) {

	long main_file = find_dir_file_cluster(path);
	off_t temp_offset = offset;
	size_t write_size = size;
	long overflow_cluster;
	int ret_size=0;

	//printf("main_file: %d\n", main_file);
	Cluster *prev_cluster, *cluster = find_cluster(main_file);
	if(cluster == NULL)
		return MAX_CLUSTER_NUMBER;

	while(temp_offset > 0) {
		size_t temp_size = find_size_of_cluster(cluster);
		//printf("cluster->next_cluster %ld\n", cluster->next_cluster);

		if(temp_offset > temp_size) {
			temp_offset -= temp_size;
			cluster = find_cluster(cluster->next_cluster);
		}
		else if(temp_offset < temp_size) {
			//coz cluster to read from reached
			printf("wird case: \n");
			break;
		} else {
			temp_offset = temp_size;
			break;
			//cluster = find_cluster(cluster->next_cluster);
		}
	}

	//printf("outside while: value of temp_offset %d\n", temp_offset);

	off_t read_offset = 0;
	while(write_size > 0) {
		size_t temp_size;
		size_t space_left_in_buffer;
		space_left_in_buffer = BUFFER_SIZE - temp_offset;

		if(write_size < space_left_in_buffer) {
			temp_size = write_size;
		} else if(write_size > space_left_in_buffer) {
			temp_size = space_left_in_buffer;
		} else {
			temp_size = write_size;
		}
		memcpy(cluster->buffer+temp_offset, buf+read_offset, temp_size);
		temp_offset = 0;
		write_size -= temp_size;
		read_offset += temp_size;
		ret_size += temp_size;

		//printf("After writing data: %d\n", write_size);
		if(write_size > 0){
			prev_cluster = cluster;

			//Create new cluster
			overflow_cluster = create_file_overflow_cluster();
			if(overflow_cluster == -1)
				return -ENOMEM;
			//printf("created new lcuster: %ld\n", overflow_cluster);
			cluster = find_cluster(overflow_cluster);
			prev_cluster->next_cluster = overflow_cluster;
			//printf("setting prev_cluster->next_cluster: %d\n", prev_cluster->next_cluster);
		}
	}

	//printf("wrote bytes: %d\n", ret_size);
	return ret_size;
}

size_t handle_read_data(const char* path, const char* buf, size_t size, off_t offset) {

	long main_file = find_dir_file_cluster(path);

	Cluster *cluster = find_cluster(main_file);
	if(cluster == NULL)
		return MAX_CLUSTER_NUMBER;

	size_t original_size_of_file = find_size(cluster);
	//printf("Current size of file data: %d\n", original_size_of_file);
	size_t read_size = size;
	off_t temp_offset = offset;

	off_t position_to_write = 0;
	size_t ret_size = 0;
	off_t position_to_read = offset;
	//Cluster* read_start_cluster;

	size_t data_left_after_offset = original_size_of_file - offset;
	if(data_left_after_offset>=size){
		read_size = size;
	} else {
		read_size = data_left_after_offset;
	}
	//printf("calculated read_size: %d\n", read_size);

	//find from which cluster to start reading using the offset
	while(temp_offset>0) {
		size_t temp_size = find_size_of_cluster(cluster);
		//printf("1. while: find_size of cluster %d\n", temp_size);

		if(temp_offset > temp_size) {
			temp_offset -= temp_size;
			cluster = find_cluster(cluster->next_cluster);
		}
		else if(temp_offset < temp_size) {
			//coz cluster to read from reached
			break;
		} else {
			temp_offset = 0;
			cluster = find_cluster(cluster->next_cluster);
		}


		//printf("1. while: value of temp_offset %d\n", temp_offset);
		/*if(temp_offset >= BUFFER_SIZE)
			cluster = find_cluster(cluster->next_cluster);*/
	}

	//printf("found cluster to start reading from\n");
	//printf("actual read_size: %d\n", read_size);
	while(read_size > 0) {
		size_t filled_size_of_buffer = find_size_of_cluster(cluster);
		//printf("size of buf found: %d\n", filled_size_of_buffer);
		off_t relative_position_to_read = temp_offset;
		//reset temp_offset for next read:
		temp_offset = 0;

		size_t temp;
		if(read_size > (filled_size_of_buffer - relative_position_to_read))
			temp = (filled_size_of_buffer - relative_position_to_read);
		else
			temp = read_size;
		//printf("temp: %d\n", temp);
		memcpy(buf+position_to_write, (cluster->buffer + relative_position_to_read), temp);

		read_size -= temp;
		position_to_read += temp;
		position_to_write += temp;
		ret_size += temp;

		//printf("read_size: %d\n", read_size);
		//printf("ret_size: %d\n", ret_size);
		//printf("next cluster: %d\n", cluster->next_cluster);
		if(cluster->next_cluster == 0)
			break;
		cluster = find_cluster(cluster->next_cluster);
	}
	//printf("read bytes: %d\n", ret_size);
	//printf("bytes reqd to be read: %d\n", size);

	return ret_size;
}

long create_file_overflow_cluster() {
	long file_cluster_no = find_empty_cluster();
	if(file_cluster_no == MAX_CLUSTER_NUMBER)
		return -1;

	Cluster* new_cluster = find_cluster(file_cluster_no);
	new_cluster -> type = 'F';
	new_cluster -> sibling_cluster = 0;
	new_cluster -> child_cluster = 0;
	new_cluster -> next_cluster = 0;
	new_cluster->buffer[0] = 0;

	return file_cluster_no;
}

long create_file_cluster(const char* path) {
	long file_cluster_no = find_empty_cluster();
	if(file_cluster_no == MAX_CLUSTER_NUMBER)
		return file_cluster_no;


	char* name = (char*)(malloc((sizeof(char) * (strlen(path)) + 1)));

	char* temp = (char*)malloc(sizeof(char) * (strlen(path) + 1));
	char* tok = (char*)malloc(sizeof(char) * (strlen(path) + 1));
	char* temp_tok = (char*)malloc(sizeof(char) * (strlen(path) + 1));
	strcpy(temp, path);

	tok = strtok(temp, "/");
	while(tok){
		strcpy(temp_tok, tok);
		tok = strtok(NULL, "/");
	}
	strcpy(name, temp_tok);
	free(temp); free(tok); free(temp_tok);
	//printf("file: name %s\n", name);

	Cluster* new_cluster = find_cluster(file_cluster_no);
	new_cluster -> type = 'F';
	new_cluster -> sibling_cluster = 0;
	new_cluster -> child_cluster = 0;
	new_cluster -> next_cluster = 0;
	new_cluster->buffer[0] = 0;
	strcpy(new_cluster->name, name);

	return file_cluster_no;
}

size_t find_size(Cluster* cluster) {

	size_t size = 0;
	if(cluster->type == 'D')
		size = 100;
	else if(cluster->type == 'F'){

		while(cluster) {
			int count = 0;
			while((cluster->buffer[count] != '\0') && (count!=BUFFER_SIZE)) {
	        	count++;
	    	}
	    	//printf("cluster->next_cluster: %ld\n", cluster->next_cluster);
	    	if(cluster->next_cluster != 0)
	    		cluster = find_cluster(cluster->next_cluster);
	    	else
	    		cluster = NULL;
	    	size+= count;
	    }

	}
	//printf("size of file/dir: %u\n", size);
	return size;
}

size_t find_size_of_cluster(Cluster* cluster) {

	int count = 0;
	if(cluster) {
		while((cluster->buffer[count] != '\0') && (count!=BUFFER_SIZE)) {
        	count++;
    	}
    }
    return count;
}