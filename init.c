#include <stdio.h>
#include <string.h>
#include "init.h"

static void update_super_block(void) //写超级块
{
    fp=fopen("./Ext2","r+");
    fseek(fp,DISK_START,SEEK_SET);
    fwrite(sb_block,SB_SIZE,1,fp);
    fflush(fp); //立刻将缓冲区的内容输出，保证磁盘内存数据的一致性
}

static void reload_super_block(void) //读超级块
{
    fseek(fp,DISK_START,SEEK_SET);
    fread(sb_block,SB_SIZE,1,fp);//读取内容到超级块缓冲区中
}

static void update_group_desc(void) //写组描述符
{
    fp=fopen("./Ext2","r+");
    fseek(fp,GDT_START,SEEK_SET);
    fwrite(gdt,GD_SIZE,1,fp);
    fflush(fp);
}

static void reload_group_desc(void) // 读组描述符
{

    fseek(fp,GDT_START,SEEK_SET);
    fread(gdt,GD_SIZE,1,fp);
}

static void update_inode_entry(unsigned short i) // 写第i个inode
{
    fp=fopen("./Ext2","r+");
    fseek(fp,INODE_TABLE+(i-1)*INODE_SIZE,SEEK_SET);
    fwrite(inode_area,INODE_SIZE,1,fp);
    fflush(fp);
}

static void reload_inode_entry(unsigned short i) // 读第i个inode
{
    fseek(fp,INODE_TABLE+(i-1)*INODE_SIZE,SEEK_SET);
    fread(inode_area,INODE_SIZE,1,fp);
}

static void update_dir(unsigned short i) //   写第i个 数据块
{
    fp=fopen("./Ext2","r+");
    fseek(fp,DATA_BLOCK+i*BLOCK_SIZE,SEEK_SET);
    fwrite(dir,BLOCK_SIZE,1,fp);
    fflush(fp);
}

static void reload_dir(unsigned short i) // 读第i个 数据块
{
    fseek(fp,DATA_BLOCK+i*BLOCK_SIZE,SEEK_SET);
    fread(dir,BLOCK_SIZE,1,fp);
    //fclose(fp);
}

static void update_block_bitmap(void) //写block位图
{
    fp=fopen("./Ext2","r+");
    fseek(fp,BLOCK_BITMAP,SEEK_SET);
    fwrite(bitbuf,BLOCK_SIZE,1,fp);
    fflush(fp);
}

static void reload_block_bitmap(void) //读block位图
{
    fseek(fp,BLOCK_BITMAP,SEEK_SET);
    fread(bitbuf,BLOCK_SIZE,1,fp);
}

static void update_inode_bitmap(void) //写inode位图
{
    fp=fopen("./Ext2","r+");
    fseek(fp,INODE_BITMAP,SEEK_SET);
    fwrite(ibuf,BLOCK_SIZE,1,fp);
    fflush(fp);
}

static void reload_inode_bitmap(void) // 读inode位图
{
    fseek(fp,INODE_BITMAP,SEEK_SET);
    fread(ibuf,BLOCK_SIZE,1,fp);
}

static void update_block(unsigned short i) // 写第i个数据块
{
    fp=fopen("./Ext2","r+");
    fseek(fp,DATA_BLOCK+i*BLOCK_SIZE,SEEK_SET);
    //fseek(fp,0,SEEK_SET);
    fwrite(Buffer,BLOCK_SIZE,1,fp);
    fflush(fp);
}

static void reload_block(unsigned short i) // 读第i个数据块
{
    fseek(fp,DATA_BLOCK+i*BLOCK_SIZE,SEEK_SET);
    fread(Buffer,BLOCK_SIZE,1,fp);
}


