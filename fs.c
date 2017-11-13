#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <libgen.h>
#include <libpmemobj.h>


POBJ_LAYOUT_BEGIN(inode);
POBJ_LAYOUT_ROOT(r_inode, struct r_inode);
POBJ_LAYOUT_TOID(inode, struct pc_inode);
POBJ_LAYOUT_END(inode);

struct r_inode {
	TOID(struct pc_inode) head;
	TOID(struct pc_inode) tail;
};

struct pc_inode {
	char name[128];
	char data[4096];
	struct stat st;

	TOID(struct pc_inode) next;
};

static char *pmem_pool;
static PMEMobjpool *pop;
static TOID(struct r_inode) root;

static TOID(struct pc_inode) pre_search_inode(const char *path)
{
	TOID(struct pc_inode) inode = TOID_NULL(struct pc_inode);
	TOID(struct pc_inode) p_inode = TOID_NULL(struct pc_inode);
	char *s_path;

	inode = D_RO(root)->head;

	if (!strcmp(path, "/"))
		return inode;

	s_path = (char *)calloc(strlen(path), sizeof(char));
	strcpy(s_path, path);
	s_path = s_path + 1;

	while (!TOID_IS_NULL(inode)) {
		if (!strcmp(D_RO(inode)->name, s_path))
			return p_inode;
		p_inode = inode;
		inode = D_RO(inode)->next;
	}

	return TOID_NULL(struct pc_inode);
}

static TOID(struct pc_inode) search_inode(const char *path)
{
	TOID(struct pc_inode) inode = TOID_NULL(struct pc_inode);
	char *s_path;

	inode = D_RO(root)->head;

	if (!strcmp(path, "/"))
		return inode;

	s_path = (char *)calloc(strlen(path), sizeof(char));
	strcpy(s_path, path);
	s_path = s_path + 1;

	while (!TOID_IS_NULL(inode)) {
		if (!strcmp(D_RO(inode)->name, s_path))
			return inode;
		inode = D_RO(inode)->next;
	}

	return TOID_NULL(struct pc_inode);
}

static int pc_getattr(const char *path, struct stat *stbuf)
{
	TOID(struct pc_inode) inode = TOID_NULL(struct pc_inode);

	inode = search_inode(path);
	if (TOID_IS_NULL(inode)) {
		return -ENOENT;
	} else if (!strcmp(path, "/")) {
		memset(stbuf, 0, sizeof(struct stat));
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (!strcmp(path + 1, D_RO(inode)->name)){
		memset(stbuf, 0, sizeof(struct stat));
		stbuf->st_mode = D_RO(inode)->st.st_mode;
		stbuf->st_nlink = D_RO(inode)->st.st_nlink;
		stbuf->st_size = D_RO(inode)->st.st_size;
	} else
		return -ENOENT;

	return 0;
}

static int pc_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	TOID(struct pc_inode) inode = TOID_NULL(struct pc_inode);
	(void) path;
	(void) fi;
	(void) offset;

	inode = D_RO(root)->head;
	inode = D_RO(inode)->next;

	if (TOID_IS_NULL(inode))
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	do {
		filler(buf, D_RO(inode)->name, NULL, 0);
		inode = D_RO(inode)->next;
	} while (!TOID_IS_NULL((inode)));

        return 0;
}

static int pc_open(const char *path, struct fuse_file_info *fi)
{
	(void) fi;
	(void) path;

	return 0;
}

static int pc_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	TOID(struct pc_inode) inode = TOID_NULL(struct pc_inode);
	size_t len;
	(void) fi;

	inode = search_inode(path);
	if (TOID_IS_NULL(inode))
		return -ENOENT;

	len = D_RO(inode)->st.st_size;

	if ((size_t)offset < len)
	{
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, D_RO(inode)->data + offset, size);
	}
	else
		size = 0;

	return size;
}

static int pc_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	TOID(struct pc_inode) inode = TOID_NULL(struct pc_inode);
	char *tmp;
	char buffer[BUFSIZ];
	(void) fi;

	inode = search_inode(path);
	if (TOID_IS_NULL(inode))
		return -ENOENT;

	if ((size_t)(size + offset) > (size_t)D_RO(inode)->st.st_size) {
		if (D_RO(inode)->st.st_size != 0) {
			tmp = (char *)malloc(size + D_RO(inode)->st.st_size + 1);
			memcpy(tmp, D_RO(inode)->data, D_RO(inode)->st.st_size);
			sprintf(buffer, "%s%s", tmp, buf);
			free(tmp);
		} else {
			sprintf(buffer, "%s", buf);
		}
	} else {
		sprintf(buffer, "%s", buf);
	}

	TX_BEGIN (pop) {
		D_RW(inode)->st.st_size += size;
		TX_MEMCPY(D_RW(inode)->data, buffer, strlen(buffer));
	} TX_END

	return size;
}

