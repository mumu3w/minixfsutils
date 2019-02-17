#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

// 文件类型
#define S_IFMT	(0170000)
#define S_IFREG	(0100000)
#define S_IFBLK	(0060000)
#define S_IFDIR	(0040000)
#define S_IFCHR	(0020000)
#define S_IFIFO	(0010000)
// 用户权限
#define S_IRWXU	(000700)
#define S_IRUSR	(000400)
#define S_IWUSR	(000200)
#define S_IXUSR	(000100)

// root i-number
#define ROOTINO (1)	
// block size	
#define BSIZE	(1024)
// sector size
#define SSIZE	(512)
// Inodes per block.
#define IPB		(BSIZE / sizeof(struct dinode))
// Bitmap bits per block
#define BPB		(BSIZE * 8)
// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 	(14)

struct dirent {
	ushort	inum;
	char	name[DIRSIZ];
};

struct superblock {
	ushort	s_ninodes;
	ushort	s_nzones;
	ushort	s_imap_block;
	ushort	s_zmap_block;
	ushort	s_firstdatazone;
	ushort	s_log_zine_size;
	uint	s_max_size;
	ushort	s_magic;
};

struct dinode {
	ushort	i_mode;
	ushort	i_uid;
	uint	i_size;
	uint	i_mtime;
	uchar	i_gid;
	uchar	i_nlinks;
	ushort	i_zone[9];
};


FILE *fsfd;
uint start_sec;// 分区的开始扇区
uint freeinode = 1;
uint freeblock;
struct superblock sb;
ushort firstimapzone;
ushort firstzmapzone;
ushort firstinodezone;


void cls_bit(char *map, int x);
void make_sb(uint nr_sects);
void wblock(uint blkno, void *buf);
void winode(uint inum, struct dinode *ip);
void rinode(uint inum, struct dinode *ip);
void rblock(uint blkno, void *buf);
uint ialloc(ushort type);
void balloc(int used);
void iappend(uint inum, void *xp, int n);

#define UPPER(size, n)  	((size+((n)-1))/(n))

#define INODE_SIZE			(sizeof(struct dinode))
#define INODE_BLOCKS		UPPER(INODES, IPB)
#define INODE_BUFFER_SIZE 	(INODE_BLOCKS * BSIZE)

#define INODES			(Super->s_ninodes)
#define ZONES			(Super->s_nzones)
#define IMAPS			(Super->s_imap_block)
#define ZMAPS			(Super->s_zmap_block)
#define FIRSTZONE		(Super->s_firstdatazone)
#define ZONESIZE		(Super->s_log_zine_size)
#define MAXSIZE			(Super->s_max_size)
#define MAGIC			(Super->s_magic)

#define NORM_FIRSTZONE	(2+IMAPS+ZMAPS+INODE_BLOCKS)
char sb_buf[BSIZE];
char inode_map[BSIZE * 8];
char zone_map[BSIZE * 8];

int 
main(int argc, char *argv[])
{
	int i, cc;
	FILE *fd;
	uint rootino, inum, off;
	struct dirent de;
	char buf[BSIZE];
	struct dinode din;
	
	fsfd = fopen("rootfs.img", "rb+");
	
	start_sec = 2048;
	make_sb(18000);
	
	rootino = ialloc(S_IFDIR);
	
	memset(&de, 0, sizeof(struct dirent));
	de.inum = rootino;
	strcpy(de.name, ".");
	iappend(rootino, &de, sizeof(struct dirent));
	
	memset(&de, 0, sizeof(struct dirent));
	de.inum = rootino;
	strcpy(de.name, "..");
	iappend(rootino, &de, sizeof(struct dirent));
	
//	printf("1....\n");
#if 1	
	for(i = 1; i < argc; i++) {
		char *name = argv[i];
		
		if((fd = fopen(argv[i], "rb+")) == NULL) {
			perror(argv[i]);
			exit(1);
		}
		
		inum = ialloc(S_IFREG);
		
		memset(&de, 0, sizeof(struct dirent));
		de.inum = inum;
		strncpy(de.name, name, DIRSIZ);
		iappend(rootino, &de, sizeof(struct dirent));
		
		while((cc = fread(buf, 1, BSIZE, fd)) > 0) {
			iappend(inum, buf, cc);
		}
		
		fclose(fd);
		rinode(rootino, &din);
		din.i_nlinks++;
		winode(rootino, &din);
	}
#endif	
	balloc(freeblock - sb.s_firstdatazone + 1);
	
	fclose(fsfd);
	exit(0);
}