static int alloc_block(void) // 分配一个数据块,返回数据块号
{

    //bitbuf共有512个字节，表示4096个数据块。根据last_alloc_block/8计算它在bitbuf的哪一个字节

    unsigned short cur=last_alloc_block;
    //printf("cur: %d\n",cur);
    unsigned char con=128; // 1000 0000b
    int flag=0;
    if(gdt[0].bg_free_blocks_count==0)
    {
        printf("There is no block to be alloced!\n");
        return(0);
    }
    reload_block_bitmap();
    cur/=8;
    while(bitbuf[cur]==255)//该字节的8个bit都已有数据
    {
        if(cur==511)cur=0; //最后一个字节也已经满，从头开始寻找
        else cur++;
    }
    while(bitbuf[cur]&con) //在一个字节中找具体的某一个bit
    {
        con=con/2;
        flag++;
    }
    bitbuf[cur]=bitbuf[cur]+con;
    last_alloc_block=cur*8+flag;

    update_block_bitmap();
    gdt[0].bg_free_blocks_count--;
    update_group_desc();
    return last_alloc_block;
}

static int get_inode(void) // 分配一个inode
{
    unsigned short cur=last_alloc_inode;
    unsigned char con=128;
    int flag=0;
    if(gdt[0].bg_free_inodes_count==0)
    {
        printf("There is no Inode to be alloced!\n");
        return 0;
    }
    reload_inode_bitmap();

    cur=(cur-1)/8;   //第一个标号是1，但是存储是从0开始的
    //printf("%s",)
    while(ibuf[cur]==255) //先看该字节的8个位是否已经填满
    {
        if(cur==511)cur=0;
        else cur++;
    }
    while(ibuf[cur]&con)  //再看某个字节的具体哪一位没有被占用
    {
        con=con/2;
        flag++;
    }
    ibuf[cur]=ibuf[cur]+con;
    last_alloc_inode=cur*8+flag+1;
    update_inode_bitmap();
    gdt[0].bg_free_inodes_count--;
    update_group_desc();
    //printf("%d\n",last_alloc_inode);
    return last_alloc_inode;
}

//当前目录中查找文件或目录为tmp，并得到该文件的 inode 号，它在上级目录中的数据块号以及数据块中目录的项号

static unsigned short reserch_file(char tmp[9],int file_type,unsigned short *inode_num,
														unsigned short *block_num,unsigned short *dir_num)
{
    unsigned short j,k;
    reload_inode_entry(current_dir); //进入当前目录
    j=0;
    while(j<inode_area[0].i_blocks)
    {
        reload_dir(inode_area[0].i_block[j]);
        k=0;
        while(k<32)
        {
            if(!dir[k].inode||dir[k].file_type!=file_type||strcmp(dir[k].name,tmp))
            {
            	k++;
            }
            else
            {
                *inode_num=dir[k].inode;
                *block_num=j;
                *dir_num=k;
                return 1;
            }
        }
        j++;
    }
    return 0;
}

/*为新增目录或文件分配 dir_entry
对于新增文件，只需分配一个inode号
对于新增目录，除了inode号外，还需要分配数据区存储.和..两个目录项*/

static void dir_prepare(unsigned short tmp,unsigned short len,int type)
{
    reload_inode_entry(tmp);

    if(type==2) // 目录
    {
        inode_area[0].i_size=32;
        inode_area[0].i_blocks=1;
        inode_area[0].i_block[0]=alloc_block();
        //printf("alloc %d\n",inode_area[0].i_block[0]);
        dir[0].inode=tmp;
        dir[1].inode=current_dir;
        dir[0].name_len=len;
        dir[1].name_len=current_dirlen;
        dir[0].file_type=dir[1].file_type=2;

        for(type=2;type<32;type++)
            dir[type].inode=0;
        strcpy(dir[0].name,".");
        strcpy(dir[1].name,"..");
        update_dir(inode_area[0].i_block[0]);

        inode_area[0].i_mode=01006;
    }
    else
    {
        inode_area[0].i_size=0;
        inode_area[0].i_blocks=0;
        inode_area[0].i_mode=0407;
    }
    update_inode_entry(tmp);
}


//删除一个块号

