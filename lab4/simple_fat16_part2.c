#include <assert.h>
#include <string.h>
#include <errno.h>

#include "fat16.h"

// 忘记写到fat16.h 里的函数定义
extern FAT16* get_fat16_ins();

/**
 * @brief 请勿修改该函数。
 * 该函数用于修复5月13日发布的simple_fat16_part1.c中RootOffset和DataOffset的计算错误。
 * 如果你在Part1中也使用了以下字段或函数：
 *     fat16_ins->RootOffset
 *     fat16_ins->DataOffset
 *     find_root函数的offset_dir输出参数
 * 请手动修改pre_init_fat16函数定义中fat16_ins->RootOffset的计算，如下：
 * 正确的计算：
 *      fat16_ins->RootOffset = fat16_ins->FatOffset + fat16_ins->FatSize * fat16_ins->Bpb.BPB_NumFATS;
 * 错误的计算（5月13日发布的simple_fat16_part1.c中的版本）：
 *   // fat16_ins->RootOffset = fat16_ins->FatOffset * fat16_ins->FatSize * fat16_ins->Bpb.BPB_NumFATS;
 * 即将RootOffset计算中第一个乘号改为加号。
 * @return FAT16* 修复计算并返回文件系统指针
 */
FAT16* get_fat16_ins_fix() {
  FAT16 *fat16_ins = get_fat16_ins();
  fat16_ins->FatOffset =  fat16_ins->Bpb.BPB_RsvdSecCnt * fat16_ins->Bpb.BPB_BytsPerSec;
  fat16_ins->FatSize = fat16_ins->Bpb.BPB_BytsPerSec * fat16_ins->Bpb.BPB_FATSz16;
  fat16_ins->RootOffset = fat16_ins->FatOffset + fat16_ins->FatSize * fat16_ins->Bpb.BPB_NumFATS;
  fat16_ins->ClusterSize = fat16_ins->Bpb.BPB_BytsPerSec * fat16_ins->Bpb.BPB_SecPerClus;
  fat16_ins->DataOffset = fat16_ins->RootOffset + fat16_ins->Bpb.BPB_RootEntCnt * BYTES_PER_DIR;
  return fat16_ins;
}

/**
 * @brief 簇号是否是合法的（表示正在使用的）数据簇号（在CLUSTER_MIN和CLUSTER_MAX之间）
 * 
 * @param cluster_num 簇号
 * @return int        
 */
int is_cluster_inuse(uint16_t cluster_num)
{
  return CLUSTER_MIN <= cluster_num && cluster_num <= CLUSTER_MAX;
}

/**
 * @brief 将data写入簇号为clusterN的簇对应的FAT表项，注意要对文件系统中所有FAT表都进行相同的写入。
 * 
 * @param fat16_ins 文件系统指针
 * @param clusterN  要写入表项的簇号
 * @param data      要写入表项的数据，如下一个簇号，CLUSTER_END（文件末尾），或者0（释放该簇）等等
 * @return int      成功返回0
 */
int write_fat_entry(FAT16 *fat16_ins, WORD clusterN, WORD data) {
  // Hint: 这个函数逻辑与fat_entry_by_cluster函数类似，但这个函数需要修改对应值并写回FAT表中
  BYTE SectorBuffer[BYTES_PER_SECTOR];
  /** TODO: 计算下列值，当然，你也可以不使用这些变量*/
  uint FirstFatSecNum;  // 第一个FAT表开始的扇区号
  uint ClusterOffset;   // clusterN这个簇对应的表项，在每个FAT表项的哪个偏移量
  uint ClusterSec;      // clusterN这个簇对应的表项，在每个FAT表中的第几个扇区（Hint: 这个值与ClusterSec的关系是？）
  uint SecOffset;       // clusterN这个簇对应的表项，在所在扇区的哪个偏移量（Hint: 这个值与ClusterSec的关系是？）
  /*** BEGIN ***/
  FirstFatSecNum=fat16_ins->Bpb.BPB_RsvdSecCnt;
  ClusterOffset=clusterN* 2;
  ClusterSec=ClusterOffset / fat16_ins->Bpb.BPB_BytsPerSec;
  SecOffset=ClusterOffset % fat16_ins->Bpb.BPB_BytsPerSec;
  // printf("%d---%d---%d\n",FirstFatSecNum,ClusterSec,clusterN);
  /*** END ***/
  // Hint: 对系统中每个FAT表都进行写入
  for(uint i=0; i<fat16_ins->Bpb.BPB_NumFATS; i++) {
    /*** BEGIN ***/
    // Hint: 计算出当前要写入的FAT表扇区号
    // Hint: 读扇区，在正确偏移量将值修改为data，写回扇区
    sector_read(fat16_ins->fd,FirstFatSecNum+i*fat16_ins->Bpb.BPB_FATSz16+ClusterSec,SectorBuffer);
    memcpy(&SectorBuffer[SecOffset],&data,2*sizeof(BYTE));
    // printf("?=%d\n",SectorBuffer[SecOffset]);
    sector_write(fat16_ins->fd,FirstFatSecNum+i*fat16_ins->Bpb.BPB_FATSz16+ClusterSec,SectorBuffer);
    /*** END ***/
  }
  return 0;
}

