#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdarg.h>
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

struct dirent {
  ushort  inum;
  char    name[DIRSIZ];
};

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

struct cmd_table_ent {
  char  *name;
  char  *args;
  int   (*fun)(img_t, int, char **);
};

int exec_cmd(img_t img, char *cmd, int argc, char *argv[]);
void error(const char *fmt, ...);
void rpart(char *hd_img, int id, struct partition *part);
int do_diskinfo(img_t img, int argc, char *argv[]);

struct cmd_table_ent cmd_table[] = {
  { "diskinfo", "", do_diskinfo },
  { NULL, NULL}
};

char *progname;


int exec_cmd(img_t img, char *cmd, int argc, char *argv[])
{
  for(int i = 0; cmd_table[i].name != NULL; i++) {
    if(strcmp(cmd, cmd_table[i].name) == 0)
      return cmd_table[i].fun(img, argc, argv);
  }
  error("unknown command: %s\n", cmd);
  return EXIT_FAILURE;
}

void error(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
}

void rpart(char *hd_img, int id, struct partition *part) 
{
  struct partition *p;
  
  p = (struct partition *)&hd_img[0x1be];
  *part = *(p + id);
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
      if(S_ISREG(((struct dinode *)img[b])[i].i_mode))  
        n_files++;
      else if(S_ISLNK(((struct dinode *)img[b])[i].i_mode))
        n_files++;
      else if(S_ISDIR(((struct dinode *)img[b])[i].i_mode))
        n_dirs++;
      else if(S_ISCHR(((struct dinode *)img[b])[i].i_mode))
        n_devs++;
      else if(S_ISBLK(((struct dinode *)img[b])[i].i_mode))
        n_dirs++;
      else if(S_ISFIFO(((struct dinode *)img[b])[i].i_mode))
        n_fifos++;
      else 
        ;
    }
  printf(" # of used inodes: %d", n_dirs + n_files + n_devs + n_fifos);
  printf(" (dirs: %d, files: %d, devs: %d, fifos: %d)\n",
       n_dirs, n_files, n_devs, n_fifos);

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
  
  HANDLE img_map = CreateFileMapping(img_fd, NULL, PAGE_READWRITE, 0, 0, NULL);
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
  
  int status = EXIT_FAILURE;
  status = exec_cmd(img, cmd, argc - 3, argv + 3);
  
  UnmapViewOfFile(hd_img);
  CloseHandle(img_map);
  CloseHandle(img_fd);
  
  return status;
}