static void remove_block(unsigned short del_num)
{
    unsigned short tmp;
    tmp=del_num/8;
    reload_block_bitmap();
    switch(del_num%8) // 更新block位图 将具体的位置为0
    {
        case 0:bitbuf[tmp]=bitbuf[tmp]&127;break; // bitbuf[tmp] & 0111 1111b
        case 1:bitbuf[tmp]=bitbuf[tmp]&191;break; //bitbuf[tmp]  & 1011 1111b
        case 2:bitbuf[tmp]=bitbuf[tmp]&223;break; //bitbuf[tmp]  & 1101 1111b
        case 3:bitbuf[tmp]=bitbuf[tmp]&239;break; //bitbbuf[tmp] & 1110 1111b
        case 4:bitbuf[tmp]=bitbuf[tmp]&247;break; //bitbuf[tmp]  & 1111 0111b
        case 5:bitbuf[tmp]=bitbuf[tmp]&251;break; //bitbuf[tmp]  & 1111 1011b
        case 6:bitbuf[tmp]=bitbuf[tmp]&253;break; //bitbuf[tmp]  & 1111 1101b
        case 7:bitbuf[tmp]=bitbuf[tmp]&254;break; // bitbuf[tmp] & 1111 1110b
    }
    update_block_bitmap();
    gdt[0].bg_free_blocks_count++;
    update_group_desc();
}


//删除一个inode 号

static void remove_inode(unsigned short del_num)
{
    unsigned short tmp;
    tmp=(del_num-1)/8;
    reload_inode_bitmap();
    switch((del_num-1)%8)//更改block位图
    {
        case 0:bitbuf[tmp]=bitbuf[tmp]&127;break;
        case 1:bitbuf[tmp]=bitbuf[tmp]&191;break;
        case 2:bitbuf[tmp]=bitbuf[tmp]&223;break;
        case 3:bitbuf[tmp]=bitbuf[tmp]&239;break;
        case 4:bitbuf[tmp]=bitbuf[tmp]&247;break;
        case 5:bitbuf[tmp]=bitbuf[tmp]&251;break;
        case 6:bitbuf[tmp]=bitbuf[tmp]&253;break;
        case 7:bitbuf[tmp]=bitbuf[tmp]&254;break;
    }
    update_inode_bitmap();
    gdt[0].bg_free_inodes_count++;
    update_group_desc();
}

//在打开文件表中查找是否已打开文件
static unsigned short search_file(unsigned short Inode)
{
    unsigned short fopen_table_point=0;
    while(fopen_table_point<16&&fopen_table[fopen_table_point++]!=Inode);
    if(fopen_table_point==16)
    {
    	return 0;
    }
    return 1;
}

void initialize_disk(void)  //初始化磁盘
{
    int i=0;
    printf("Creating the ext2 file system\n");
    printf("Please wait ");
    while(i<1)
    {
        printf("... ");
        ++i;
    }
    printf("\n");
    last_alloc_inode=1;
    last_alloc_block=0;
    for(i=0;i<16;i++)
    {
    	fopen_table[i]=0; //清空缓冲表
    }
    for(i=0;i<BLOCK_SIZE;i++)
    {
    	Buffer[i]=0; // 清空缓冲区
    }
    if(fp!=NULL)
    {
    	fclose(fp);
    }
    fp=fopen("./Ext2","w+"); //此文件大小是4612*512=2361344B，用此文件来模拟文件系统
    fseek(fp,DISK_START,SEEK_SET);//将文件指针从0开始
    for(i=0;i<DISK_SIZE;i++)
    {
    	fwrite(Buffer,BLOCK_SIZE,1,fp); // 清空文件，即清空磁盘全部用0填充 Buffer为缓冲区起始地址，BLOCK_SIZE表示读取大小，1表示写入对象的个数*/
    }
    // 初始化超级块内容
    reload_super_block();
    strcpy(sb_block[0].sb_volume_name,VOLUME_NAME);
    sb_block[0].sb_disk_size=DISK_SIZE;
    sb_block[0].sb_blocks_per_group=BLOCKS_PER_GROUP;
    sb_block[0].sb_size_per_block=BLOCK_SIZE;
    update_super_block();
    // 根目录的inode号为1
    reload_inode_entry(1);

    reload_dir(0);
    strcpy(current_path,"[root@ /");  // 修改路径名为根目录
    // 初始化组描述符内容
    reload_group_desc();

    gdt[0].bg_block_bitmap=BLOCK_BITMAP; //第一个块位图的起始地址
    gdt[0].bg_inode_bitmap=INODE_BITMAP; //第一个inode位图的起始地址
    gdt[0].bg_inode_table=INODE_TABLE;   //inode表的起始地址
    gdt[0].bg_free_blocks_count=DATA_BLOCK_COUNTS; //空闲数据块数
    gdt[0].bg_free_inodes_count=INODE_TABLE_COUNTS; //空闲inode数
    gdt[0].bg_used_dirs_count=0; // 初始分配给目录的节点数是0
    update_group_desc(); // 更新组描述符内容

    reload_block_bitmap();
    reload_inode_bitmap();

    inode_area[0].i_mode=518;
    inode_area[0].i_blocks=0;
    inode_area[0].i_size=32;
    inode_area[0].i_atime=0;
    inode_area[0].i_ctime=0;
    inode_area[0].i_mtime=0;
    inode_area[0].i_dtime=0;
    inode_area[0].i_block[0]=alloc_block(); //分配数据块
    //printf("%d f\n",inode_area[0].i_block[0]);
    inode_area[0].i_blocks++;
    current_dir=get_inode();
    update_inode_entry(current_dir);

    //初始化根目录的目录项
    dir[0].inode=dir[1].inode=current_dir;
    dir[0].name_len=0;
    dir[1].name_len=0;
    dir[0].file_type=dir[1].file_type=2;
    strcpy(dir[0].name,".");
    strcpy(dir[1].name,"..");
    update_dir(inode_area[0].i_block[0]);
    printf("The ext2 file system has been installed!\n");
    check_disk();
    fclose(fp);
}

