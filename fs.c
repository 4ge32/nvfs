#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <libpmemobj.h>

POBJ_LAYOUT_BEGIN(inode);
POBJ_LAYOUT_ROOT(r_inode, struct r_inode);
POBJ_LAYOUT_TOID(inode, struct pc_inode);
POBJ_LAYOUT_END(inode);

struct r_inode {
	TOID(struct pc_inode) head;
};

struct pc_inode {
	char 	*name;
	void	*data;
	struct stat st;

	TOID(struct pc_inode) next;
};

struct c_inode {
	char *name;
	void *data;
	struct c_inode *next;
};

static char *pmem_pool = "pmem_cache";
PMEMobjpool *pop;
static TOID(struct r_inode) root;

static TOID(struct pc_inode) search_inode(const char *path)
{
	printf("start - search_inode\n");
	TOID(struct pc_inode) inode = TOID_NULL(struct pc_inode);
	char *s_path;

	inode = D_RO(root)->head;

	if (!strcmp(path, "/"))
		return inode;

	printf("where?");

	s_path = (char *)calloc(strlen(path), sizeof(char));
	strcpy(s_path, path);
	s_path = s_path + 1;
	printf("\npath %s\n", path);
	printf("spath %s\n", s_path);

	while (!TOID_IS_NULL(inode)) {
		printf("search roop: %s\n", D_RO(inode)->name);
		if (!strcmp(D_RO(inode)->name, s_path))
			return inode;
		inode = D_RO(inode)->next;
	}

	printf("end - search_inode\n");
	return TOID_NULL(struct pc_inode);
}

static int pc_getattr(const char *path, struct stat *stbuf)
{
	printf("start - getattr\n");
	TOID(struct pc_inode) inode = TOID_NULL(struct pc_inode);

	inode = search_inode(path);
	if (TOID_IS_NULL(inode)) {
		printf("hello? noentry?\n");
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

	printf("end - getattr\n");
	return 0;
}

static int pc_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	printf("start - readdir\n");
	TOID(struct pc_inode) inode = TOID_NULL(struct pc_inode);

	printf("fname:%s\n", path);

	inode = D_RO(root)->head;
	inode = D_RO(inode)->next;

	if (TOID_IS_NULL(inode))
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	do {
		printf("readdir roop %s\n", D_RO(inode)->name);
		filler(buf, D_RO(inode)->name, NULL, 0);
		inode = D_RO(inode)->next;
	} while (!TOID_IS_NULL((inode)));

	printf("end - readdir\n");
        return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi)
{
	printf("hello_open");
	printf("hello_open end");
	return 0;
}

static int pc_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	printf("start - pc_read\n");
	TOID(struct pc_inode) inode = TOID_NULL(struct pc_inode);
	size_t len;

	inode = search_inode(path);
	printf("read_PASS?\n");
	if (TOID_IS_NULL(inode))
		return -ENOENT;
	printf("read_PASS?\n");

	len = D_RO(inode)->st.st_size;
	printf("%lu:%s\n", len, (char *)D_RO(inode)->data);
	printf("HEY?\n");

	if (offset < len)
	{
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, D_RO(inode)->data + offset, size);
	}
	else
		size = 0;

	printf("end - pc_read\n");
	return size;
}

static int pc_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	TOID(struct pc_inode) inode = TOID_NULL(struct pc_inode);
	void *tmp;

	inode = search_inode(path);
	if (TOID_IS_NULL(inode))
		return -ENOENT;

	if (size + offset > D_RO(inode)->st.st_size) {
		tmp = (char *)malloc(D_RO(inode)->st.st_size + size);
		if (tmp == NULL)
			return -ENOMEM;
		if (D_RO(inode)->data) {
			memcpy(tmp, D_RO(inode)->data, D_RO(inode)->st.st_size);
			printf("if write : %s -end\n", (char *)tmp);
		}
		D_RW(inode)->data = tmp;
		printf("iff write : %s -end\n", (char *)D_RO(inode)->data);
		D_RW(inode)->st.st_size += size;
	}

	TX_BEGIN (pop) {
		TX_MEMCPY(D_RW(inode)->data + offset, buf, size);
	} TX_END

	printf("itad write : %s\n", buf);
	printf("itad write : %s\n", (char *)D_RO(inode)->data + offset);

	printf("1write : %s\n", buf);
	//printf("2write : %s\n", (char *)tmp);
	printf("3write : %s\n", (char *)D_RO(inode)->data);
	printf("4write : %s\n", (char *)D_RO(inode)->data + offset);

	return size;
}

static void *pc_init(struct fuse_conn_info *conn) {
	printf("start-init\n");
	DIR *d;
	if ((d = opendir("/home/fumiya/experiment/experiment/pmem_cache"))) {
		pop = pmemobj_open(pmem_pool, POBJ_LAYOUT_NAME(inode));
		if (pop == NULL)
			goto end;
		printf("exit memory map\n");
		goto end;
	}

	pop = pmemobj_create(pmem_pool, POBJ_LAYOUT_NAME(inode), PMEMOBJ_MIN_POOL, 0666);
	if (pop == NULL) {
		perror("pmemobj_create");
		return NULL;
	}

	root = POBJ_ROOT(pop, struct r_inode);


	TX_BEGIN(pop) {
		TX_ADD(root);
		TOID(struct pc_inode) inode = TX_NEW(struct pc_inode);
		D_RW(inode)->name = "";
		D_RW(inode)->next = TOID_NULL(struct pc_inode);
		D_RW(inode)->st.st_mode = (S_IFDIR | 0755);
		D_RW(inode)->st.st_nlink = 2;

		D_RW(root)->head = inode;
		printf("hey - %s\n", D_RO(inode)->name);

		TOID(struct pc_inode) iinode = TX_NEW(struct pc_inode);
		D_RW(iinode)->name = "test";
		D_RW(iinode)->next = TOID_NULL(struct pc_inode);
		D_RW(iinode)->st.st_mode = (S_IFREG | 0755);
		D_RW(iinode)->st.st_nlink = 1;
		D_RW(iinode)->data = "Hello, World!\nThank you\n";
		D_RW(iinode)->st.st_size = strlen(D_RO(iinode)->data);


		D_RW(inode)->next = iinode;
		printf("hey - %s\n", D_RO(iinode)->name);

		TOID(struct pc_inode) iiinode = TX_NEW(struct pc_inode);
		D_RW(iiinode)->name = "file";
		D_RW(iiinode)->next = TOID_NULL(struct pc_inode);
		D_RW(iiinode)->st.st_mode = (S_IFREG | 0755);
		D_RW(iiinode)->st.st_nlink = 1;
		D_RW(iiinode)->data = "Why am I?\n";
		D_RW(iiinode)->st.st_size = strlen(D_RO(iiinode)->data);


		D_RW(iinode)->next = iiinode;
		printf("hey - %s\n", D_RO(iiinode)->name);
	} TX_END;

end:
	printf("end-init\n");
	return NULL;
}

static struct fuse_operations hello_oper = {
		.init		= pc_init,
		.getattr	= pc_getattr,
		.readdir	= pc_readdir,
		.open		= hello_open,
		.read		= pc_read,
		.write		= pc_write,
		//.mknod	= hello_mknod,
		//.unlink		= hello_unlink,
		//.utimens        = hello_utimens,
};

int main(int argc, char *argv[])
{
	umask(0);
	return fuse_main(argc, argv, &hello_oper, NULL);
}
