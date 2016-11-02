#include <stdio.h>
#include <crete/harness.h>

int harness(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	int x;

	crete_make_concolic(&x, sizeof(x), "x");

	if(x == 42)
		printf("42!\n");
	else 
		printf("not 42!\n");

	return 0;
}

int main(int argc, char* argv[])
{
	crete_initialize(argc, argv);
	crete_start(harness);

	return 0;
}
