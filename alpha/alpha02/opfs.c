#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>

typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

// 文件类型
#define S_IFMT  (0170000)
#define S_IFLNK (0120000)
#define S_IFREG (0100000)
#define S_IFBLK (0060000)
#define S_IFDIR (0040000)
#define S_IFCHR (0020000)
#define S_IFIFO (0010000)

#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
// 用户权限
#define S_IRWXU (000700)
#define S_IRUSR (000400)
#define S_IWUSR (000200)
#define S_IXUSR (000100)

// root i-number
#define ROOTINO (1) 
// block size 
#define BSIZE   (1024)
// sector size
#define SSIZE   (512)
// Inodes per block.
#define IPB     (BSIZE / sizeof(struct dinode))
// Bitmap bits per block
#define BPB     (BSIZE * 8)
// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ  (14)

typedef uchar (*img_t)[BSIZE];

// super block
#define SBLK(img) ((struct superblock *)(img)[1])

struct partition {
  uchar   boot_ind;
  uchar   head;
  ushort  sect_cyl;  // uint8 sector; uint8 cyl;
  uchar   sys_ind;
  uchar   end_head;
  ushort  end_sect_cyl; // uint8 end_sector; uint8 end_cyl;
  uint    start_sect;
  uint    nr_sects;
};

struct superblock {
  ushort  s_ninodes;
  ushort  s_nzones;
  ushort  s_imap_block;
  ushort  s_zmap_block;
  ushort  s_firstdatazone;
  ushort  s_log_zine_size;
  uint    s_max_size;
  ushort  s_magic;
};

struct dinode {
  ushort  i_mode;
  ushort  i_uid;
  uint    i_size;
  uint    i_mtime;
  uchar   i_gid;
  uchar   i_nlinks;
  ushort  i_zone[9];
};

struct dirent {
  ushort  inum;
  char    name[DIRSIZ];
};

struct cmd_table_ent {
  char  *name;
  char  *args;
  int   (*fun)(img_t, int, char **);
};

#define ddebug(...) debug_message("DEBUG", __VA_ARGS__)
#define derror(...) debug_message("ERROR", __VA_ARGS__)
#define dwarn(...) debug_message("WARNING", __VA_ARGS__)

// inode
typedef struct dinode *inode_t;

int exec_cmd(img_t img, char *cmd, int argc, char *argv[]);
void rpart(char *hd_img, int id, struct partition *part);
void debug_message(const char *tag, const char *fmt, ...);
void error(const char *fmt, ...);
void fatal(const char *fmt, ...);
char *typename(ushort mode);
inode_t iget(img_t img, uint inum);
int valid_data_block(img_t img, uint b);
uint balloc(img_t img);
uint bmap(img_t img, inode_t ip, uint n);
int iread(img_t img, inode_t ip, uchar *buf, uint n, uint off);
int is_empty(char *s);
int is_sep(char c);
char *skipelem(char *path, char *name);
inode_t ilookup(img_t img, inode_t rp, char *path);
int do_diskinfo(img_t img, int argc, char *argv[]);
int do_get(img_t img, int argc, char *argv[]);

struct cmd_table_ent cmd_table[] = {
  { "diskinfo", "", do_diskinfo },
  { "get", "spath dpath", do_get },
  { NULL, NULL}
};

// inode of the root directory
const uint root_inode_number = 1;
inode_t root_inode;

// program name
char *progname;
jmp_buf fatal_exception_buf;


int exec_cmd(img_t img, char *cmd, int argc, char *argv[])
{
  for(int i = 0; cmd_table[i].name != NULL; i++) {
    if(strcmp(cmd, cmd_table[i].name) == 0)
      return cmd_table[i].fun(img, argc, argv);
  }
  error("unknown command: %s\n", cmd);
  return EXIT_FAILURE;
}

void rpart(char *hd_img, int id, struct partition *part) 
{
  struct partition *p;
  
  p = (struct partition *)&hd_img[0x1be];
  *part = *(p + id);
}

#undef min
static inline int min(int x, int y)
{
  return x < y ? x : y;
}

#undef max
static inline int max(int x, int y)
{
  return x > y ? x : y;
}

// ceiling(x / y) where x >=0, y >= 0
static inline int divceil(int x, int y)
{
  return x == 0 ? 0 : (x - 1) / y + 1;
}