//初始化内存
void initialize_memory(void)
{
    int i=0;
    last_alloc_inode=1;
    last_alloc_block=0;
    for(i=0;i<16;i++)
    {
    	fopen_table[i]=0;
    }
    strcpy(current_path,"[root@ /");
    current_dir=1;
    fp=fopen("./Ext2","r+");
    if(fp==NULL)
    {
        printf("The File system does not exist!\n");
        initialize_disk();
        return ;
    }
    reload_super_block();
    if(strcmp(sb_block[0].sb_volume_name,VOLUME_NAME))
    {
    	printf("The File system [%s] is not suppoted yet!\n", sb_block[0].sb_volume_name);
    	printf("The File system loaded error!\n");
    	initialize_disk();
    	return ;
    }
    reload_group_desc();
}

//格式化
void format(void)
{
    initialize_disk();
    initialize_memory();
}

//进入某个目录，实际上是改变当前路径
void cd(char tmp[9])
{
    unsigned short i,j,k,flag;
    /*if(!strcmp(tmp,"../"))
    {
    		tmp[2]='\0';
    }
    else if(!strcmp(tmp,"./"))
    {
    		tmp[1]='\0';
    }*/
    flag=reserch_file(tmp,2,&i,&j,&k);

    if(flag)
    {
        current_dir=i;
        if(!strcmp(tmp,"..")&&dir[k-1].name_len) /* 到上一级目录且不是..目录 */
        {
            current_path[strlen(current_path)-dir[k-1].name_len-1]='\0';
            current_dirlen=dir[k].name_len;
        }
        else if(!strcmp(tmp,"."))
        {
        	return ;
        }
        else if(strcmp(tmp,"..")) // cd 到子目录
        {
            current_dirlen=strlen(tmp);
            strcat(current_path,tmp);
            strcat(current_path,"/");
        }
    }
    else
    {
    	printf("The directory %s not exists!\n",tmp);
    }
}

