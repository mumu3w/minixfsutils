#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef unsigned char       uint8;
typedef unsigned short      uint16;
typedef unsigned int        uint32;

struct partition
{
    uint8   boot_ind;
    uint8   head;
 // uint8   sector;
 // uint8   cyl;
    uint16  sect_cyl;
    uint8   sys_ind;
    uint8   end_head;
 // uint8   end_sector;
 // uint8   end_cyl;
    uint16  end_sect_cyl;
    uint32  start_sect;
    uint32  nr_sects;
};

struct super_block
{
    uint16  s_ninodes;
    uint16  s_nzones;
    uint16  s_imap_block;
    uint16  s_zmap_block;
    uint16  s_firstdatazone;
    uint16  s_log_zine_size;
    uint32  s_max_size;
    uint16  s_magic;
};

struct d_inode
{
    uint16  i_mode;
    uint16  i_uid;
    uint32  i_size;
    uint32  i_time;
    uint8   i_gid;
    uint8   i_nlinks;
    uint16  i_zone[9];
};

#define ROOT_INO		(1)

#define UPPER(size, n)  ((size+((n)-1))/(n))
#define BLOCK_SIZE      (1024)
#define SECTOR_SIZE     (512)

#define INODES_PER_BLOCK \
			((BLOCK_SIZE)/(sizeof(struct d_inode)))
#define INODE_SIZE		(sizeof(struct d_inode))
#define INODE_BLOCKS	UPPER(INODES, INODES_PER_BLOCK)
#define INODE_BUFFER_SIZE \
			(INODE_BLOCKS * BLOCK_SIZE)
			
#define BITS_PER_BLOCK	(BLOCK_SIZE << 3)

#define INODES			(Super->s_ninodes)
#define ZONES			(Super->s_nzones)
#define IMAPS			(Super->s_imap_block)
#define ZMAPS			(Super->s_zmap_block)
#define FIRSTZONE		(Super->s_firstdatazone)
#define ZONESIZE		(Super->s_log_zine_size)
#define MAXSIZE			(Super->s_max_size)
#define MAGIC			(Super->s_magic)

#define NORM_FIRSTZONE	(2+IMAPS+ZMAPS+INODE_BLOCKS)

char super_block_buffer[BLOCK_SIZE];
char inode_map[BLOCK_SIZE * 8];
char zone_map[BLOCK_SIZE * 8];

void cls_bit(char *map, int x)
{
	uint8 m = 0;
	
	m = 0x01 << (x%8);
	map[x/8] &= ~m;
}

static uint32 get_seconds(void)
{
    time_t seconds;
    time(&seconds);
    return seconds;
}

void mkfs(int start_sect, int nr_sects)
{
	int i;
	FILE *img_fp;
	struct super_block *Super = (struct super_block *)super_block_buffer;
	char *inode_buffer;
	struct d_inode *Inode;
	char block_buf[BLOCK_SIZE];
	
	memset(inode_map, 0xff, sizeof(inode_map));
	memset(zone_map, 0xff, sizeof(zone_map));
	memset(super_block_buffer, 0, BLOCK_SIZE);
	
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
	IMAPS = UPPER(INODES, BITS_PER_BLOCK);
	ZMAPS = 0;
	while(ZMAPS != UPPER(ZONES - NORM_FIRSTZONE, BITS_PER_BLOCK))
	{
		ZMAPS = UPPER(ZONES - NORM_FIRSTZONE, BITS_PER_BLOCK);
	}
	FIRSTZONE = NORM_FIRSTZONE;
	
	for(i = FIRSTZONE + 1; i < ZONES; i++)
	{
		cls_bit(zone_map, i - FIRSTZONE + 1);
	}
	for(i = ROOT_INO + 1; i < INODES; i++)
	{
		cls_bit(inode_map, i);
	}
	
	inode_buffer = malloc(INODE_BUFFER_SIZE);
	memset(inode_buffer, 0, INODE_BUFFER_SIZE);
	Inode = (struct d_inode *)inode_buffer;
	Inode->i_zone[0] = FIRSTZONE;
	Inode->i_nlinks = 2;
	Inode->i_time = get_seconds();
	Inode->i_size = 32;
	Inode->i_mode = 0040000 + 0755;
	
	memset(block_buf, 0, BLOCK_SIZE);
	memcpy(block_buf, "\01\0.\0\0\0\0\0\0\0\0\0\0\0\0\0", 16);
	memcpy(block_buf+16, "\01\0..\0\0\0\0\0\0\0\0\0\0\0\0", 16);
	
	printf("%d inodes\n", INODES);
	printf("%d blocks\n", ZONES);
	printf("Firstdatazone=%d (%d)\n", FIRSTZONE, NORM_FIRSTZONE);
	printf("Zonesize=%d\n", BLOCK_SIZE << ZONESIZE);
	printf("Maxsize=%d \n\n", MAXSIZE);
	
	img_fp = fopen("rootfs.img", "rb+");
	fseek(img_fp, start_sect * 512 + 1 * BLOCK_SIZE, SEEK_SET);
	fwrite(super_block_buffer, 1, BLOCK_SIZE, img_fp);
	
	fseek(img_fp, start_sect * 512 + 2 * BLOCK_SIZE, SEEK_SET);
	fwrite(inode_map, 1, IMAPS * BLOCK_SIZE, img_fp);
	
	fseek(img_fp, start_sect * 512 + 2 * BLOCK_SIZE + IMAPS * BLOCK_SIZE, SEEK_SET);
	fwrite(zone_map, 1, ZMAPS * BLOCK_SIZE, img_fp);
	
	fseek(img_fp, start_sect * 512 + 2 * BLOCK_SIZE + IMAPS * BLOCK_SIZE + ZMAPS * BLOCK_SIZE, SEEK_SET);
	fwrite(inode_buffer, 1, INODE_BUFFER_SIZE, img_fp);
	
	fseek(img_fp, start_sect * 512 + FIRSTZONE * BLOCK_SIZE, SEEK_SET);
	fwrite(block_buf, 1, BLOCK_SIZE, img_fp);
	
	fclose(img_fp);
}

int main(void)
{
    mkfs(2048, 18000);
    return 0;
}