/**
 * @brief 分配n个空闲簇，分配过程中将n个簇通过FAT表项连在一起，然后返回第一个簇的簇号。
 *        最后一个簇的FAT表项将会指向0xFFFF（即文件中止）。
 * @param fat16_ins 文件系统指针
 * @param n         要分配簇的个数
 * @return WORD 分配的第一个簇，分配失败，将返回CLUSTER_END，若n==0，也将返回CLUSTER_END。
 */
WORD alloc_clusters(FAT16 *fat16_ins, uint32_t n)
{
  if (n == 0)
    return CLUSTER_END;

  // Hint: 用于保存找到的n个空闲簇，另外在末尾加上CLUSTER_END，共n+1个簇号
  WORD *clusters = malloc((n + 1) * sizeof(WORD));
  uint allocated = 0; // 已找到的空闲簇个数

  /** TODO: 扫描FAT表，找到n个空闲的簇，存入cluster数组。注意此时不需要修改对应的FAT表项 **/
  /*** BEGIN ***/
  uint FirstFatSecNum=fat16_ins->Bpb.BPB_RsvdSecCnt;
  for(WORD i=2;i<=0xffef&&allocated<n;i++){
    if(fat_entry_by_cluster(fat16_ins,i)==0x0000){
      clusters[allocated++]=i;
    }
  }
  /*** END ***/

  if(allocated != n) {  // 找不到n个簇，分配失败
    free(clusters);
    return CLUSTER_END;
  }

  // Hint: 找到了n个空闲簇，将CLUSTER_END加至末尾。
  clusters[n] = CLUSTER_END;

  /** TODO: 修改clusters中存储的N个簇对应的FAT表项，将每个簇与下一个簇连接在一起。同时清零每一个新分配的簇。**/
  /*** BEGIN ***/
  for(int i=0;i<n;i++){
    write_fat_entry(fat16_ins,clusters[i],clusters[i+1]);
    WORD FatClusEntryVal, FirstSectorofCluster;
    BYTE sector_buffer[BYTES_PER_SECTOR];
    first_sector_by_cluster(fat16_ins,clusters[i],&FatClusEntryVal, &FirstSectorofCluster, sector_buffer);
    for(int i=0;i<fat16_ins->Bpb.BPB_SecPerClus;i++){
      sector_read(fat16_ins->fd,FirstSectorofCluster+i,sector_buffer);
      memset(sector_buffer,0,BYTES_PER_SECTOR);
      sector_write(fat16_ins->fd,FirstSectorofCluster+i,sector_buffer);
    }
  }

  /*** END ***/

  // 返回首个分配的簇
  WORD first_cluster = clusters[0];
  free(clusters);
  return first_cluster;
}


// ------------------TASK3: 创建/删除文件夹-----------------------------------

/**
 * @brief 创建path对应的文件夹
 * 
 * @param path 创建的文件夹路径
 * @param mode 文件模式，本次实验可忽略，默认都为普通文件夹
 * @return int 成功:0， 失败: POSIX错误代码的负值
 */
