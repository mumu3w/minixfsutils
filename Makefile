RM	= -del /S
CC	= gcc -std=c99 -g 

all : minixfsutils fdisk mkfs

minixfsutils : minixfsutils.c
	$(CC) -o minixfsutils minixfsutils.c
	
fdisk : fdisk.c
	$(CC) -o fdisk fdisk.c
	
mkfs : mkfs.c
	$(CC) -o mkfs mkfs.c
	
clean :
	@$(RM) minixfsutils.exe
	@$(RM) fdisk.exe
	@$(RM) mkfs.exe
	