#define FUSE_USE_VERSION 26
#define MAX_NAME_LEN 255

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <libpmemobj.h>

/*
struct inode {
		char 		*name;
		void		*data;
		struct stat st;

		struct	inode	*parent;
		struct	inode	*child;
		struct	inode	*next;
		struct	inode	*before;
};
*/

#define MAX_BUF_LEN 256

POBJ_LAYOUT_BEGIN(inode);
POBJ_LAYOUT_ROOT(r_inode, struct r_inode);
POBJ_LAYOUT_TOID(inode, struct pc_inode);
POBJ_LAYOUT_END(inode);

struct r_inode {
	TOID(struct pc_inode) head;
};

struct pc_inode {
	char 	*name;
	char	*data;
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
static struct c_inode *rcnode;

//static struct inode* root_inode;

/* find node in path */

//static struct inode* find_node(struct inode* parent, const char* name){
//	struct inode *node = parent->child;
//	while(node != NULL && strcmp(node->name, name)) {
//		node = node->next;
//	}
//	return nodenit
//	/;
//}

/*
 * will also add  . and ..
 */
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

//static int hello_read(const char *path, char *buf, size_t size, off_t offset,
//		      struct fuse_file_info *fi)
//{
//	size_t len;
//	(void) fi;
//	struct inode* node;
//
//	printf("hello_read\n");
//	printf("offset:%lu\n", offset);
//	printf("size:%lu\n", size);
//
//	node = find_path(path);
//	if(node == NULL)
//		return -ENOENT;
//	len = node->st.st_size;
//	if (offset < len)
//	{
//		if (offset + size > len)
//			size = len - offset;
//		memcpy(buf, node->data + offset, size);
//	}
//	else
//		size = 0;
//	printf("content:%s\n", buf);
//	printf("node->data+offset:%s\n", (char *)node->data+offset);
//	printf("size:%lu\n", size);
//	printf("hello_read end");
//  	return size;
//	return pc_read(path, buf, size, offset, fi);
//}
//


static int pc_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	TOID(struct pc_inode) inode = POBJ_ROOT(pop, struct pc_inode);

	inode = search_inode(path);
	if (TOID_IS_NULL(inode))
		return -ENOENT;

	TX_BEGIN(pop) {
	} TX_END
	return 1;
}

//static int hello_write(const char *path, const char *buf, size_t size,
//	 		 off_t offset, struct fuse_file_info *fi)
//{
//	 struct inode* node;
//	 void* tmp;
//	 (void) fi;
//
//	 printf("hello_write");
//
//	 node = find_path(path);
//
//	 if(node == NULL)
//	 	return -ENOENT;
//
//	 if (size + offset > node->st.st_size)
//	 {
//	 	tmp = malloc(node->st.st_size + size);
//	 	if (tmp) {
//	 		if (node->data) {
//	 			memcpy(tmp, node->data, node->st.st_size);
//	 			free(node->data);
//	 		}
//	 		node->data = tmp;
//	 		node->st.st_size += size;
//	 	}
//	}
//	memcpy(node->data + offset, buf, size);
//	printf("hello_write end");
//	return size;
//	return pc_write(path, buf, size, offset, fi);
//}

//static int hello_mknod(const char *path, mode_t mode, dev_t dev)
//{
// 		time_t	current_time = time(NULL);
//		struct inode*	parent;
//		struct inode* 	node;
//		struct inode*	last_node;
//
//		char*	parent_path = find_parent(path);
//		int len = strlen(parent_path);
//		int i;
//
//		printf("hello_mknod");
//
//		parent = find_path(parent_path);
//
//		if (parent == NULL)
//			return -ENOENT;
//		if ( find_path(path) != NULL )
//			return -EEXIST;
//
//		node = (struct inode*)calloc(1, sizeof(struct inode));
//		node->parent = parent;
//		node->st.st_nlink = 1;
//		node->st.st_mode = mode;
//		node->st.st_mtime = current_time;
//		node->st.st_ctime = current_time;
//		node->st.st_atime = current_time;
//		node->child = NULL;
//		node->next = NULL;
//
//		node->name = (char*)calloc(MAX_NAME_LEN, sizeof(char));
//
//		for( i = len ; path[i] != '\0' ; i++ )
//			node->name[i-len] = path[i];
//		node->name[i-len] = '\0';
//
//		if(parent->child == NULL){
//			parent->child = node;
//			node->before = NULL;
//		}
//		else {
//			for(last_node = parent->child; last_node->next != NULL;
//				last_node = last_node->next);
//			last_node->next = node;
//			node->before = last_node;
//		}
//	printf("hello_mknod end");
//	return 0;
//}