int fat16_mkdir(const char *path, mode_t mode) {
  /* Gets volume data supplied in the context during the fat16_init function */
  FAT16 *fat16_ins = get_fat16_ins_fix();

  int findFlag = 0;     // 是否找到空闲的目录项
  int sectorNum = 0;    // 找到的空闲目录项所在扇区号
  int offset = 0;       // 找到的空闲目录项在扇区中的偏移量
  /** TODO: 模仿mknod，计算出findFlag, sectorNum和offset的值
   *  你也可以选择不使用这些值，自己定义其它变量。注意本函数前半段和mknod前半段十分类似。
   **/
  /*** BEGIN ***/
  int pathDepth;
  char **paths = path_split((char *)path, &pathDepth);
  char *copyPath = strdup(path);
  const char **orgPaths = (const char **)org_path_split(copyPath);
  char *prtPath = get_prt_path(path, orgPaths, pathDepth);
  BYTE sector_buffer[BYTES_PER_SECTOR];
  int RootDirCnt = 1, DirSecCnt = 1;
  WORD ClusterN, FatClusEntryVal, FirstSectorofCluster;
  if (strcmp(prtPath, "/") == 0)
  {
    sector_read(fat16_ins->fd, fat16_ins->FirstRootDirSecNum, sector_buffer);
    for (uint i = 1; i <= fat16_ins->Bpb.BPB_RootEntCnt; i++)
    {
      BYTE file_name[12]={0};
      if(!(sector_buffer[((i-1)%16)*32+11]==0x0f)){
        for(uint j=0;j<11;j++){
          file_name[j]=sector_buffer[((i-1)%16)*32+j];
        }
        if(strcmp(*paths,file_name)==0){
          findFlag=0;
          break;
        }
      }
      if((sector_buffer[((i-1)%16)*32]==0||sector_buffer[((i-1)%16)*32]==0xe5)&&findFlag==0){
        findFlag=1;
        sectorNum=fat16_ins->FirstRootDirSecNum+RootDirCnt-1;
        offset=((i-1)%16)*32;
      }
      if (i % 16 == 0 && i != fat16_ins->Bpb.BPB_RootEntCnt)
      {
        sector_read(fat16_ins->fd, fat16_ins->FirstRootDirSecNum + RootDirCnt, sector_buffer);
        RootDirCnt++;
      }

    }
  }
  else
  {
    DIR_ENTRY Dir;
    off_t offset_dir;
    find_root(fat16_ins, &Dir, prtPath, &offset_dir);
    WORD ClusterN;
    WORD FatClusEntryVal;
    WORD FirstSectorofCluster;
    ClusterN = Dir.DIR_FstClusLO;
    first_sector_by_cluster(fat16_ins, ClusterN, &FatClusEntryVal, &FirstSectorofCluster, sector_buffer);
    for (uint i = 1;; i++)
    {
      BYTE file_name[12]={0};
      if(!(sector_buffer[((i-1)%16)*32+11]==0x0f)){
        for(uint j=0;j<11;j++){
          file_name[j]=sector_buffer[((i-1)%16)*32+j];
        }
        if(strcmp(paths[pathDepth-1],file_name)==0){
          findFlag=0;
          break;
        }
      }
      if((sector_buffer[((i-1)%16)*32]==0||sector_buffer[((i-1)%16)*32]==0xe5)&&findFlag==0){
        findFlag=1;
        sectorNum=FirstSectorofCluster+DirSecCnt-1;
        offset=((i-1)%16)*32;
      }
      if (i % 16 == 0)
      {
        if (DirSecCnt < fat16_ins->Bpb.BPB_SecPerClus)
        {
          sector_read(fat16_ins->fd,FirstSectorofCluster+DirSecCnt,sector_buffer);
          DirSecCnt++;
        }
        else 
        {
          if (FatClusEntryVal == 0xffff||FatClusEntryVal == 0xfff8)
          {
            break;
          }
          ClusterN=FatClusEntryVal;
          first_sector_by_cluster(fat16_ins, ClusterN, &FatClusEntryVal, &FirstSectorofCluster, sector_buffer);
          i=1;
          DirSecCnt=1;
        }
      }
    }
  }
  /*** END ***/

  /** TODO: 在父目录的目录项中添加新建的目录。
   *        同时，为新目录分配一个簇，并在这个簇中创建两个目录项，分别指向. 和 .. 。
   *        目录的文件大小设置为0即可。
   *  HINT: 使用正确参数调用dir_entry_create来创建上述三个目录项。
   **/
  if (findFlag == 1) {
    /*** BEGIN ***/
    // printf("begin create dir %s,%d\n",paths[pathDepth-1],sectorNum);
    WORD cluster=alloc_clusters(fat16_ins,1);
    if(cluster==0xffff)return -1;
    dir_entry_create(fat16_ins,sectorNum,offset,paths[pathDepth-1],0x10,cluster,fat16_ins->Bpb.BPB_BytsPerSec*fat16_ins->Bpb.BPB_SecPerClus);
    BYTE sector_buffer[BYTES_PER_SECTOR];
    WORD FatClusEntryVal,FirstSectorofCluster;
    first_sector_by_cluster(fat16_ins, cluster, &FatClusEntryVal, &FirstSectorofCluster, sector_buffer);
    dir_entry_create(fat16_ins,FirstSectorofCluster,0,".",0x20,0xffff,0);
    dir_entry_create(fat16_ins,FirstSectorofCluster,32,"..",0x20,0xffff,0);
    /*** END ***/
  }
  return 0;
}