void 
cls_bit(char *map, int x)
{
	uchar m = 0;
	
	m = 0x01 << (x%8);
	map[x/8] &= ~m;
}

void
make_sb(uint nr_sects)
{
	int i;
	struct superblock *Super = (struct superblock *)sb_buf;
	
	memset(inode_map, 0xff, sizeof(inode_map));
	memset(zone_map, 0xff, sizeof(zone_map));
	memset(sb_buf, 0, BSIZE);
	
	MAGIC = 0x137f;
	ZONESIZE = 0;
	MAXSIZE = (7 + 512 + 512 * 512) * 1024;
	ZONES = nr_sects / 2;
	INODES = ZONES / 3;
	if((INODES & 8191) > 8188)
	{
		INODES -= 5;
	}
	if((INODES & 8191) < 10)
	{
		INODES -= 20;
	}
	IMAPS = UPPER(INODES, BPB);
	ZMAPS = 0;
	while(ZMAPS != UPPER(ZONES - NORM_FIRSTZONE, BPB))
	{
		ZMAPS = UPPER(ZONES - NORM_FIRSTZONE, BPB);
	}
	FIRSTZONE = NORM_FIRSTZONE;
	
	for(i = FIRSTZONE; i < ZONES; i++)
	{
		cls_bit(zone_map, i - FIRSTZONE + 1);
	}
	for(i = ROOTINO; i < INODES; i++)
	{
		cls_bit(inode_map, i);
	}
	
	sb = *Super;
	freeblock = FIRSTZONE;
	firstimapzone = 2;
	firstzmapzone = firstimapzone + IMAPS;
	firstinodezone = firstzmapzone + ZMAPS;
	
	printf("%d inodes\n", INODES);
	printf("%d blocks\n", ZONES);
	printf("Firstdatazone=%d (%d)\n", FIRSTZONE, NORM_FIRSTZONE);
	printf("Zonesize=%d\n", BSIZE << ZONESIZE);
	printf("Maxsize=%d \n\n", MAXSIZE);
	
	wblock(1, sb_buf);
	wblock(firstimapzone, inode_map);
	wblock(firstzmapzone, zone_map);
}

void 
wblock(uint blkno, void *buf)
{
	if(fseek(fsfd, start_sec * SSIZE + blkno * BSIZE, SEEK_SET) != 0) {
		perror("fseek");
		exit(1);
	}
	if(fwrite(buf, 1, BSIZE, fsfd) != BSIZE) {
		perror("fwrite");
		exit(1);
	}
//	printf("wblock SEEK = %d\n", start_sec * SSIZE + blkno * BSIZE);
}

void
winode(uint inum, struct dinode *ip)
{
	char buf[BSIZE];
	uint bn;
	struct dinode *dip;
	
	bn = (inum - 1) / IPB;
	rblock(firstinodezone + bn, buf);
	//off = ((inum - 1) % IPB) * sizeof(struct dinode);
	dip = ((struct dinode*)buf) + ((inum - 1) % IPB);
	*dip = *ip;
	wblock(firstinodezone + bn, buf);
}

void
rinode(uint inum, struct dinode *ip)
{
	char buf[BSIZE];
	uint bn;
	struct dinode *dip;
	
	bn = (inum - 1) / IPB;
	rblock(firstinodezone + bn, buf);
	//off = ((inum - 1) % IPB) * sizeof(struct dinode);
	dip = ((struct dinode*)buf) + ((inum - 1) % IPB);
	*ip = *dip;
}

void
rblock(uint blkno, void *buf)
{
	if(fseek(fsfd, start_sec * SSIZE + blkno * BSIZE, SEEK_SET) != 0) {
		perror("fseek");
		exit(1);
	}
	if(fread(buf, 1, BSIZE, fsfd) != BSIZE) {
		perror("fwrite");
		exit(1);
	}
//	printf("rblock SEEK = %d\n", start_sec * SSIZE + blkno * BSIZE);
}