uint bitcount(uint x)
{
  int n = 0;
  uchar m = 0x1;
  
  for(int i = 0; i < 8; i++) {
    if(!(x & m)) {
      n++;
    }
    m = m << 1;
  }
  
  return n;
}

void debug_message(const char *tag, const char *fmt, ...)
{
#ifndef NDEBUG
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "%s: ", tag);
  vfprintf(stderr, fmt, args);
  va_end(args);
#endif
}

void error(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}

void fatal(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "FATAL: ");
  vfprintf(stderr, fmt, args);
  va_end(args);
  longjmp(fatal_exception_buf, 1);
}

char *typename(ushort mode)
{
  if(S_ISREG(mode))  
    return "file";
  else if(S_ISLNK(mode))
    return "link";
  else if(S_ISDIR(mode))
    return "directory";
  else if(S_ISCHR(mode))
    return "char device";
  else if(S_ISBLK(mode))
    return "block device";
  else if(S_ISFIFO(mode))
    return "fifo";
  else 
    return "unknown";
}

inode_t iget(img_t img, uint inum)
{
  uint firstinode = SBLK(img)->s_imap_block + SBLK(img)->s_zmap_block + 2;
  uint ioff = inum - 1;
  
  if(0 < inum && inum < SBLK(img)->s_ninodes)
    return (inode_t)img[firstinode + ioff / IPB] + ioff % IPB;
  derror("iget: %u: invalid inode number\n", inum);
  return NULL;
}

int valid_data_block(img_t img, uint b)
{
  return b >= SBLK(img)->s_firstdatazone && b <= SBLK(img)->s_nzones - 1;
}

uint balloc(img_t img)
{
  uint firstimap = SBLK(img)->s_imap_block + 2;
  uint datasize = SBLK(img)->s_nzones - SBLK(img)->s_firstdatazone;
  uint blkno;
  
  for(int b = 0; b <= datasize; b += BPB) {
    uchar *bp = img[firstimap + b / BPB];
    for(int bi = 0; bi < BPB && b + bi <= datasize; bi++) {
      uchar m = 0x1 << (bi % 8);
      if((bp[bi / 8] & m) == 0) {
        bp[bi / 8] |= m;
        blkno = b + bi + firstimap - 1;
        if(!valid_data_block(img, blkno)) {
          fatal("balloc: %u: invalid data block number\n", blkno);
          return 0; // dummy
        }
        memset(img[blkno], 0, BSIZE);
        return blkno;
      }
    }
  }
  fatal("balloc: no free blocks\n");
  return 0; // dummy
}

uint bmap(img_t img, inode_t ip, uint n)
{
  uint k = n;
  
  if(n < 7) {
    uint addr = ip->i_zone[n];
    if(addr == 0) {
      addr = balloc(img);
      ip->i_zone[n] = addr;
    }
    return addr;
  }
  
  n -= 7;
  if(n < 512) {
    uint iaddr = ip->i_zone[7];
    if(iaddr == 0) {
      iaddr = balloc(img);
      ip->i_zone[7] = iaddr;
    }
    ushort *iblock = (ushort *)img[iaddr];
    if(iblock[n] == 0) {
      iblock[n] = balloc(img);
    }
    return iblock[n];
  }
  
  n -= 512;
  if(n < 512 * 512) {
    uint iaddr = ip->i_zone[8];
    if(iaddr == 0) {
      iaddr = balloc(img);
      ip->i_zone[8] = iaddr;
    }
    ushort *iblock = (ushort *)img[iaddr];
    uint iiaddr = iblock[n >> 9];
    if(iiaddr == 0) {
      iiaddr = balloc(img);
      iblock[n >> 9] = iiaddr;
    }
    ushort *iiblock = (ushort *)img[iiaddr];
    if(iiblock[n & 511] == 0) {
      iiblock[n & 511] = balloc(img);
    }
    return iiblock[n & 511];
  }
  
  derror("bmap: %u: invalid index number\n", k);
  return 0;
}