/**
 * @brief 删除offset位置的目录项
 * 
 * @param fat16_ins 文件系统指针
 * @param offset    find_root传回的offset_dir值
 */
void dir_entry_delete(FAT16 *fat16_ins, off_t offset) {
  BYTE buffer[BYTES_PER_SECTOR];
  /** TODO: 删除目录项，或者说，将镜像文件offset处的目录项第一个字节设置为0xe5即可。
   *  HINT: offset对应的扇区号和扇区的偏移量是？只需要读取扇区，修改offset处的一个字节，然后将扇区写回即可。
   */
  /*** BEGIN ***/
  sector_read(fat16_ins->fd,offset/fat16_ins->Bpb.BPB_BytsPerSec,buffer);
  uint8_t del=0xe5;
  memcpy(&buffer[offset%fat16_ins->Bpb.BPB_BytsPerSec],&del,sizeof(BYTE));
  sector_write(fat16_ins->fd,offset/fat16_ins->Bpb.BPB_BytsPerSec,buffer);
  /*** END ***/
}

/**
 * @brief 写入offset位置的目录项
 * 
 * @param fat16_ins 文件系统指针
 * @param offset    find_root传回的offset_dir值
 * @param Dir       要写入的目录项
 */
void dir_entry_write(FAT16 *fat16_ins, off_t offset, const DIR_ENTRY *Dir) {
  BYTE buffer[BYTES_PER_SECTOR];
  // TODO: 修改目录项，和dir_entry_delete完全类似，只是需要将整个Dir写入offset所在的位置。
  /*** BEGIN ***/
  sector_read(fat16_ins->fd,offset/fat16_ins->Bpb.BPB_BytsPerSec,buffer);
  memcpy(&buffer[offset%fat16_ins->Bpb.BPB_BytsPerSec],Dir,32*sizeof(BYTE));
  sector_write(fat16_ins->fd,offset/fat16_ins->Bpb.BPB_BytsPerSec,buffer);

  /*** END ***/
}
/**
 * @brief 删除path对应的文件夹
 * 
 * @param path 要删除的文件夹路径
 * @return int 成功:0， 失败: POSIX错误代码的负值
 */
