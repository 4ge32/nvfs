#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <libpmemobj.h>
#include <unistd.h>

#define MAX_BUF_LEN 10

POBJ_LAYOUT_BEGIN(cache);
POBJ_LAYOUT_ROOT(cache_store, struct cache_root);
POBJ_LAYOUT_END(cache);

char c_buf[4096];
static const char *filecontent = "You can see me? test file content\n";
static const char *filepath = "/file";

struct cache_root {
	char buf[MAX_BUF_LEN];
};

struct inode {
	char *path;
	char *name;
	char *content;

	struct stat st;
	struct inode *next;
	struct inode *child;
	struct inode *parent;
	struct inode *inode;
};

struct inode *root_inode;


static struct inode *search_inode(const char *path, struct inode *inode)
{
	char *search_path;

	search_path = (char *)malloc(sizeof(search_path));
	strcpy(search_path, path);

	search_path = strtok(search_path, "/");
	printf("inodeTYUMOKU!!:%s\n", path);
	printf("inodeTYUMOKU!!:%s\n", root_inode->child->name);

	while (inode != NULL) {
		if (!strcmp(inode->name, search_path)) {
			printf("inodeTYUMOKU!!:%s\n", inode->name);
			return inode;
		}
		inode = inode->next;
	}

	return NULL;
}

static int getattr_callback(const char *path, struct stat *stbuf) {
	printf("!!getatter_callback!!\n");

	struct inode *inode = root_inode->child;
	printf("TYUMOKU!!:%s\n", path);
	printf("TYUMOKU!!:%s\n", root_inode->child->name);

	memset(stbuf, 0, sizeof(struct stat));
	if (!strcmp(path, "/")) {
		stbuf->st_mode = root_inode->st.st_mode;
		stbuf->st_nlink = root_inode->st.st_nlink;
	} else {
		inode = search_inode(path, inode);
		if (inode != NULL) {
			stbuf->st_mode = inode->st.st_mode;
			stbuf->st_nlink = inode->st.st_nlink;
		} else
			return -ENOENT;
	}

	printf("!!getatter_callbakc!!\n");
	return 0;

}

static int readdir_callback(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi) {
	printf("!!readdir_callback!!\n");

	struct inode *inode = root_inode->child;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	while (inode != NULL) {
		filler(buf, inode->name, NULL, 0);
		inode = inode->next;
	}

	printf("!!readdir_callback!!\n");
	return 0;
}

static int open_callback(const char *path, struct fuse_file_info *fi)
{
	printf("OPEN_HELLO\n");
	return 0;
}

// Is offset strange?
static int read_callback(const char *path, char *buf, size_t size, off_t offset,
			 struct fuse_file_info *fi)
{

	printf("!!read_callback!!\n");
	struct inode *inode = root_inode->child;
	size_t len;

	inode = search_inode(path, inode);
	if (inode == NULL)
		return -ENOENT;
	printf("size:%lu\n", size);

	len = inode->st.st_size;
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, (void *)inode->content + offset, size);
	} else {
		size = 0;
	}

	printf("!!read_:%lu\n", offset);
	printf("%s", buf);
	printf("%s", inode->content + offset);
	printf("size:%lu\n", size);
	printf("!!read_callback!!\n");

	return size;

	if (strcmp(path, filepath) == 0) {
		printf("INNNN\n");
		printf("%s\n", filecontent);
		size_t len = strlen(filecontent);
		if (offset >= len) {
			return 0;
		}

		if (offset + size > len) {
			memcpy(buf, filecontent + offset, len - offset);
			return len - offset;
		}

		memcpy(buf, filecontent + offset, size);
		return size;
	}
	return -ENOENT;
}

static int create_callback(const char *path, mode_t mode,
			   struct fuse_file_info *fi)
{
	sync();
	return 0;
}

static int write_callback(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	printf("!!write!!\n");

	int fd;
	int res;

	(void) fi;
	if (fi == NULL)
		fd = open(path, O_WRONLY);
	else
		fd = fi->fh;

	if (fd == -1)
		return -errno;

	res = pwrite(fd, buf, size, offset);
	if (res == -1)
		res = -errno;

	if (fi == NULL)
		close(fd);

	return res;
}

/*
static int setxattr_callback(const char *path, const char *name, const char *value,
			     size_t size, int flags)
{
	int res = lsetxattr(path, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int getxattr_callback(const char *path, const char *name, char *value, size_t size)
{
	int res = lgetxattr(path, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}
*/

static int flush_buf(const char *buf, struct fuse_file_info *fi)
{
	return 0;
}

static void *init_file(struct fuse_conn_info *conn)
{
	struct inode *inode;
	inode = (struct inode *)malloc(sizeof(*inode));

	struct inode *node;
	node = (struct inode *)malloc(sizeof(*node));

	root_inode = (struct inode *)malloc(sizeof(*root_inode));
	root_inode->child = inode;
	root_inode->st.st_mode = (S_IFDIR | 0777);
	root_inode->st.st_nlink = 2;

	inode->st.st_mode = (S_IFREG | 0777);
	inode->st.st_nlink = 1;
	inode->parent = root_inode;
	inode->next = node;
	inode->name = "file";
	inode->content = "I don't understand C pointer\n";
	inode->st.st_size = strlen(inode->content);

	node->st.st_mode = (S_IFREG | 0777);
	node->st.st_nlink = 1;
	node->parent = root_inode;
	node->next = NULL;
	node->name = "test";
	node->content = "I have a solution\n";
	node->st.st_size = strlen(inode->content);


	return NULL;
}

static struct fuse_operations fuse_example_operations = {
	.getattr  = getattr_callback,
	.open     = open_callback,
	.read     = read_callback,
	.readdir  = readdir_callback,
	.create   = create_callback,
	.write    = write_callback,
	.flush    = flush_buf,
	.init     = init_file,
};

int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &fuse_example_operations, NULL);
}
