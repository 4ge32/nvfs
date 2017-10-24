#include <stdio.h>
#include <string.h>
#include <libpmemobj.h>

#define MAX_BUF_LEN 10

POBJ_LAYOUT_BEGIN(string_store);
POBJ_LAYOUT_ROOT(string_store, struct my_root);
POBJ_LAYOUT_END(string_store);

struct my_root {
	PMEMobjpool *pop;
	char buf[MAX_BUF_LEN];
};

int mkfs_pmemobj(const char *fname)
{
	struct my_root *pm = calloc(1, sizeof(*obj));
	if (!obj)
		return -1;

	int ret = 0;

	pm->pop = pmemobj_create(fname, POBJ_LAYOUT_NAME(test_fs));
	if (pm->pop == NULL) {
		perror("pmemobj_open");
		return 1;
	}


}

int reader(int argc, char *argv[])
{
	if (argc != 3) {
		printf("usage: %s file-name\n", argv[0]);
		return 1;
	}

	PMEMobjpool *pop = pmemobj_open(argv[1],
					POBJ_LAYOUT_NAME(string_store));
	if (pop == NULL) {
		perror("pmemobj_open");
		return 1;
	}

	TOID(struct my_root) root = POBJ_ROOT(pop, struct my_root);

	printf("%s\n", D_RO(root)->buf);

	pmemobj_close(pop);

	return 0;
}

int writer(int argc, char *argv[])
{
	if (argc != 3) {
		printf("usage: %s file-name\n", argv[0]);
		return 1;
	}

	PMEMobjpool *pop = pmemobj_create(argv[1],
			POBJ_LAYOUT_NAME(string_store), PMEMOBJ_MIN_POOL, 0666);

	if (pop == NULL) {
		perror("pmemobj_create");
		return 1;
	}

	char buf[MAX_BUF_LEN] = {0};
	int num = scanf("%9s", buf);

	if (num == EOF) {
		fprintf(stderr, "EOF\n");
		return 1;
	}
	TOID(struct my_root) root = POBJ_ROOT(pop, struct my_root);

	TX_BEGIN(pop) {
		TX_MEMCPY(D_RW(root)->buf, buf, strlen(buf));
	} TX_END

	pmemobj_close(pop);
	return 0;
}

int main(int argc, char *argv[])
{
	if (!strcmp(argv[2], "writer"))
		writer(argc, argv);
	else if (!strcmp(argv[2], "reader"))
		reader(argc, argv);

	return 0;
}

/* experiment part */
void test_cprog(int argc, char *argv[])
{
	if (argc != 2) {
		printf("usage: %s file-name\n", argv[0]);
		return;
	}

	PMEMobjpool *pop = pmemobj_create(argv[1],
			POBJ_LAYOUT_NAME(string_store), PMEMOBJ_MIN_POOL, 0666);

	if (pop == NULL) {
		perror("pmemobj_create");
		return;
	}

	char *buf = "#include<stdio.h> int main() {printf(\"Hello, World\n\");return 0;}";

	TOID(struct my_root) root = POBJ_ROOT(pop, struct my_root);

	TX_BEGIN(pop) {
		TX_MEMCPY(D_RW(root)->buf, buf, strlen(buf));
	} TX_END

	pmemobj_close(pop);
}