int fat16_rmdir(const char *path) {
  /* Gets volume data supplied in the context during the fat16_init function */
  // FAT16 *fat16_ins = get_fat16_ins_fix();
  FAT16 *fat16_ins = get_fat16_ins();

  if(strcmp(path, "/") == 0) {
    return -EBUSY;  // 无法删除根目录，根目录是挂载点（可参考`man 2 rmdir`）
  }

  DIR_ENTRY Dir;
  DIR_ENTRY curDir;
  off_t offset;
  int res = find_root(fat16_ins, &Dir, path, &offset);

  if(res != 0) {
    return -ENOENT; // 路径不存在
  }

  if(Dir.DIR_Attr != ATTR_DIRECTORY) {
    return ENOTDIR; // 路径不是目录
  }

  /** TODO: 检查目录是否为空，如果目录不为空，直接返回-ENOTEMPTY。
   *        注意空目录也可能有"."和".."两个子目录。
   *  HINT: 这一段和readdir的非根目录部分十分类似。
   *  HINT: 注意忽略DIR_Attr为0x0F的长文件名项(LFN)。
   **/

  /*** BEGIN ***/
  // printf("check\n;");
  WORD ClusterN; 
  WORD FatClusEntryVal;
  WORD FirstSectorofCluster;  
  int DirSecCnt=1;
  BYTE sector_buffer[BYTES_PER_SECTOR];
  ClusterN = Dir.DIR_FstClusLO; 
  first_sector_by_cluster(fat16_ins, ClusterN, &FatClusEntryVal, &FirstSectorofCluster, sector_buffer);
  for (uint i = 1; Dir.DIR_Name[0] != 0x00&&ClusterN!=0xffff; i++)
  {
    BYTE file_name[12]={0};
    if(!(sector_buffer[((i-1)%16)*32+11]==0x0f||sector_buffer[((i-1)%16)*32]==0xe5||sector_buffer[((i-1)%16)*32]==0x00)){
      memcpy(file_name,sector_buffer,11*sizeof(BYTE));
      if(!(strcmp(path_decode(file_name),".")==0||strcmp(path_decode(file_name),"..")==0))return -ENOTEMPTY;
    }
    if (i % 16 == 0)
    {
      if (DirSecCnt < fat16_ins->Bpb.BPB_SecPerClus)
      {
        sector_read(fat16_ins->fd,FirstSectorofCluster+DirSecCnt,sector_buffer);
        DirSecCnt++;
      }
      else
      {
        if (FatClusEntryVal == 0xffff)
        {
          break;
        }
        ClusterN=FatClusEntryVal;
        first_sector_by_cluster(fat16_ins, ClusterN, &FatClusEntryVal, &FirstSectorofCluster, sector_buffer);
        i=1;
        DirSecCnt=1;
      }
    }
  }

  /*** END ***/

  // 已确认目录项为空，释放目录占用的簇
  // TODO: 循环调用free_cluster释放对应簇，和unlink类似。
  /*** BEGIN ***/
  // printf("free cluster\n");
  ClusterN=Dir.DIR_FstClusLO;
  while(ClusterN!=0xffff){
    ClusterN=(WORD)free_cluster(fat16_ins,ClusterN);
    if(ClusterN==0xfff8)printf("error!\n\n");
  }

  /*** END ***/

  // TODO: 删除父目录中的目录项
  // HINT: 如果你正确实现了dir_entry_delete，这里只需要一行代码调用它即可
  //       你也可以使用你在unlink使用的方法。
  /*** BEGIN ***/
  dir_entry_delete(fat16_ins,offset);
  /*** END ***/

  return 0;
}


// ------------------TASK4: 写文件-----------------------------------

/**
 * @brief 将data中的数据写入编号为clusterN的簇的offset位置。
 *        注意size+offset <= 簇大小
 * 
 * @param fat16_ins 文件系统指针
 * @param clusterN  要写入数据的块号
 * @param data      要写入的数据
 * @param size      要写入数据的大小（字节）
 * @param offset    要写入簇的偏移量
 * @return size_t   成功写入的字节数
 */
size_t write_to_cluster_at_offset(FAT16 *fat16_ins, WORD clusterN, off_t offset, const BYTE* data, size_t size) {
  assert(offset + size <= fat16_ins->ClusterSize);  // offset + size 必须小于簇大小
  BYTE sector_buffer[BYTES_PER_SECTOR];
  /** TODO: 将数据写入簇对应的偏移量上。
   *        你需要找到第一个需要写入的扇区，和要写入的偏移量，然后依次写入后续每个扇区，直到所有数据都写入完成。
   *        注意，offset对应的首个扇区和offset+size对应的最后一个扇区都可能只需要写入一部分。
   *        所以应该先将扇区读出，修改要写入的部分，再写回整个扇区。
   */
  /*** BEGIN ***/
  int off=offset%fat16_ins->Bpb.BPB_BytsPerSec;
  WORD FatClusEntryVal,FirstSector;
  first_sector_by_cluster(fat16_ins, clusterN, &FatClusEntryVal, &FirstSector, sector_buffer);
  size_t cpsize=size>fat16_ins->Bpb.BPB_BytsPerSec-off?(fat16_ins->Bpb.BPB_BytsPerSec-off):size;
  size_t resize=size;
  size_t alsize=0;
  for(int secnum=FirstSector+offset/fat16_ins->Bpb.BPB_BytsPerSec;secnum<FirstSector+fat16_ins->Bpb.BPB_SecPerClus&&resize>0;secnum++){
    sector_read(fat16_ins->fd,secnum,sector_buffer);
    printf("sector=%d---off=%d\n",secnum,off);
    printf("cpsize=%d---resize=%d---alsize=%d\n",cpsize,resize,alsize);
    memcpy(&sector_buffer[off],&data[alsize],cpsize);
    resize=resize-cpsize;
    alsize+=cpsize;
    if(secnum==156)printf("sector=%s\n",sector_buffer);
    sector_write(fat16_ins->fd,secnum,sector_buffer);
    off=0;
    cpsize=resize>fat16_ins->Bpb.BPB_BytsPerSec-off?(fat16_ins->Bpb.BPB_BytsPerSec-off):resize;
    // printf("!!\n");
  }
  
  /*** END ***/
  return size;
}


