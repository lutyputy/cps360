#include "types.h"
#include "defs.h"
#include "spinlock.h"
#include "fs.h"
#include "vfs.h"
#include "file.h"
#include "stat.h"
#include "vfs_ramfs.h"

struct vfs_ops ramfs_vfs_ops = {
  .stati = ramfs_stati,
  .ilock = ramfs_ilock,
  .iunlock = ramfs_iunlock,
  .iput = ramfs_iput,
  .writei = ramfs_writei,
  .readi = ramfs_readi
};

void raminit(void)
{
  initlock(drive.lock, "ram");
}

struct vfile* ramfs_create(char *path, short type, short major, short minor)
{
  for(int i = 0; i < MAX_RAMFILES; i++)
  {
    if(drive.ramFiles[i].alloc == 0 || namecmp(drive.ramFiles[i].fName, path) == 0)
    {
      memmove(drive.ramFiles[i].fName, path, 14);
      drive.ramFiles[i].alloc = 1;
      drive.ramFiles[i].dataBlocks[0].data = kalloc();
      memset(drive.ramFiles[i].dataBlocks[0].data, 0, 4096);
      drive.ramFiles[i].dataBlocks[0].alloc = 1;
      struct vfile *vf = vfile_alloc(&drive.ramFiles[i], &ramfs_vfs_ops);
      vf->type = 2;
      return vf;
    }
  }
  return vfile_alloc(NULL, &ramfs_vfs_ops);
}

void ramfs_stati(struct vfile *vfile, struct stat *st)
{
  st->type = 2;
  st->dev = 0;
  st->ino = 0;
  st->nlink = 0;
  int size = 0;
  struct ram* file = (struct ram*)(vfile->fsp);
  for(int i = 0; i < 16; i++)
  {
    if(file->dataBlocks[i].alloc != 0)
    {
      size += strlen(file->dataBlocks[i].data);
    }
  }
  st->size = size;
}

void ramfs_ilock(struct vfile* vfile)
{
  // DON'T NEED TO DO
}

void ramfs_iunlock(struct vfile* vfile)
{
  // DON'T NEED TO DO
}

void ramfs_iput(struct vfile* vfile) 
{
  // DON'T NEED TO DO
}

int ramfs_writei(struct vfile* vfile, char *src, uint off, uint n)
{
 struct ram* writeTo = (struct ram*)vfile->fsp;
 if(writeTo->dataBlocks[off / 4096].alloc != 1)
 {
  writeTo->dataBlocks[off / 4096].data = kalloc();
  memset(writeTo->dataBlocks[off / 4096].data, 0, 4096);
  writeTo->dataBlocks[off / 4096].alloc = 1;
 }
  memmove(&writeTo->dataBlocks[off / 4096].data[off], src, n);
  return n;
}

int ramfs_readi(struct vfile* vfile, char *src, uint off, uint n)
{
   struct ram* readFrom = (struct ram*)vfile->fsp;
  memmove(src, &readFrom->dataBlocks[off / 4096].data[off], n);
  if(strlen(src) != 0)
  {
    return strlen(src);
  }
  else
  {
    return 0;
  }

}