static int pc_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	TOID(struct pc_inode) p_inode = TOID_NULL(struct pc_inode);
	p_inode = D_RW(root)->tail;
	uint64_t len = strlen(path);
	char *fname = (char *)malloc(sizeof(char *) * len);
	strcpy(fname, path);
	fname = fname + 1;
	(void) fi;

	TX_BEGIN(pop) {
		TOID(struct pc_inode) inode = TX_NEW(struct pc_inode);
		TX_MEMCPY(D_RW(inode)->name, fname, strlen(fname));
		D_RW(inode)->next = TOID_NULL(struct pc_inode);
		D_RW(inode)->st.st_mode = mode | 0777;
		D_RW(inode)->st.st_nlink = 1;
		D_RW(inode)->st.st_size = 0;

		D_RW(inode)->next = TOID_NULL(struct pc_inode);
		D_RW(p_inode)->next = inode;
		D_RW(root)->tail = inode;
	} TX_END

	return 0;
}

static int pc_unlink(const char *path)
{
	TOID(struct pc_inode) inode = TOID_NULL(struct pc_inode);
	TOID(struct pc_inode) old = TOID_NULL(struct pc_inode);

	inode = pre_search_inode(path);

	TX_BEGIN(pop) {
		old = D_RO(inode)->next;
		D_RW(inode)->next = D_RO(old)->next;
		TX_FREE(old);
	} TX_END

	return 0;
}

static void *pc_init(struct fuse_conn_info *conn)
{
	(void) conn;

	pop = pmemobj_open(pmem_pool, POBJ_LAYOUT_NAME(inode));
	if (pop == NULL) {
		perror("pmemobj_open");
	}

	root = POBJ_ROOT(pop, struct r_inode);

	TX_BEGIN(pop) {
		TX_ADD(root);
	} TX_END

	return NULL;
}

static struct fuse_operations fs_oper = {
		.init	 = pc_init,
		.getattr = pc_getattr,
		.readdir = pc_readdir,
		.open	 = pc_open,
		.read	 = pc_read,
		.write	 = pc_write,
		.create  = pc_create,
		.unlink  = pc_unlink,
};

#define MKFS "mkfs.fs"

static int mkfs(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Usage: ./"MKFS" <file>\n");
		return -1;
	}

	pmem_pool = argv[1];

	printf("mkfs - create -start\n");
	pop = pmemobj_create(pmem_pool, POBJ_LAYOUT_NAME(inode), PMEMOBJ_MIN_POOL, 0666);

	if (pop == NULL) {
		perror("pmemobj_create");
		return -1;
	}
	root = POBJ_ROOT(pop, struct r_inode);

	TX_BEGIN(pop) {
		TX_ADD(root);
		TOID(struct pc_inode) inode = TX_NEW(struct pc_inode);
		TX_MEMCPY(D_RW(inode)->name, "", strlen(""));
		D_RW(inode)->next = TOID_NULL(struct pc_inode);
		D_RW(inode)->st.st_mode = (S_IFDIR | 0755);
		D_RW(inode)->st.st_nlink = 2;

		D_RW(root)->head = inode;
		printf("hey - %s\n", D_RO(inode)->name);
#ifdef __DEBUG__

		TOID(struct pc_inode) iinode = TX_NEW(struct pc_inode);
		TX_MEMCPY(D_RW(iinode)->name, "test", strlen("test"));
		D_RW(iinode)->next = TOID_NULL(struct pc_inode);
		D_RW(iinode)->st.st_mode = (S_IFREG | 0755);
		D_RW(iinode)->st.st_nlink = 1;
		TX_MEMCPY(D_RW(iinode)->data, "Hello, World!\n", strlen("Hello, World!\n"));
		D_RW(iinode)->st.st_size = strlen(D_RO(iinode)->data);

		D_RW(inode)->next = iinode;
		printf("hey - %s\n", D_RO(iinode)->name);

		TOID(struct pc_inode) iiinode = TX_NEW(struct pc_inode);
		TX_MEMCPY(D_RW(iiinode)->name, "file", strlen("file"));
		D_RW(iiinode)->next = TOID_NULL(struct pc_inode);
		D_RW(iiinode)->st.st_mode = (S_IFREG | 0755);
		D_RW(iiinode)->st.st_nlink = 1;
		char *con = "#include<stdio.h>\nint main(void) {\nprintf(\"hello\");\nreturn 0;}\n";
		TX_MEMCPY(D_RW(iiinode)->data, con, strlen(con));
		D_RW(iiinode)->st.st_size = strlen(D_RO(iiinode)->data);


		D_RW(iinode)->next = iiinode;
		printf("hey - %s\n", D_RO(iiinode)->name);

		D_RW(root)->tail = iiinode;
#endif
		D_RW(root)->tail = inode;
	} TX_END;

	printf("mkfs - create - end\n");
	return 1;
}


int main(int argc, char *argv[])
{
	int ret = 0;
	char *bname = basename(argv[0]);

	if (!strcmp(MKFS, bname)) {
		mkfs(argc, argv);
	} else {
		if (argc == 6) {
			pmem_pool = argv[4];
			argv[4] = argv[5];
			ret = fuse_main(--argc, argv, &fs_oper, NULL);
			pmemobj_close(pop);
		}
		if (argc == 5) {
			pmem_pool = argv[3];
			argv[3] = argv[4];
			ret = fuse_main(--argc, argv, &fs_oper, NULL);
			pmemobj_close(pop);
		}

	}

	return ret;
}