// 创建目录
void mkdir(char tmp[9],int type)
{
    //printf("%s %d\n",tmp,type);
    unsigned short tmpno,i,j,k,flag;

    // 当前目录下新增目录或文件
    reload_inode_entry(current_dir);
    if(!reserch_file(tmp,type,&i,&j,&k)) // 未找到同名文件
    {
        if(inode_area[0].i_size==4096) // 目录项已满
        {
            printf("Directory has no room to be alloced!\n");
            return;
        }
        flag=1;
        if(inode_area[0].i_size!=inode_area[0].i_blocks*512) // 目录中有某些块中32个 dir_entry 未满
        {
            i=0;
            while(flag&&i<inode_area[0].i_blocks)
            {
                reload_dir(inode_area[0].i_block[i]);
                j=0;
                while(j<32)
                {
                    if(dir[j].inode==0)
                    {
                        flag=0; //找到某个未装满目录项的块
                        break;
                    }
                    j++;
                }
                i++;
            }
            tmpno=dir[j].inode=get_inode();

            dir[j].name_len=strlen(tmp);
            dir[j].file_type=type;
            strcpy(dir[j].name,tmp);
            update_dir(inode_area[0].i_block[i-1]);
        }
        else // 全满 新增加块
        {
            inode_area[0].i_block[inode_area[0].i_blocks]=alloc_block();
            inode_area[0].i_blocks++;
            reload_dir(inode_area[0].i_block[inode_area[0].i_blocks-1]);
            tmpno=dir[0].inode=get_inode();
            dir[0].name_len=strlen(tmp);
            dir[0].file_type=type;
            strcpy(dir[0].name,tmp);
            // 初始化新块的其余目录项
            for(flag=1;flag<32;flag++)
            {
            	dir[flag].inode=0;
            }
            update_dir(inode_area[0].i_block[inode_area[0].i_blocks-1]);
        }
        inode_area[0].i_size+=16;

        update_inode_entry(current_dir);

        // 为新增目录分配 dir_entry
        dir_prepare(tmpno,strlen(tmp),type);
    }
    else  // 已经存在同名文件或目录
    {
        printf("Directory has already existed!\n");
    }

}
//创建文件
void cat(char tmp[9],int type)
{
    unsigned short tmpno,i,j,k,flag;
    reload_inode_entry(current_dir);
    if(!reserch_file(tmp,type,&i,&j,&k))
    {
        if(inode_area[0].i_size==4096)
        {
            printf("Directory has no room to be alloced!\n");
            return;
        }
        flag=1;
        if(inode_area[0].i_size!=inode_area[0].i_blocks*512)
        {
            i=0;
            while(flag&&i<inode_area[0].i_blocks)
            {
                reload_dir(inode_area[0].i_block[i]);
                j=0;
                while(j<32)
                {
                    if(dir[j].inode==0)//找到了未分配的目录项
                    {
                        flag=0;
                        break;
                    }
                    j++;
                }
                i++;
            }
            tmpno=dir[j].inode=get_inode();//分配一个新的inode项
            dir[j].name_len=strlen(tmp);
            dir[j].file_type=type;
            strcpy(dir[j].name,tmp);
            update_dir(inode_area[0].i_block[i-1]);
        }
        else //分配一个新的数据块
        {
            inode_area[0].i_block[inode_area[0].i_blocks]=alloc_block();
            inode_area[0].i_blocks++;
            reload_dir(inode_area[0].i_block[inode_area[0].i_blocks-1]);
            tmpno=dir[0].inode=get_inode();
            dir[0].name_len=strlen(tmp);
            dir[0].file_type=type;
            strcpy(dir[0].name,tmp);
            //初始化新快其他项目为0
            for(flag=1;flag<32;flag++)
            {
                dir[flag].inode=0;
            }
            update_dir(inode_area[0].i_block[inode_area[0].i_blocks-1]);
        }
        inode_area[0].i_size+=16;
        update_inode_entry(current_dir);
        //将新增文件的inode节点初始化
        dir_prepare(tmpno,strlen(tmp),type);

    }
    else
    {
        printf("File has already existed!\n");
    }
}

