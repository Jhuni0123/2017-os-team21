#include <unistd.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
	int t = atoi(argv[1]);
	sleep(t);
	return 0;
}
