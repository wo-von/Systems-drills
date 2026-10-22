#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
int *twoSum(int *nums, int numsSize, int target, int *returnSize);

struct Map {
	int *keys;
	int *values;
	bool *exists;
	size_t mask;
};

unsigned int hash_function(struct Map *mp, int key){
	unsigned int hash = (unsigned int)key;
	hash *= 0x9E3779B1u;
	hash ^= hash >> 16;
	return hash & mp->mask;
}
void free_map(struct Map *mp){
	free(mp->keys);
	mp->keys = NULL;
	free(mp->values);
	mp->values = NULL;
	free(mp->exists);
	mp->exists = NULL;
	mp->mask = 0;
	return;
}

bool init_map(struct Map *mp, size_t capacity){
	size_t cap = 1;
	while (cap < capacity){
		cap <<= 1;
	}
	mp->keys = malloc(cap * sizeof(*mp->keys));
	mp->values = malloc(cap * sizeof(*mp->values));
	mp->exists = calloc(cap, sizeof(*mp->exists));
	if (!mp->keys || !mp->values || !mp->exists){
		free_map(mp);
		return false;
	}
	mp->mask = cap - 1;
	return true;
}

// returns the index of the key in the map, empty or not
size_t probe_map(struct Map *mp, int key){
	size_t index = hash_function(mp, key);
	while(mp->exists[index] && (mp->keys[index] != key)){
		index = (index + 1) & mp->mask;
	}
	return index;
}

void set_map(struct Map *mp, int key, int value){
	size_t index = probe_map(mp, key);
	mp->keys[index] = key;
	mp->values[index] = value;
	mp->exists[index] = true;
	return;
}

bool get_map(struct Map *mp, int key, int *value){
	size_t index = probe_map(mp, key);
	if (mp->exists[index]){
		*value = mp->values[index];
		return true;
	}
	return false;
}

int *twoSum(int *nums, int numsSize, int target, int *returnSize){
	if (nums == NULL) {
		*returnSize = 0;
		return NULL;
	}
	if (numsSize < 2){
		*returnSize = 0;
		return NULL;
	
	}
	int *res = (int *)malloc(2 * sizeof(int));
	if (res == NULL){
		fprintf(stderr, "malloc failed\n");
		*returnSize = 0;
		return NULL;
	}
	struct Map mp;
	if (!init_map(&mp, 2 * (size_t)numsSize)){ // dict failed
		fprintf(stderr, "could not initialize dictionary\n");
		*returnSize = 0;
		free(res);
		return NULL;
	}
	// start sweeping the numbers
	for (int i = 0; i < numsSize; i++){
		int want = target - nums[i];
		int second_index;
		if (get_map(&mp, want, &second_index)){
			res[0] = second_index;
			res[1] = i;
			*returnSize = 2;
			free_map(&mp);
			return res;
		}
		set_map(&mp, nums[i], i);
	}
	*returnSize = 0;
	free(res); free_map(&mp);
	return NULL;
}



int main(void){

	int returnsize;

	int *nums = malloc(2 * sizeof(int));

	nums[0] = 3;
	nums[1] = 3;
	int target = 6;
	int *res = twoSum(nums, 2, target, &returnsize); 
	assert(returnsize == 2);
	assert(res[0]==0);
	assert(res[1]==1);
	free(nums);
	free(res);

	nums = malloc(4 * sizeof(int));

	nums[0] = 2;
	nums[1] = 7;
	nums[2] = 11;
	nums[3] = 15;
	target = 9;
	res = twoSum(nums, 4, target, &returnsize); 
	assert(returnsize == 2);
	assert(res[0]==0);
	assert(res[1]==1);
	free(nums);
	free(res);

	nums = malloc(3 * sizeof(int));

	nums[0] = 3;
	nums[1] = 2;
	nums[2] = 4;
	target = 6;
	res = twoSum(nums, 3, target, &returnsize); 
	assert(returnsize == 2);
	assert(res[0]==1);
	assert(res[1]==2);
	free(nums);
	free(res);

	nums = malloc(4 * sizeof(int));

	nums[0] = -3;
	nums[1] = 4;
	nums[2] = 3;
	nums[3] = 90;
	target = 0;
	res = twoSum(nums, 4, target, &returnsize); 
	assert(returnsize == 2);
	assert(res[0]==0);
	assert(res[1]==2);
	free(nums);
	free(res);

	res = twoSum((int[]){2, 7, 11, 15}, 4, 22, &returnsize);

	assert(returnsize == 2);
	assert(res[0]==1);
	assert(res[1]==3);
	free(res);
	return 0;
}

