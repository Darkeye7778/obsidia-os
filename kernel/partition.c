#include "partition.h"
#include "memory/heap.h"
#include <stdint.h>

#define PARTITION_LIMIT 16U
#define GPT_ENTRY_LIMIT 128U

const uint8_t OBSIDIA_STATE_PARTITION_TYPE_GUID[16]={
    0x4f,0x42,0x53,0x49,0x44,0x49,0x41,0x53,0x54,0x41,0x54,0x45,0x46,0x53,0x01,0x00
};

typedef struct {
    block_device_t* parent;
    block_partition_info_t info;
} partition_private_t;

static uint32_t read32(const uint8_t*value){return(uint32_t)value[0]|((uint32_t)value[1]<<8)|((uint32_t)value[2]<<16)|((uint32_t)value[3]<<24);}
static uint64_t read64(const uint8_t*value){uint64_t result=0;for(uint32_t i=0;i<8;i++)result|=(uint64_t)value[i]<<(i*8);return result;}
static void write32(uint8_t*value,uint32_t input){for(uint32_t i=0;i<4;i++)value[i]=(uint8_t)(input>>(i*8));}
static void write64(uint8_t*value,uint64_t input){for(uint32_t i=0;i<8;i++)value[i]=(uint8_t)(input>>(i*8));}
static int equal(const uint8_t*a,const uint8_t*b,uint32_t count){for(uint32_t i=0;i<count;i++)if(a[i]!=b[i])return 0;return 1;}
static int zero_guid(const uint8_t*guid){for(uint32_t i=0;i<16;i++)if(guid[i])return 0;return 1;}
static uint32_t crc32(const void*data,uint32_t length){uint32_t crc=0xffffffffU;const uint8_t*p=data;for(uint32_t i=0;i<length;i++){crc^=p[i];for(uint32_t bit=0;bit<8;bit++)crc=(crc>>1)^(0xedb88320U&((uint32_t)-(int32_t)(crc&1)));}return~crc;}

static int overlaps(const block_partition_info_t*parts,uint32_t count,uint64_t start,uint64_t blocks){
    uint64_t end=start+blocks;
    for(uint32_t i=0;i<count;i++){uint64_t other_end=parts[i].start_lba+parts[i].block_count;if(start<other_end&&parts[i].start_lba<end)return 1;}
    return 0;
}

static int parse_mbr(block_device_t*disk,block_partition_info_t*parts,uint32_t*count,int*protective){
    *count=0;*protective=0;
    if(!disk||disk->block_size!=512)return-1;
    uint8_t*sector=kmalloc(512);if(!sector)return-1;
    if(block_read(disk,0,1,sector)){kfree(sector);return-1;}
    if(sector[510]!=0x55||sector[511]!=0xaa){kfree(sector);return 0;}
    for(uint32_t i=0;i<4;i++){
        const uint8_t*entry=sector+446+i*16;uint8_t status=entry[0],type=entry[4];uint64_t start=read32(entry+8),blocks=read32(entry+12);
        if(status!=0&&status!=0x80){kfree(sector);return-1;}if(!type&&!start&&!blocks)continue;if(!type||!blocks||start>=disk->block_count||blocks>disk->block_count-start){kfree(sector);return-1;}
        if(type==0xee){*protective=1;continue;}if(type==0x05||type==0x0f||type==0x85)continue;
        if(*count>=PARTITION_LIMIT||overlaps(parts,*count,start,blocks)){kfree(sector);return-1;}
        block_partition_info_t*p=&parts[(*count)++];for(uint32_t n=0;n<sizeof(*p);n++)((uint8_t*)p)[n]=0;
        p->scheme=BLOCK_PARTITION_SCHEME_MBR;p->index=i+1;p->start_lba=start;p->block_count=blocks;p->mbr_type=type;
    }
    kfree(sector);return 1;
}