uint
ialloc(ushort type)
{
	char buf[BSIZE];
	uint bi, b, inum;
	uchar m;
	struct dinode din;
#if 0	
	for(b = 1; b < sb.s_ninodes; b += BPB) {
		rblock(firstimapzone + b / BPB, buf);
		for(bi = 0; bi < BPB && b + bi < sb.s_ninodes; bi++) {
			m = 0x1 << (bi % 8);
			if(buf[bi / 8] & m) == 0) {
				inum = b + bi;
				buf[bi / 8] |= m;// 置位
				wblock(firstimapzone + b / BPB, buf);
				
				memset(&din, 0, sizeof(struct dinode));
				din.i_mode = type | S_IRWXU;
				din.i_uid = 0;
				din.i_size = 0;
				din.i_mtime = 0;
				din.i_gid = 0;
				din.i_nlinks = 1;
				winode(inum, &din);
				
				return inum;// 返回找到的空闲i节点
			}
		}
	}
	
	return 0;// 找不到可用的i节点
#else
	inum = freeinode++;
	rblock(firstimapzone + inum / BPB, buf);
	buf[inum / 8] |= (0x1 << (inum % 8));
	wblock(firstimapzone + inum / BPB, buf);
	
	memset(&din, 0, sizeof(struct dinode));
	din.i_mode = type | S_IRWXU;
	din.i_uid = 0;
	din.i_size = 0;
	din.i_mtime = 0;
	din.i_gid = 0;
	din.i_nlinks = 1;
	winode(inum, &din);
	
	return inum;// 返回找到的空闲i节点
#endif
}

void
balloc(int used)
{
	char buf[BSIZE];
	uint bi, b;
	uchar m;

	for(b = 0; b < used; b += BPB) {
		rblock(firstzmapzone + b / BPB, buf);
		for(bi = 0; bi < BPB && b + bi < used; bi++) {
			buf[bi / 8] |= (0x1 << (bi % 8));// 置位
//			printf("%d   %d\n", bi / 8, bi % 8);
		}
		wblock(firstzmapzone + b / BPB, buf);
	}
}

#define min(a, b)	((a) < (b) ? (a) : (b))

void
iappend(uint inum, void *xp, int n)
{
	char *p = (char *)xp;
	uint fbn, fbnt, off, nl;
	struct dinode din;
	char buf[BSIZE];
	ushort buf_ind[BSIZE / sizeof(ushort)];
	ushort buf_dind[BSIZE / sizeof(ushort)];
	uint x;
	
	rinode(inum, &din);
	
	off = din.i_size;
	while(n > 0) {
		fbn = off / BSIZE;
		fbnt = fbn;
		if(fbn < 7) {
			if(din.i_zone[fbn] == 0) {
				din.i_zone[fbn] = freeblock++;
			}
			x = din.i_zone[fbn];
		}else if((fbn - 7) < 512) {
			fbn -= 7;
			if(din.i_zone[7] == 0) {
				din.i_zone[7] = freeblock++;
			}
			rblock(din.i_zone[7], (char *)buf_ind);
			if(buf_ind[fbn] == 0) {
				buf_ind[fbn] = freeblock++;
				wblock(din.i_zone[7], (char *)buf_ind);
			}
			x = buf_ind[fbn];
		}else {
			fbn -= (7 + 512);
			if(din.i_zone[8] == 0) {
				din.i_zone[8] = freeblock++;
			}
			rblock(din.i_zone[8], (char *)buf_ind);
			if(buf_ind[fbn >> 9] == 0) {
				buf_ind[fbn >> 9] = freeblock++;
				wblock(din.i_zone[8], (char *)buf_ind);
			}
			rblock(buf_ind[fbn >> 9], (char *)buf_dind);
			if(buf_dind[fbn & 511] == 0) {
				buf_dind[fbn & 511] = freeblock++;
				wblock(buf_ind[fbn >> 9], (char *)buf_dind);
			}
			x = buf_dind[fbn & 511];
		}
		nl = min(n, (fbnt + 1) * BSIZE - off);
		rblock(x, buf);
//		printf(" ...  0\n");
		memcpy(buf + off - (fbnt * BSIZE), p, nl);
//		printf(" ...  1\n");
		wblock(x, buf);
//		printf("x = %d\n", x);
		n -= nl;
		off += nl;
		p += nl;
	}
	din.i_size = off;
	winode(inum, &din);
}	