//删除一个空目录
void rmdir(char tmp[9])
{
    unsigned short i,j,k,flag;
    unsigned short m,n;
    if(!strcmp(tmp,"..")||!strcmp(tmp,"."))
    {
        printf("The directory can not be deleted!\n");
        return;
    }
    flag=reserch_file(tmp,2,&i,&j,&k);
    if(flag)
    {
        reload_inode_entry(dir[k].inode); // 加载要删除的节点
        if(inode_area[0].i_size==32)  // 只有.and ..
        {
            inode_area[0].i_size=0;
            inode_area[0].i_blocks=0;

            remove_block(inode_area[0].i_block[0]);
            // 更新 tmp 所在父目录
            reload_inode_entry(current_dir);
            reload_dir(inode_area[0].i_block[j]);
            remove_inode(dir[k].inode);
            dir[k].inode=0;
            update_dir(inode_area[0].i_block[j]);
            inode_area[0].i_size-=16;
            flag=0;

            /*删除32 个 dir_entry 全为空的数据块
            由于 inode_area[0].i_block[0] 中有目录 . 和 ..
            所以这个数据块的非空 dir_entry 不可能为0*/

            //printf("rm: %d\n",inode_area[0]);
            m=1;
            while(flag<32&&m<inode_area[0].i_blocks)
            {
                flag=n=0;
                reload_dir(inode_area[0].i_block[m]);
                while(n<32)
                {
                    if(!dir[n].inode)
                    {
                    	flag++;
                    }
                    n++;
                }
                //如果删除过后，整个数据块的目录项全都为空。类似于在数组中删除某一个位置
                if(flag==32)
                {
                    remove_block(inode_area[0].i_block[m]);
                    inode_area[0].i_blocks--;
                    while(m<inode_area[0].i_blocks)
                    {
                    	inode_area[0].i_block[m]=inode_area[0].i_block[m+1];
                    	++m;
                    }
                }
            }
            update_inode_entry(current_dir);
            return;
        }
        else
        {
            //printf("%d\n",inode_area[0].i_size);
            for(int l=0;l<inode_area[0].i_blocks;l++)
            {
                reload_dir(inode_area[0].i_block[l]);
                for(int m=0;m<32;m++)
                {
                    if(!strcmp(dir[m].name,".")||!strcmp(dir[m].name,"..")||dir[m].inode==0)
                        continue;
                    if(dir[m].file_type==2)
                    {
                        strcpy(current_path,tmp);
                        current_dir=i;
                       // printf("%d\n",current_dir);
                        //printf("%s\n",dir[m].name);
                        rmdir(dir[m].name);
                    }
                    else if(dir[m].file_type==1)
                    {
                        //printf("%s\n",dir[m].name);
                        del(dir[m].name);
                    }
                }
                //printf("%d\n",inode_area[0].i_size);
                if(inode_area[0].i_size==32)
                {
                    strcpy(current_path,"[root@");
                    current_dir=1;
                    //printf("tmp: %s\n",tmp);
                    rmdir(tmp);
                }
            }
            return;
        	printf("Directory is not null!\n");
        }
    }
    else
    {
    	printf("Directory to be deleted not exists!\n");
    }
}

//删除文件
void del(char tmp[9])
{
    unsigned short i,j,k,m,n,flag;
    m=0;
    flag=reserch_file(tmp,1,&i,&j,&k);
    if(flag)
    {
        flag=0;
        // 若文件 tmp 已打开, 则将对应的 fopen_table 项清0
        while(fopen_table[flag]!=dir[k].inode&&flag<16)
        {
        	flag++;
        }
        if(flag<16)
        {
        	fopen_table[flag]=0;
        }
        reload_inode_entry(i); // 加载删除文件 inode
        //删除文件对应的数据块
        while(m<inode_area[0].i_blocks)
        {
        	remove_block(inode_area[0].i_block[m++]);
        }
        inode_area[0].i_blocks=0;
        inode_area[0].i_size=0;
        remove_inode(i);
        // 更新父目录
        reload_inode_entry(current_dir);
        reload_dir(inode_area[0].i_block[j]);
        dir[k].inode=0; //删除inode节点

        update_dir(inode_area[0].i_block[j]);
        inode_area[0].i_size-=16;
        m=1;
        //删除一项后整个数据块为空，则将该数据块删除
        while(m<inode_area[i].i_blocks)
        {
            flag=n=0;
            reload_dir(inode_area[0].i_block[m]);
            while(n<32)
            {
                if(!dir[n].inode)
                {
                	flag++;
                }
                n++;
            }
            if(flag==32)
            {
                remove_block(inode_area[i].i_block[m]);
                inode_area[i].i_blocks--;
                while(m<inode_area[i].i_blocks)
                {
                	inode_area[i].i_block[m]=inode_area[i].i_block[m+1];
                	++m;
                }
            }
        }
        update_inode_entry(current_dir);
    }
    else
    {
    	printf("The file %s not exists!\n",tmp);
    }
}

