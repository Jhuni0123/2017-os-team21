#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char** argv)
{
	if(argc < 3){
		printf("Invalid input\n");
		return 1;
	}
	
	FILE* f = fopen(argv[1], "w");
	if(argc >= 4) {
		int t = atoi(argv[3]);
		sleep(t);
	}
	fprintf(f, "%s", argv[2]);
	fclose(f);
	return 0;
}