/**
 * @brief 查找文件最末尾的一个簇，同时计算文件当前簇数，如果文件没有任何簇，返回CLUSTER_END。
 * 
 * @param fat16_ins 文件系统指针 
 * @param Dir       文件的目录项
 * @param count     输出参数，当为NULL时忽略该参数，否则设置为文件当前簇的数量
 * @return WORD     文件最后一个簇的编号
 */
WORD file_last_cluster(FAT16 *fat16_ins, DIR_ENTRY *Dir, int64_t *count) {
  
  int64_t cnt = 0;        // 文件拥有的簇数量
  WORD cur = CLUSTER_END; // 最后一个被文件使用的簇号
  // TODO: 找到Dir对应的文件的最后一个簇编号，并将该文件当前簇的数目填充入count
  // HINT: 可能用到的函数：is_cluster_inuse和fat_entry_by_cluster函数。
  /*** BEGIN ***/
  WORD nextcluster=Dir->DIR_FstClusLO;
  // printf("?1\n");
  while(is_cluster_inuse(nextcluster)){
    cnt++;
    cur=nextcluster;
    // printf("11next=%d\n",nextcluster);
    nextcluster=fat_entry_by_cluster(fat16_ins,cur);
    // printf("11next=%d\n",nextcluster);
  }
  // printf("zhen\n");
  /*** END ***/
  if(count != NULL) { // 如果count为NULL，不填充count
    *count = cnt;
  }
  // printf("%d\n",cur);
  return cur;
}

/**
 * @brief 为Dir指向的文件新分配count个簇，并将其连接在文件末尾，保证新分配的簇全部以0填充。
 *        注意，如果文件当前没有任何簇，本函数应该修改Dir->DIR_FstClusLO值，使其指向第一个簇。
 * 
 * @param fat16_ins     文件系统指针
 * @param Dir           要分配新簇的文件的目录项
 * @param last_cluster  file_last_cluster的返回值，当前该文件的最后一个簇簇号。
 * @param count         要新分配的簇数量
 * @return int 成功返回分配前原文件最后一个簇簇号，失败返回POSIX错误代码的负值
 */
int file_new_cluster(FAT16 *fat16_ins, DIR_ENTRY *Dir, WORD last_cluster, DWORD count)
{
  /** TODO: 先用alloc_clusters分配count个簇。
   *        然后若原文件本身有至少一个簇，修改原文件最后一个簇的FAT表项，使其与新分配的簇连接。
   *        否则修改Dir->DIR_FstClusLO值，使其指向第一个簇。
   */
  /*** BEGIN ***/
  int64_t tmpcount=1;
  if(last_cluster==0xffff){
    Dir->DIR_FstClusLO=alloc_clusters(fat16_ins,count);
    last_cluster=file_last_cluster(fat16_ins,Dir,&tmpcount);
  }
  else{
    write_fat_entry(fat16_ins,last_cluster,alloc_clusters(fat16_ins,count));
    last_cluster=file_last_cluster(fat16_ins,Dir,&tmpcount);
    // printf("!last=%d\n",last_cluster);
  }
  /*** END ***/
  return last_cluster;
}

/**
 * @brief 在文件offset的位置写入buff中的数据，数据长度为length。
 * 
 * @param fat16_ins   文件系统执政
 * @param Dir         要写入的文件目录项
 * @param offset_dir  find_root返回的offset_dir值
 * @param buff        要写入的数据
 * @param offset      文件要写入的位置
 * @param length      要写入的数据长度（字节）
 * @return int        成功时返回成功写入数据的字节数，失败时返回POSIX错误代码的负值
 */
