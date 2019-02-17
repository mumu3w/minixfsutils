#define NDIRECT 7
int itruncate(img_t img, inode_t ip, uint size)
{
  if(S_ISLNK(ip->i_mode))
    return -1;
  if(S_ISCHR(ip->i_mode))
    return -1;
  if(S_ISBLK(ip->i_mode))
    return -1;
  if(S_ISFIFO(ip->i_mode))
    return -1;
  if(size > SBLK(img)->s_max_size)
    return -1;
  
  if(size < ip->i_size) {
    int n = divceil(ip->i_size, BSIZE);  // # of used blocks
    int k = divceil(size, BSIZE);      // # of blocks to keep
  
    int nd = min(n, NDIRECT);          // # of used direct blocks
    int kd = min(k, NDIRECT);          // # of direct blocks to keep
    for(int i = kd; i < nd; i++) {
      bfree(img, ip->i_zone[i]);
      ip->i_zone[i] = 0;
    }
    
    if(n > NDIRECT) {
      uint iaddr = ip->i_zone[NDIRECT];
      assert(iaddr != 0);
      ushort *iblock = (ushort *)img[iaddr];
      int ni = min(n - NDIRECT, 512);
      int ki = min(k - NDIRECT, 512);
      for(uint i = ki; i < ni; i++) {
        bfree(img, iblock[i]);
        iblock[i] = 0;
      }
      if(ki == 0) {
        bfree(img, iaddr);
        ip->i_zone[NDIRECT] = 0;
      }
    }
    
    if(n > 512) {
      uint iaddr = ip->i_zone[8];
      assert(iaddr != 0);
      ushort *iblock = (ushort *)img[iaddr];
      int ni = min(n >> 9, 512);
      int ki = min(k >> 9, 512);
      for(uint i = ki; i < ni; i++) {
        uint iiaddr = iblock[i];
        assert(iiaddr != 0);
        ushort *iiblock = (ushort *)img[iiaddr];
        int nii = max((n - NDIRECT - 512) & 511, 0);
        int kii = max((k - NDIRECT - 512) & 511, 0);
        for(uint j = kii; j < nii; j++) {
          bfree(img, iiblock[j]);
          iiblock[j] = 0;
        }
        if(kii == 0) {
          bfree(img, iiaddr);
          iiblock[j] = 0;
        }
      }
      if(ki == 0) {
        bfree(img, iaddr);
        ip->i_zone[8] = 0;
      }
    }
  }else {
    int n = size - ip->i_size; // # of bytes to be filled
    for(uint off = ip->i_size, t = 0, m = 0; t < n; t += m, off += m) {
      uchar *bp = img[bmap(img, ip, off / BSIZE)];
      m = min(n - t, BSIZE - off % BSIZE);
      memset(bp + off % BSIZE, 0, m);
    }
  }
  ip->i_size = size;
  return 0;
}