int iread(img_t img, inode_t ip, uchar *buf, uint n, uint off)
{
  if(S_ISLNK(ip->i_mode))
    return -1;
  if(S_ISCHR(ip->i_mode))
    return -1;
  if(S_ISBLK(ip->i_mode))
    return -1;
  if(S_ISFIFO(ip->i_mode))
    return -1;
  if(off > ip->i_size || off + n < off)
    return -1;
  if(off + n > ip->i_size)
    n = ip->i_size - off;
  
  // t : total bytes that have been read
  // m : last bytes that were read
  uint t = 0;
  for(uint m = 0; t < n; t += m, off += m, buf += m) {
    uint b = bmap(img, ip, off / BSIZE);
    if(!valid_data_block(img, b)) {
      derror("iread: %u: invalid data block\n", b);
      break;
    }
    m = min(n - t, BSIZE - off % BSIZE);
    memmove(buf, img[b] + off % BSIZE, m);
  }
  return t;
}

// check if s is an empty string
int is_empty(char *s)
{
  return *s == 0;
}

// check if c is a path separator
int is_sep(char c)
{
  return c == '/';
}

// adapted from skipelem in xv6/fs.c
/*
	str = "/root/test/bin"		   skipelem return = "/test/bin"     name = "root"
	str = "root/test/bin"        skipelem return = "/test/bin"     name = "root"
	str = "///root//test//bin"	 skipelem return = "//test//bin"   name = "root"
	str = ""                     skipelem return = ""              name = ""
	str = "///root//test//bin/"  skipelem return = "//test//bin/"  name = "root"
	str = "bin/"                 skipelem return = "/"             name = "bin"
	str = "/"                    skipelem return = ""              name = ""
*/
char *skipelem(char *path, char *name)
{
  while(is_sep(*path))
    path++;
  char *s = path;
  while(!is_empty(path) && !is_sep(*path))
    path++;
  int len = min(path - s, DIRSIZ);
  memmove(name, s, len);
  if(len < DIRSIZ)
    name[len] = 0;
  return path;
}

inode_t dlookup(img_t img, inode_t dp, char *name, uint *offp)
{
  assert(S_ISDIR(dp->i_mode));
  struct dirent de;
  
  for(uint off = 0; off < dp->i_size; off += sizeof(de)) {
    if(iread(img, dp, (uchar *)&de, sizeof(de), off) != sizeof(de)) {
      derror("dlookup: %s: read error\n", name);
      return NULL;
    }
    if(strncmp(name, de.name, DIRSIZ) == 0) {
      if (offp != NULL)
      *offp = off;
      return iget(img, de.inum);
    }
  }
  return NULL;
}

inode_t ilookup(img_t img, inode_t rp, char *path)
{
  char name[DIRSIZ + 1];
  name[DIRSIZ] = 0;
  while(1) {
    assert(path != NULL && rp != NULL && S_ISDIR(rp->i_mode));
    path = skipelem(path, name);
    // if path is empty (or a sequence of path separators),
    // it should specify the root direcotry (rp) itself
    if(is_empty(name))
      return rp;
    
    inode_t ip = dlookup(img, rp, name, NULL);
    if(ip == NULL)
      return NULL;
    if(is_empty(path))
      return ip;
    if(!(S_ISDIR(rp->i_mode))) {
      derror("ilookup: %s: not a directory\n", name);
      return NULL;
    }
    rp = ip;
  }
}

// diskinfo
int do_diskinfo(img_t img, int argc, char *argv[]) 
{
  if(argc != 0) {
    error("usage: %s img_file diskinfo\n", progname);
    return EXIT_FAILURE;
  }

  uint N = SBLK(img)->s_nzones;
  uint Nm = SBLK(img)->s_imap_block + SBLK(img)->s_zmap_block;
  uint Ni = SBLK(img)->s_firstdatazone - (2 + Nm);
  uint Nd = N - SBLK(img)->s_firstdatazone;

  printf(" total blocks: %d (%d bytes)\n", N, N * BSIZE);
  printf(" bitmap blocks: #%d-#%d (%d blocks)\n", 2, 2 + Nm - 1, Nm);
  printf(" inode blocks: #%d-#%d (%d blocks, %d inodes)\n", 
    2 + Nm, SBLK(img)->s_firstdatazone - 1 ,Ni ,SBLK(img)->s_ninodes);
  printf(" data blocks: #%d-#%d (%d blocks)\n", 
    SBLK(img)->s_firstdatazone, SBLK(img)->s_nzones - 1, Nd);
  printf(" maximum file size (bytes): %d\n", SBLK(img)->s_max_size);

  int nblocks = 0;
  for(uint b = SBLK(img)->s_imap_block + 2; b < 2 + Nm; b++)
    for (int i = 0; i < BSIZE; i++)
      nblocks += bitcount(img[b][i]);
  printf(" free blocks: %d\n", nblocks);

  int n_dirs = 0, n_files = 0, n_devs = 0, n_fifos = 0;
  for (uint b = 2 + Nm; b < SBLK(img)->s_firstdatazone; b++)
    for (int i = 0; i < IPB; i++) {
      if(S_ISREG(((inode_t)img[b])[i].i_mode))  
        n_files++;
      else if(S_ISLNK(((inode_t)img[b])[i].i_mode))
        n_files++;
      else if(S_ISDIR(((inode_t)img[b])[i].i_mode))
        n_dirs++;
      else if(S_ISCHR(((inode_t)img[b])[i].i_mode))
        n_devs++;
      else if(S_ISBLK(((inode_t)img[b])[i].i_mode))
        n_dirs++;
      else if(S_ISFIFO(((inode_t)img[b])[i].i_mode))
        n_fifos++;
      else 
        ;
    }
  printf(" # of used inodes: %d", n_dirs + n_files + n_devs + n_fifos);
  printf(" (dirs: %d, files: %d, devs: %d, fifos: %d)\n",
       n_dirs, n_files, n_devs, n_fifos);

  return EXIT_SUCCESS;
}