//打开文件
void open_file(char tmp[9])
{
    unsigned short flag,i,j,k;
    flag=reserch_file(tmp,1,&i,&j,&k);
    if(flag)
    {
        if(search_file(dir[k].inode))
        {
        	printf("The file %s has opened!\n",tmp);
        }
        else
        {
            flag=0;
            while(fopen_table[flag])
            {
            	flag++;
            }
            fopen_table[flag]=dir[k].inode;
            printf("File %s opened!\n",tmp);
        }
    }
    else printf("The file %s does not exist!\n",tmp);
}

//关闭文件
void close_file(char tmp[9])
{
    unsigned short flag,i,j,k;
    flag=reserch_file(tmp,1,&i,&j,&k);

    if(flag)
    {
        if(search_file(dir[k].inode))
        {
            flag=0;
            while(fopen_table[flag]!=dir[k].inode)
            {
            	flag++;
            }
            fopen_table[flag]=0;
            printf("File %s closed!\n",tmp);
        }
        else
        {
        	printf("The file %s has not been opened!\n",tmp);
        }
    }
    else
    {
    	printf("The file %s does not exist!\n",tmp);
    }
}

// 读文件
void read_file(char tmp[9])
{
    unsigned short flag,i,j,k,t;
    flag=reserch_file(tmp,1,&i,&j,&k);
    if(flag)
    {
        if(search_file(dir[k].inode)) //读文件的前提是该文件已经打开
        {
            reload_inode_entry(dir[k].inode);
            //判断是否有读的权限
            if(!(inode_area[0].i_mode&4)) // i_mode:111b:读,写,执行
            {
                printf("The file %s can not be read!\n",tmp);
                return;
            }
            for(flag=0;flag<inode_area[0].i_blocks;flag++)
            {
                reload_block(inode_area[0].i_block[flag]);
                for(t=0;t<inode_area[0].i_size-flag*512;++t)
                {
                	printf("%c",Buffer[t]);
                }
            }
            if(flag==0)
            {
            	printf("The file %s is empty!\n",tmp);
            }
            else
            {
            	printf("\n");
            }
        }
        else
        {
        	printf("The file %s has not been opened!\n",tmp);
        }
    }
    else printf("The file %s not exists!\n",tmp);
}

//文件以覆盖方式写入
void write_file(char tmp[9]) // 写文件
{
    unsigned short flag,i,j,k,size=0,need_blocks,length;
    flag=reserch_file(tmp,1,&i,&j,&k);
    if(flag)
    {
        if(search_file(dir[k].inode))
        {
            reload_inode_entry(dir[k].inode);
            if(!(inode_area[0].i_mode&2)) // i_mode:111b:读,写,执行
            {
                printf("The file %s can not be writed!\n",tmp);
                return;
            }
            fflush(stdin);
            while(1)
            {
                tempbuf[size]=getchar();
                if(tempbuf[size]=='#')
                {
                    tempbuf[size]='\0';
                    break;
                }
                if(size>=4095)
                {
                    printf("Sorry,the max size of a file is 4KB!\n");
                    break;
                }
                size++;
            }
            if(size>=4095)
            {
            	length=4096;
            }
            else
            {
            	length=strlen(tempbuf);
            }
            //计算需要的数据块数目
            need_blocks=length/512;
            if(length%512)
            {
            	need_blocks++;
            }
            if(need_blocks<9) // 文件最大 8 个 blocks(512 bytes)
            {
                // 分配文件所需块数目
                //因为以覆盖写的方式写，要先判断原有的数据块数目
                if(inode_area[0].i_blocks<=need_blocks)
                {
                    while(inode_area[0].i_blocks<need_blocks)
                    {
                        inode_area[0].i_block[inode_area[0].i_blocks]=alloc_block();
                        inode_area[0].i_blocks++;
                    }
                }
                else
                {
                    while(inode_area[0].i_blocks>need_blocks)
                    {
                        remove_block(inode_area[0].i_block[inode_area[0].i_blocks - 1]);
                        inode_area[0].i_blocks--;
                    }
                }
                j=0;
                while(j<need_blocks)
                {
                    if(j!=need_blocks-1)
                    {
                        reload_block(inode_area[0].i_block[j]);
                        memcpy(Buffer,tempbuf+j*BLOCK_SIZE,BLOCK_SIZE);
                        update_block(inode_area[0].i_block[j]);
                    }
                    else
                    {
                        reload_block(inode_area[0].i_block[j]);
                        memcpy(Buffer,tempbuf+j*BLOCK_SIZE,length-j*BLOCK_SIZE);
                        inode_area[0].i_size=length;
                        update_block(inode_area[0].i_block[j]);
                    }
                    j++;
                }
                update_inode_entry(dir[k].inode);
            }
            else
            {
            	printf("Sorry,the max size of a file is 4KB!\n");
            }
        }
        else
        {
        	printf("The file %s has not opened!\n",tmp);
        }
    }
    else
    {
    	printf("The file %s does not exist!\n",tmp);
    }
}