static int parse_gpt_at(block_device_t*disk,uint64_t header_lba,block_partition_info_t*parts,uint32_t*count){
    *count=0;
    if(!disk||disk->block_size!=512||disk->block_count<68)return-1;
    uint8_t*header=kmalloc(512);if(!header)return-1;
    if(header_lba>=disk->block_count||block_read(disk,header_lba,1,header)||!equal(header,(const uint8_t*)"EFI PART",8)){kfree(header);return-1;}
    uint32_t revision=read32(header+8),header_size=read32(header+12),saved_crc=read32(header+16);
    uint64_t current=read64(header+24),backup=read64(header+32),first=read64(header+40),last=read64(header+48),table_lba=read64(header+72);
    uint32_t entries=read32(header+80),entry_size=read32(header+84),table_crc=read32(header+88);
    if(revision<0x00010000U||header_size<92||header_size>512||current!=header_lba||backup>=disk->block_count||backup==current||
       first>last||last>=disk->block_count||!entries||entries>GPT_ENTRY_LIMIT||entry_size<128||entry_size>512||(entry_size&7)){kfree(header);return-1;}
    write32(header+16,0);if(crc32(header,header_size)!=saved_crc){kfree(header);return-1;}kfree(header);
    uint64_t table_bytes=(uint64_t)entries*entry_size,sectors=(table_bytes+511U)/512U;
    if(!sectors||sectors>128||table_lba>=disk->block_count||sectors>disk->block_count-table_lba)return-1;
    uint8_t*table=kmalloc(sectors*512U);if(!table)return-1;
    if(block_read(disk,table_lba,sectors,table)||crc32(table,(uint32_t)table_bytes)!=table_crc){kfree(table);return-1;}
    for(uint32_t i=0;i<entries;i++){
        uint8_t*entry=table+(uint64_t)i*entry_size;if(zero_guid(entry))continue;uint64_t begin=read64(entry+32),end=read64(entry+40);
        if(begin>end||begin<first||end>last||end>=disk->block_count||*count>=PARTITION_LIMIT||overlaps(parts,*count,begin,end-begin+1)){kfree(table);return-1;}
        block_partition_info_t*p=&parts[(*count)++];for(uint32_t n=0;n<sizeof(*p);n++)((uint8_t*)p)[n]=0;
        p->scheme=BLOCK_PARTITION_SCHEME_GPT;p->index=i+1;p->start_lba=begin;p->block_count=end-begin+1;
        for(uint32_t n=0;n<16;n++){p->type_guid[n]=entry[n];p->unique_guid[n]=entry[16+n];}
        uint32_t label=0;for(uint32_t n=0;n<36&&label+1<BLOCK_PARTITION_LABEL_MAX;n++){uint16_t ch=(uint16_t)entry[56+n*2]|((uint16_t)entry[57+n*2]<<8);if(!ch)break;p->label[label++]=ch>=32&&ch<=126?(char)ch:'?';}p->label[label]=0;
    }
    kfree(table);return 0;
}

static int parse_gpt(block_device_t*disk,block_partition_info_t*parts,uint32_t*count){
    if(parse_gpt_at(disk,1,parts,count)==0)return 0;
    /* The final logical block is the standardized recovery-header location.
       It is independently CRC/range validated before any entry is exposed. */
    return parse_gpt_at(disk,disk->block_count-1,parts,count);
}

static int inspect(block_device_t*disk,block_partition_info_t*parts,uint32_t*count){
    int protective=0;int mbr=parse_mbr(disk,parts,count,&protective);if(mbr<=0)return mbr;if(protective)return parse_gpt(disk,parts,count);return 0;
}