int do_get(img_t img, int argc, char *argv[]) {
  FILE *dfp;
  
  if(argc != 2) {
    error("usage: %s img_file get spath dpath\n", progname);
    return EXIT_FAILURE;
  }
  char *spath = argv[0];
  char *dpath = argv[1];

  dfp = fopen(dpath, "wb");
  if(dfp == NULL) {
    error("get: failed to create the file: %s\n", dpath);
    return EXIT_FAILURE;
  }
  
  // source
  inode_t ip = ilookup(img, root_inode, spath);
  if(ip == NULL) {
    error("get: no such file or directory: %s\n", spath);
    return EXIT_FAILURE;
  }
  
  uchar buf[BSIZE];
  for(uint off = 0; off < ip->i_size; off += BSIZE) {
    int n = iread(img, ip, buf, BSIZE, off);
    if (n < 0) {
      error("get: %s: read error\n", spath);
      return EXIT_FAILURE;
    }
    fwrite(buf, 1, n, dfp);
  }
  fclose(dfp);

  return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
  progname = argv[0];
  if (argc < 3) {
    error("usage: %s img_file command [arg...]\n", progname);
    error("Commands are:\n");
    for (int i = 0; cmd_table[i].name != NULL; i++)
      error("    %s %s\n", cmd_table[i].name, cmd_table[i].args);
    return EXIT_FAILURE;
  }
  char *img_file = argv[1];
  char *cmd = argv[2];
  
  HANDLE img_fd = CreateFile(img_file, 
                    GENERIC_READ | GENERIC_WRITE, 
                    FILE_SHARE_READ, 
                    NULL, 
                    OPEN_EXISTING, 
                    FILE_ATTRIBUTE_NORMAL, 
                    NULL);  
  if(img_fd == INVALID_HANDLE_VALUE) {
    perror(img_file);
    return EXIT_FAILURE;
  }
  
  HANDLE img_map = CreateFileMapping(img_fd, 
                    NULL, 
                    PAGE_READWRITE, 
                    0, 
                    0, 
                    NULL);
  if(img_map == NULL) {
    perror(img_file);
    CloseHandle(img_fd);
    return EXIT_FAILURE;
  }
  
  char *hd_img = MapViewOfFile(img_map, FILE_MAP_WRITE, 0, 0, 0);
  if(hd_img == NULL) {
    perror(img_file);
    CloseHandle(img_map);
    CloseHandle(img_fd);
    return EXIT_FAILURE;
  }
  
  struct partition part;
  rpart(hd_img, 0, &part);
  // printf("start_sect = %d\n", part.start_sect);
  img_t img = (img_t)(hd_img + (part.start_sect * SSIZE));
  
  root_inode = iget(img, root_inode_number);
  
  int status = EXIT_FAILURE;
  if(setjmp(fatal_exception_buf) == 0)
    status = exec_cmd(img, cmd, argc - 3, argv + 3);
  
  UnmapViewOfFile(hd_img);
  CloseHandle(img_map);
  CloseHandle(img_fd);
  
  return status;
}