//查看目录下的内容
void ls(void)
{
    printf("items          type           mode           size\n"); /* 15*4 */
    unsigned short i,j,k,flag;
    i=0;
    reload_inode_entry(current_dir);
    while(i<inode_area[0].i_blocks)
    {
        k=0;
        reload_dir(inode_area[0].i_block[i]);
        while(k<32)
        {
            if(dir[k].inode)
            {
                printf("%s",dir[k].name);
                if(dir[k].file_type==2)
                {
                    j=0;
                    reload_inode_entry(dir[k].inode);
                    if(!strcmp(dir[k].name,".."))
                    {
                    	while(j++<13)
                    	{
                    		printf(" ");
                    	}
                    	flag=1;
                    }
                    else if(!strcmp(dir[k].name,"."))
                    {
                    	while(j++<14)
                    	{
                    		printf(" ");
                    	}
                    	flag=0;
                    }
                    else
                    {
                    	while(j++<15-dir[k].name_len)
                    	{
                    		printf(" ");
                    	}
                    	flag=2;
                    }
                    printf("<DIR>          ");
                    switch(inode_area[0].i_mode&7)
                    {
                        case 1:printf("____x");break;
                        case 2:printf("__w__");break;
                        case 3:printf("__w_x");break;
                        case 4:printf("r____");break;
                        case 5:printf("r___x");break;
                        case 6:printf("r_w__");break;
                        case 7:printf("r_w_x");break;
                    }
                    if(flag!=2)
                    {
                    	printf("          ----");
                    }
                    else
                    {
                    	printf("          ");
                    	printf("%4ld bytes",inode_area[0].i_size);
                    }
                }
                else if(dir[k].file_type==1)
                {
                    j=0;
                    reload_inode_entry(dir[k].inode);
                    while(j++<15-dir[k].name_len)printf(" ");
                    printf("<FILE>         ");
                    switch(inode_area[0].i_mode&7)
                    {
                        case 1:printf("____x");break;
                        case 2:printf("__w__");break;
                        case 3:printf("__w_x");break;
                        case 4:printf("r____");break;
                        case 5:printf("r___x");break;
                        case 6:printf("r_w__");break;
                        case 7:printf("r_w_x");break;
                    }
                    printf("          ");
                    printf("%4ld bytes",inode_area[0].i_size);
                }
                printf("\n");
            }
            k++;
            reload_inode_entry(current_dir);
        }
        i++;
    }
}

//检查磁盘状态
void check_disk(void)
{
	reload_super_block();
	printf("volume name       : %s\n", sb_block[0].sb_volume_name);
	printf("disk size         : %d(blocks)\n", sb_block[0].sb_disk_size);
	printf("blocks per group  : %d(blocks)\n", sb_block[0].sb_blocks_per_group);
	printf("ext2 file size    : %d(kb)\n", sb_block[0].sb_disk_size*sb_block[0].sb_size_per_block/1024);
	printf("block size        : %d(kb)\n", sb_block[0].sb_size_per_block);
}