static int partition_read(block_device_t*device,uint64_t lba,uint64_t count,void*buffer){partition_private_t*p=device?device->private_data:0;if(!p||!count||lba>=p->info.block_count||count>p->info.block_count-lba)return-1;return block_read(p->parent,p->info.start_lba+lba,count,buffer);}
static int partition_write(block_device_t*device,uint64_t lba,uint64_t count,const void*buffer){partition_private_t*p=device?device->private_data:0;if(!p||!p->parent->write||!count||lba>=p->info.block_count||count>p->info.block_count-lba)return-1;return block_write(p->parent,p->info.start_lba+lba,count,buffer);}
static int partition_flush(block_device_t*device){partition_private_t*p=device?device->private_data:0;return p?block_flush(p->parent):-1;}

static int register_partition(block_device_t*parent,const block_partition_info_t*info){
    block_device_t*device=kmalloc(sizeof(*device));partition_private_t*private=kmalloc(sizeof(*private));if(!device||!private){if(device)kfree(device);if(private)kfree(private);return-1;}
    for(uint32_t i=0;i<sizeof(*device);i++)((uint8_t*)device)[i]=0;private->parent=parent;private->info=*info;
    uint32_t at=0;for(;parent->name[at]&&at<26;at++)device->name[at]=parent->name[at];device->name[at++]='p';
    char digits[10];uint32_t digit_count=0,index=info->index;do{digits[digit_count++]=(char)('0'+index%10);index/=10;}while(index&&digit_count<sizeof(digits));
    while(digit_count&&at+1<sizeof(device->name))device->name[at++]=digits[--digit_count];device->name[at]=0;
    device->type=BLOCK_TYPE_PARTITION;device->block_size=parent->block_size;device->block_count=info->block_count;device->read=partition_read;device->write=parent->write?partition_write:0;device->flush=parent->flush?partition_flush:0;device->private_data=private;device->parent=parent;
    device->online=parent->online;block_set_identity(device,parent->model,parent->serial);
    if(block_register(device)){kfree(private);kfree(device);return-1;}return 0;
}

int block_scan_partitions(block_device_t*disk){
    if(!disk||disk->type==BLOCK_TYPE_PARTITION)return-1;
    block_partition_info_t*parts=kmalloc(sizeof(*parts)*PARTITION_LIMIT);if(!parts)return-1;uint32_t count=0;
    if(inspect(disk,parts,&count)){kfree(parts);return-1;}
    for(uint32_t i=0;i<count;i++)if(register_partition(disk,&parts[i])){kfree(parts);return-1;}kfree(parts);return(int)count;
}
int block_partition_info(block_device_t*device,block_partition_info_t*result){if(!device||device->type!=BLOCK_TYPE_PARTITION||!result)return-1;*result=((partition_private_t*)device->private_data)->info;return 0;}
block_device_t*block_find_partition_by_type(block_device_t*parent,const uint8_t type_guid[16]){if(!parent||!type_guid)return 0;
    for(block_device_t*d=block_first_device();d;d=d->next){if(d->type!=BLOCK_TYPE_PARTITION||d->parent!=parent)continue;partition_private_t*p=d->private_data;if(p->info.scheme==BLOCK_PARTITION_SCHEME_GPT&&equal(p->info.type_guid,type_guid,16))return d;}return 0;
}

typedef struct {
    uint8_t sector[512];
    uint8_t entries[512];
    uint8_t pattern[512];
    uint8_t readback[512];
    block_partition_info_t parts[PARTITION_LIMIT];
} partition_test_workspace_t;