int write_file(FAT16 *fat16_ins, DIR_ENTRY *Dir, off_t offset_dir, const void *buff, off_t offset, size_t length)
{

  if (length == 0)
    return 0;

  if (offset + length < offset) // 溢出了
    return -EINVAL;

  /** TODO: 通过offset和length，判断文件是否修改文件大小，以及是否需要分配新簇，并正确修改大小和分配簇。
   *  HINT: 可能用到的函数：file_last_cluster, file_new_cluster等
   */
  /*** BEGIN ***/
  // printf("1\n");
  Dir->DIR_FileSize+=length;
  // printf("length=%d---offset%d\n",length,offset);
  WORD newcluster;
  WORD lastcluster;
  int64_t tmpcount=1;
  lastcluster=file_last_cluster(fat16_ins,Dir,&tmpcount);
  // printf("Dir->DIR_FstClusLO=%d\n",Dir->DIR_FstClusLO);
  // printf("next=%d\n",fat_entry_by_cluster(fat16_ins,Dir->DIR_FstClusLO));
  if(offset/(fat16_ins->Bpb.BPB_SecPerClus*fat16_ins->Bpb.BPB_BytsPerSec)<(offset+length)/(fat16_ins->Bpb.BPB_SecPerClus*fat16_ins->Bpb.BPB_BytsPerSec))
  {
    // printf("4\n");
    DWORD count=(offset+length+fat16_ins->ClusterSize-1)/fat16_ins->ClusterSize-(offset+fat16_ins->ClusterSize-1)/fat16_ins->ClusterSize;
    // printf("count=%d---%d\n",count,fat16_ins->ClusterSize-offset);
    file_new_cluster(fat16_ins,Dir,lastcluster,count);
    if(lastcluster==0xffff)lastcluster=Dir->DIR_FstClusLO;
    else lastcluster=fat_entry_by_cluster(fat16_ins,lastcluster);
    
  }
  else if(lastcluster==0xffff){
    
    Dir->DIR_FstClusLO=alloc_clusters(fat16_ins,1);
    lastcluster=Dir->DIR_FstClusLO;
    
    // printf("3--clu=%d\n",lastcluster);
  }
  if(lastcluster==0xffff)return -1;
  // return -1;
  /*** END ***/

  /** TODO: 和read类似，找到对应的偏移，并写入数据。
   *  HINT: 如果你正确实现了write_to_cluster_at_offset，此处逻辑会简单很多。
   */
  /*** BEGIN ***/
  // HINT: 记得把修改过的Dir写回目录项（如果你之前没有写回）
  // printf("2---%d\n",offset);
  size_t resize=length;
  size_t cpsize=0;
  size_t alsize=0;
  
  WORD ncluster=lastcluster;
  offset=offset%fat16_ins->ClusterSize;
  // if(offset==0)ncluster=fat_entry_by_cluster(fat16_ins,lastcluster);
  // else ncluster=lastcluster;
  // printf("buff=%s\n",buff);
  // if(ncluster==1211)printf("buff=%s\n",buff);
  for(;resize>0&&ncluster!=0xffff;){
    cpsize=resize>fat16_ins->ClusterSize-offset?(fat16_ins->ClusterSize-offset):resize;
    // printf("halfbuff=%s\n",&buff[alsize]);
    printf("cluster=%d\n",ncluster);
    printf("%d---%d---%d\n",cpsize,alsize,resize);
    write_to_cluster_at_offset(fat16_ins,ncluster,offset,&buff[alsize],cpsize);
    alsize+=cpsize;
    resize-=cpsize;
    offset=0;
    ncluster=fat_entry_by_cluster(fat16_ins,ncluster);
    // printf("??\n");
  }
  // write_to_cluster_at_offset(fat16_ins,lastcluster,off,buff,length);
  dir_entry_write(fat16_ins,offset_dir,Dir);
  return length;
  /*** END ***/
  return 0;
}

/**
 * @brief 将长度为size的数据data写入path对应的文件的offset位置。注意当写入数据量超过文件本身大小时，
 *        需要扩展文件的大小，必要时需要分配新的簇。
 * 
 * @param path    要写入的文件的路径
 * @param data    要写入的数据
 * @param size    要写入数据的长度
 * @param offset  文件中要写入数据的偏移量（字节）
 * @param fi      本次实验可忽略该参数
 * @return int    成功返回写入的字节数，失败返回POSIX错误代码的负值。
 */
