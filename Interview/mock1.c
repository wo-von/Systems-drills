#define NUM_REGIONS (1UL << 12)

struct region {
	uint64_t guest_physical_start;
	uint64_t memsize;
	uint64_t host_virtual;
};


int64_t translate(struct region regions, uint64_t tocheck) {

	for (size_t i = 0; i < NUM_REGIONS; i++) {

		if ((tocheck > regions[i]->guest_physical_start) && (tocheck < regions[i]->guest_physical_start + regions[i]->memsize)){
			return (tocheck - regions[i]->guest_physical_start) + regions[i]->host_virtual;
		}

	}
	return -1;
}



int main(void) {
	
	uint64_t tocheck; // address to be checked if it is registered
	struct region regions[NUM_REGIONS];
	int64_t host_physical = check_within(regions, tocheck);
	if (host_physical < 0){
		error();
	}

}
