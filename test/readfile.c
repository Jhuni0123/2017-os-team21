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
	char c;
	FILE* f = fopen(argv[1], "r");
	FILE* f2 = fopen("out", "w");
	if(f == NULL)
		return 1;
	fprintf(f2, "1");
	fscanf(f, "%c", &c);
	
	int t = atoi(argv[2]);
	sleep(t);
	
	fprintf(f2, "2");
	fscanf(f, "%c", &c);

	fprintf(f2, "3");
	fclose(f);
	fclose(f2);
	return 0;
}