int fat16_write(const char *path, const char *data, size_t size, off_t offset,
                struct fuse_file_info *fi)
{
  FAT16 *fat16_ins = get_fat16_ins_fix();
  /** TODO: 大部分工作都在write_file里完成了，这里调用find_root获得目录项，然后调用write_file即可
   */
  /*** BEGIN ***/
  DIR_ENTRY Dir;
  off_t offset_dir;
  // printf("%s\n",path);
  find_root(fat16_ins,&Dir,path,&offset_dir);
  return write_file(fat16_ins,&Dir,offset_dir,data,offset,size);
  /*** END ***/
  return 0;
}

/**
 * @brief 将path对应的文件大小改为size，注意size可以大于小于或等于原文件大小。
 *        若size大于原文件大小，需要将拓展的部分全部置为0，如有需要，需要分配新簇。
 *        若size小于原文件大小，将从末尾截断文件，若有簇不再被使用，应该释放对应的簇。
 *        若size等于原文件大小，什么都不需要做。
 * 
 * @param path 需要更改大小的文件路径 
 * @param size 新的文件大小
 * @return int 成功返回0，失败返回POSIX错误代码的负值。
 */
int fat16_truncate(const char *path, off_t size)
{
  /* Gets volume data supplied in the context during the fat16_init function */
  FAT16 *fat16_ins = get_fat16_ins_fix();
  // printf("truncate\n\n\n\n");
  /* Searches for the given path */
  DIR_ENTRY Dir;
  off_t offset_dir;
  find_root(fat16_ins, &Dir, path, &offset_dir);

  // 当前文件已有簇的数量，以及截断或增长后，文件所需的簇数量。
  int64_t cur_cluster_count;
  WORD last_cluster = file_last_cluster(fat16_ins, &Dir, &cur_cluster_count);
  int64_t new_cluster_count = (size + fat16_ins->ClusterSize - 1) / fat16_ins->ClusterSize;

  DWORD new_size = size;
  DWORD old_size = Dir.DIR_FileSize;

  if (old_size == new_size){
    return 0;
  } else if (old_size < new_size) {
    /** TODO: 增大文件大小，注意是否需要分配新簇，以及往新分配的空间填充0等 **/
    /*** BEGIN ***/
    Dir.DIR_FileSize=new_size;
    DWORD count=new_cluster_count-cur_cluster_count;
    if(count>0){
      WORD newcluster=alloc_clusters(fat16_ins,count);
      write_fat_entry(fat16_ins,last_cluster,newcluster);
      if(old_size==0)file_new_cluster(fat16_ins,&Dir,Dir.DIR_FstClusLO,count);
      else file_new_cluster(fat16_ins,&Dir,last_cluster,count);
    }
    BYTE buffer[BYTES_PER_SECTOR];
    WORD sector,en;
    first_sector_by_cluster(fat16_ins,last_cluster,&en,&sector,buffer);
    unsigned int sec=sector+(old_size%fat16_ins->ClusterSize+fat16_ins->Bpb.BPB_BytsPerSec-1)/fat16_ins->Bpb.BPB_BytsPerSec-1;
    unsigned int off=old_size%fat16_ins->Bpb.BPB_BytsPerSec;
    // write_to_cluster_at_offset(fat16_ins,last_cluster,off,)
    for(;sec<sector+fat16_ins->Bpb.BPB_SecPerClus&&sec>=sector;sec++){
      sector_read(fat16_ins->fd,sec,buffer);
      memset(&buffer[off],0,BYTES_PER_SECTOR-off);
      sector_write(fat16_ins->fd,sec,buffer);
      off=0;
    }
    dir_entry_write(fat16_ins,offset_dir,&Dir);
    /*** END ***/

  } else {  // 截断文件
      /** TODO: 截断文件，注意是否需要释放簇等 **/
      /*** BEGIN ***/
      Dir.DIR_FileSize=new_size;
      DWORD count=cur_cluster_count-new_cluster_count;
      if(count>0){
        WORD cluster=Dir.DIR_FstClusLO;
        WORD lastcluster;
        for(int i=1;i<=new_cluster_count;i++){
          lastcluster=cluster;
          cluster=fat_entry_by_cluster(fat16_ins,cluster);
        }
        write_fat_entry(fat16_ins,lastcluster,CLUSTER_END);
        for(int i=new_cluster_count;i<=cur_cluster_count;i++){
          cluster=free_cluster(fat16_ins,cluster);
        }
        
      }
      dir_entry_write(fat16_ins,offset_dir,&Dir);
      /*** END ***/
  }
  return 0;
}