//static int hello_unlink(const char *path)
//{
//	struct inode* node;
//	node = find_path(path);
//
// 	printf("hello_rmdir");
//
//	if (node == root_inode ||
//		((node->st.st_mode & S_IFDIR) && ( node->child )) )
//		return -EISDIR;
//
//	if(node == node->parent->child){
//		if(node->next)
//			node->next->before = NULL;
//		node->parent->child = node->next;
//		if(node->data)
//			free(node->data);
//		free(node->name);
//		free(node);
//	}
//	else {
//		node->before->next = node->next;
//		if(node->next)
//			node->next->before = node->before;
//		if(node->data)
//			free(node->data);
//		free(node->name);
//		free(node);
//	}
//	return 0;
//}

//static int hello_utimens(const char *path, const struct timespec ts[2])
//{
//	struct inode* node = find_path(path);
//	time_t current_time = time(NULL);
//
//	printf("hello_utimens");
//
//	if(node == NULL)
//		return -ENOENT;
//	node->st.st_mtime = current_time;
//	node->st.st_atime = current_time;
//
//	printf("hello_utimens end");
//	return 0;
//}
//

static void *pc_init(struct fuse_conn_info *conn) {
	printf("start-init\n");


	pop = pmemobj_create(pmem_pool, POBJ_LAYOUT_NAME(inode), PMEMOBJ_MIN_POOL, 0666);
	if (pop == NULL) {
		perror("pmemobj_create");
		return NULL;
	}

	root = POBJ_ROOT(pop, struct r_inode);

	rcnode = (struct c_inode *)malloc(sizeof(rcnode));
	struct c_inode *cnode = (struct c_inode *)calloc(1, sizeof(struct c_inode));
	struct c_inode *ccnode = (struct c_inode *)calloc(1, sizeof(struct c_inode));

	TX_BEGIN(pop) {
		TX_ADD(root);
		TOID(struct pc_inode) inode = TX_NEW(struct pc_inode);
		D_RW(inode)->name = "";
		D_RW(inode)->next = TOID_NULL(struct pc_inode);
		D_RW(inode)->st.st_mode = (S_IFDIR | 0755);
		D_RW(inode)->st.st_nlink = 2;

		rcnode->name = calloc(1, sizeof(rcnode->name));
		strcpy(rcnode->name, "");
		rcnode->next = NULL;

		D_RW(root)->head = inode;
		printf("hey - %s\n", D_RO(inode)->name);

		TOID(struct pc_inode) iinode = TX_NEW(struct pc_inode);
		D_RW(iinode)->name = "test";
		D_RW(iinode)->next = TOID_NULL(struct pc_inode);
		D_RW(iinode)->st.st_mode = (S_IFREG | 0755);
		D_RW(iinode)->st.st_nlink = 1;
		D_RW(iinode)->data = "Hello, World!\n";
		D_RW(iinode)->st.st_size = strlen(D_RO(iinode)->data);

		cnode->name = calloc(1, sizeof(cnode->name));
		strcpy(cnode->name, "test");
		cnode->next = NULL;
		rcnode->next = cnode;

		D_RW(inode)->next = iinode;
		printf("hey - %s\n", D_RO(iinode)->name);

		TOID(struct pc_inode) iiinode = TX_NEW(struct pc_inode);
		D_RW(iiinode)->name = "file";
		D_RW(iiinode)->next = TOID_NULL(struct pc_inode);
		D_RW(iiinode)->st.st_mode = (S_IFREG | 0755);
		D_RW(iiinode)->st.st_nlink = 1;
		D_RW(iiinode)->data = "Why am I?\n";
		D_RW(iiinode)->st.st_size = strlen(D_RO(iiinode)->data);

		rcnode->name = calloc(1, sizeof(rcnode));
		strcpy(rcnode->name, "file");

		ccnode->name = calloc(1, sizeof(ccnode));
		strcpy(ccnode->name, "file");
		ccnode->next = NULL;
		cnode->next = ccnode;

		D_RW(iinode)->next = iiinode;
		printf("hey - %s\n", D_RO(iiinode)->name);
	} TX_END;

	printf("end-init\n");
	return NULL;
}

/* fuse operation */
//static void *hello_init(struct fuse_conn_info *conn)
//{
//	(void) conn;
//	time_t current_time = time(NULL);
//
//	printf("Hello_init");
//
//	root_inode = (struct inode *) calloc(1, sizeof(struct inode));
//	root_inode->parent = NULL;
//	root_inode->child = NULL;
//	root_inode->next = NULL;
//	root_inode->before = NULL;
//
//	root_inode->st.st_mode = (S_IFDIR | 0755);
//	root_inode->st.st_nlink = 2;
//	root_inode->st.st_mtime = current_time;
//	root_inode->st.st_ctime = current_time;
//	root_inode->st.st_atime = current_time;
//	root_inode->name = "";
//	printf("Hello_init end");
//
//	return NULL;
//	pc_init(conn);
//}


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
