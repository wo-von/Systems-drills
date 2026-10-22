#include <stdint.h>
#include <stdio.h>

int hash_function(int key); // return a hash corresponding to the provided key
void dict_add(int key, int value); // adds the (key, value) to the dict
int *dict_read(int key); // returns the pointer to the value of the corresponding key, NULL if it does not exit
int dict_init(int); // initializes a dict with a size, return 1, if it fails 



int *twoSum(int *nums, int numsSize, int target, int *returnSize);


int *twoSum(int *nums, int numsSize, int target, int *returnSize){
	if (nums == NULL)
		return NULL;
	if (numsSize < 2)
		return NULL;
	int *res = (int *)malloc(2 * sizeof(int));
	if (res == NULL){
		fprintf(stderr, "malloc failed\n");
		*returnSize = 0;
		return NULL;
	}
	res[0] = -1, res[1] = -1;
	if (dict_init(1000)){ // dict failed
		fprintf(stderr, "could not initialize dictionary\n");
		*returnSize = 0;
		free(res);
		return NULL;
	}
	// start sweeping the numbers
	for (int i = 0; i < numsSize; i++){
		if (dict_read(target - nums[i]) != NULL){
			res[0] = i;
			res[1] = *dict_read(target - nums[i]);
			*returnSize = 2;
			return res;
		} else {
			dict_add(nums[i], i);
		}

	}
	*returnSize = 0;
	free(res);
	return NULL;
}