static int partition_self_test_run(partition_test_workspace_t*work){
    block_device_t*ram=block_find("ram0");if(!ram||ram->block_size!=512||ram->block_count<256)return-1;uint8_t*sector=work->sector;block_partition_info_t*parts=work->parts;uint32_t count=0;
    for(uint32_t i=0;i<512;i++)sector[i]=0;
    sector[446+4]=0x83;write32(sector+446+8,64);write32(sector+446+12,128);sector[510]=0x55;sector[511]=0xaa;if(block_write(ram,0,1,sector)||inspect(ram,parts,&count)||count!=1||parts[0].scheme!=BLOCK_PARTITION_SCHEME_MBR||parts[0].start_lba!=64||parts[0].block_count!=128)return-1;
    sector[462+4]=0x07;write32(sector+462+8,128);write32(sector+462+12,64);
    if(block_write(ram,0,1,sector)||inspect(ram,parts,&count)==0)return-1;
    for(uint32_t i=0;i<512;i++)sector[i]=0;sector[446+4]=0xee;write32(sector+446+8,1);write32(sector+446+12,(uint32_t)(ram->block_count-1));sector[510]=0x55;sector[511]=0xaa;if(block_write(ram,0,1,sector))return-1;
    uint8_t*entries=work->entries;for(uint32_t i=0;i<512;i++)entries[i]=0;for(uint32_t i=0;i<16;i++){entries[i]=OBSIDIA_STATE_PARTITION_TYPE_GUID[i];entries[16+i]=(uint8_t)(i+1);}write64(entries+32,64);write64(entries+40,191);const char*label="State Test";for(uint32_t i=0;label[i];i++)entries[56+i*2]=(uint8_t)label[i];if(block_write(ram,2,1,entries))return-1;
    for(uint32_t i=0;i<512;i++)sector[i]=0;const char*signature="EFI PART";for(uint32_t i=0;i<8;i++)sector[i]=(uint8_t)signature[i];write32(sector+8,0x00010000);write32(sector+12,92);write64(sector+24,1);write64(sector+32,ram->block_count-1);write64(sector+40,34);write64(sector+48,ram->block_count-34);write64(sector+72,2);write32(sector+80,4);write32(sector+84,128);write32(sector+88,crc32(entries,512));write32(sector+16,crc32(sector,92));if(block_write(ram,1,1,sector))return-1;
    if(block_write(ram,ram->block_count-2,1,entries))return-1;for(uint32_t i=0;i<512;i++)sector[i]=0;for(uint32_t i=0;i<8;i++)sector[i]=(uint8_t)signature[i];write32(sector+8,0x00010000);write32(sector+12,92);write64(sector+24,ram->block_count-1);write64(sector+32,1);write64(sector+40,34);write64(sector+48,ram->block_count-34);write64(sector+72,ram->block_count-2);write32(sector+80,4);write32(sector+84,128);write32(sector+88,crc32(entries,512));write32(sector+16,crc32(sector,92));if(block_write(ram,ram->block_count-1,1,sector))return-1;
    if(inspect(ram,parts,&count)||count!=1||parts[0].scheme!=BLOCK_PARTITION_SCHEME_GPT||!equal(parts[0].type_guid,OBSIDIA_STATE_PARTITION_TYPE_GUID,16))return-1;
    if(block_read(ram,1,1,sector))return-1;sector[16]^=0x80;if(block_write(ram,1,1,sector)||inspect(ram,parts,&count)||count!=1)return-1;
    if(block_read(ram,ram->block_count-1,1,sector))return-1;sector[16]^=0x80;if(block_write(ram,ram->block_count-1,1,sector)||inspect(ram,parts,&count)==0)return-1;
    sector[16]^=0x80;if(block_write(ram,ram->block_count-1,1,sector)||block_read(ram,1,1,sector))return-1;sector[16]^=0x80;if(block_write(ram,1,1,sector))return-1;
    if(block_scan_partitions(ram)!=1)return-1;block_device_t*volume=block_find_partition_by_type(ram,OBSIDIA_STATE_PARTITION_TYPE_GUID);if(!volume||volume->block_count!=128)return-1;
    uint8_t*pattern=work->pattern,*readback=work->readback;for(uint32_t i=0;i<512;i++)pattern[i]=(uint8_t)(i^0xa5);if(block_write(volume,0,1,pattern)||block_read(ram,64,1,readback)||!equal(pattern,readback,512)||block_read(volume,128,1,readback)==0)return-1;return 0;
}

int block_partition_self_test(void){
    partition_test_workspace_t*work=kmalloc(sizeof(*work));if(!work)return-1;int result=partition_self_test_run(work);kfree(work);return result;
}
