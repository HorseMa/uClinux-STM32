
void *rtlglue_malloc(unsigned int size) {
	
	return malloc(size);
}

void rtlglue_free(void *APTR) {

	free(APTR);
}

