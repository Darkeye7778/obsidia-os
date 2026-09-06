#include "pe.h"
#include "image.h"
#include "../memory/heap.h"

#define PE_SIGNATURE 0x00004550U
#define PE_SECTION_EXECUTE 0x20000000U
#define PE_SECTION_READ    0x40000000U
#define PE_SECTION_WRITE   0x80000000U
#define PE_DIRECTORY_IMPORT 1U
#define PE_DIRECTORY_BASERELOC 5U

extern void serial_write(const char* text);

typedef struct __attribute__((packed)) {
    uint16_t machine, section_count;
    uint32_t timestamp, symbol_table, symbol_count;
    uint16_t optional_size, characteristics;
} coff_header_t;

typedef struct __attribute__((packed)) {
    uint8_t name[8];
    uint32_t virtual_size, virtual_address, raw_size, raw_offset;
    uint32_t relocations, line_numbers;
    uint16_t relocation_count, line_count;
    uint32_t characteristics;
} section_header_t;

static uint16_t u16(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t u32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint64_t u64(const uint8_t* p) { return (uint64_t)u32(p) | ((uint64_t)u32(p+4) << 32); }
static int add_ok(uint64_t a, uint64_t b, uint64_t limit) { return a <= limit && b <= limit-a; }

static int read_exact(vfs_node_t* node, uint64_t offset, void* output, uint64_t size) {
    return add_ok(offset,size,node->size) && vfs_read(node,offset,output,size)==(int64_t)size;
}

int pe_inspect(vfs_node_t* node, pe_image_info_t* info,
               pe_section_info_t* sections, uint32_t capacity) {
    uint8_t dos[64], signature[4], optional[240];
    coff_header_t coff;
    if (!node || !info || node->type != VFS_FILE || node->size < sizeof(dos) ||
        !read_exact(node,0,dos,sizeof(dos)) || dos[0]!='M' || dos[1]!='Z') return 0;
    uint32_t pe_offset=u32(dos+0x3c);
    if (pe_offset < sizeof(dos) || !add_ok(pe_offset,4+sizeof(coff),node->size) ||
        !read_exact(node,pe_offset,signature,sizeof(signature)) || u32(signature)!=PE_SIGNATURE ||
        !read_exact(node,pe_offset+4,&coff,sizeof(coff)) || !coff.section_count ||
        coff.section_count>PE_MAX_SECTIONS || coff.optional_size>sizeof(optional) ||
        coff.optional_size<96 || !read_exact(node,pe_offset+4+sizeof(coff),optional,coff.optional_size)) return 0;
    uint16_t magic=u16(optional);
    uint32_t directory_offset;
    uint64_t image_base;
    if (magic==PE_MAGIC_PE32_PLUS) {
        if (coff.optional_size<112) return 0;
        image_base=u64(optional+24); directory_offset=112;
    } else if (magic==PE_MAGIC_PE32) {
        if (coff.optional_size<96) return 0;
        image_base=u32(optional+28); directory_offset=96;
    } else return 0;
    if ((magic==PE_MAGIC_PE32_PLUS && coff.machine!=PE_MACHINE_AMD64) ||
        (magic==PE_MAGIC_PE32 && coff.machine!=PE_MACHINE_I386)) return 0;
    uint32_t image_size=u32(optional+56), headers_size=u32(optional+60);
    uint32_t section_alignment=u32(optional+32), file_alignment=u32(optional+36);
    uint32_t directory_count=u32(optional+(magic==PE_MAGIC_PE32_PLUS?108:92));
    if (!image_base || !image_size || headers_size>node->size || headers_size>image_size ||
        u32(optional+16)>=image_size ||
        !section_alignment || (section_alignment&(section_alignment-1)) ||
        !file_alignment || (file_alignment&(file_alignment-1)) ||
        directory_count>16 || !add_ok(directory_offset,(uint64_t)directory_count*8,coff.optional_size)) return 0;
    uint64_t table=pe_offset+4+sizeof(coff)+coff.optional_size;
    if (!add_ok(table,(uint64_t)coff.section_count*sizeof(section_header_t),node->size)) return 0;
    *info=(pe_image_info_t){coff.machine,magic,coff.section_count,image_base,image_size,
        headers_size,u32(optional+16),(uint32_t)table,0,0,0,0};
    if (directory_count>PE_DIRECTORY_IMPORT) {
        info->import_rva=u32(optional+directory_offset+PE_DIRECTORY_IMPORT*8);
        info->import_size=u32(optional+directory_offset+PE_DIRECTORY_IMPORT*8+4);
    }
    if (directory_count>PE_DIRECTORY_BASERELOC) {
        info->relocation_rva=u32(optional+directory_offset+PE_DIRECTORY_BASERELOC*8);
        info->relocation_size=u32(optional+directory_offset+PE_DIRECTORY_BASERELOC*8+4);
    }
    for (uint16_t i=0;i<coff.section_count;i++) {
        section_header_t section;
        if (!read_exact(node,table+(uint64_t)i*sizeof(section),&section,sizeof(section))) return 0;
        uint32_t span=section.virtual_size;
        if(span<section.raw_size)span=section.raw_size;
        if ((section.raw_size && !add_ok(section.raw_offset,section.raw_size,node->size)) ||
            section.virtual_address>=image_size ||
            (span && !add_ok(section.virtual_address,span,image_size))) return 0;
        if (sections && i<capacity) {
            for (uint32_t j=0;j<8;j++) sections[i].name[j]=section.name[j];
            sections[i].virtual_size=section.virtual_size;
            sections[i].virtual_address=section.virtual_address;
            sections[i].raw_size=section.raw_size;
            sections[i].raw_offset=section.raw_offset;
            sections[i].characteristics=section.characteristics;
        }
    }
    if ((info->import_rva && (!info->import_size || !add_ok(info->import_rva,info->import_size,image_size))) ||
        (info->import_size && !info->import_rva) ||
        (info->relocation_rva && (!info->relocation_size || !add_ok(info->relocation_rva,info->relocation_size,image_size))) ||
        (info->relocation_size && !info->relocation_rva)) return 0;
    return 1;
}

static int rva_to_file(const pe_image_info_t* info, const pe_section_info_t* sections,
                       uint32_t rva, uint32_t bytes, uint64_t* offset) {
    if (rva < info->headers_size && bytes <= info->headers_size-rva) { *offset=rva; return 1; }
    for (uint16_t i=0;i<info->section_count;i++) {
        uint32_t span=sections[i].virtual_size;
        if (span<sections[i].raw_size) span=sections[i].raw_size;
        if (rva>=sections[i].virtual_address && rva-sections[i].virtual_address<span) {
            uint32_t relative=rva-sections[i].virtual_address;
            if (relative>sections[i].raw_size || bytes>sections[i].raw_size-relative) return 0;
            *offset=(uint64_t)sections[i].raw_offset+relative; return 1;
        }
    }
    return 0;
}

static void report_unsupported_import(vfs_node_t* node, const pe_image_info_t* info,
                                      const pe_section_info_t* sections) {
    uint64_t descriptor_offset;
    uint8_t descriptor[20];
    serial_write("WINABI: import resolution is not implemented");
    if (rva_to_file(info,sections,info->import_rva,sizeof(descriptor),&descriptor_offset) &&
        read_exact(node,descriptor_offset,descriptor,sizeof(descriptor))) {
        uint32_t name_rva=u32(descriptor+12); uint64_t name_offset;
        if (name_rva && rva_to_file(info,sections,name_rva,1,&name_offset)) {
            char name[65]; uint32_t i=0;
            for (;i<64 && name_offset+i<node->size;i++) {
                if (!read_exact(node,name_offset+i,&name[i],1)) break;
                if (!name[i]) break;
            }
            name[i]=0;
            if (i) { serial_write("; first unsupported module: "); serial_write(name); }
        }
    }
    serial_write("\n");
}

int pe_load_process(vfs_node_t* node, uint64_t* cr3_out,
                    uint64_t* entry_out, uint64_t* stack_top_out) {
    pe_image_info_t info;
    pe_section_info_t* sections=kmalloc(PE_MAX_SECTIONS*sizeof(*sections));
    exec_segment_t* segments=kmalloc((PE_MAX_SECTIONS+1)*sizeof(*segments));
    if (!sections || !segments) goto fail;
    if (!pe_inspect(node,&info,sections,PE_MAX_SECTIONS) ||
        info.optional_magic!=PE_MAGIC_PE32_PLUS || info.machine!=PE_MACHINE_AMD64) goto fail;
    if(info.image_base<0x10000ULL||info.image_base>=0x0000800000000000ULL||
       info.image_size>0x0000800000000000ULL-info.image_base)goto fail;
    if (info.import_rva || info.import_size) { report_unsupported_import(node,&info,sections); goto fail; }
    if (info.relocation_rva || info.relocation_size)
        serial_write("WINABI: relocation metadata present; preferred-base mapping selected\n");
    uint32_t count=0;
    if (info.headers_size) segments[count++]=(exec_segment_t){0,info.image_base,
        info.headers_size,info.headers_size,EXEC_SEGMENT_READ};
    for(uint16_t i=0;i<info.section_count;i++) {
        uint64_t memory_size=sections[i].virtual_size;
        if(memory_size<sections[i].raw_size)memory_size=sections[i].raw_size;
        if(!memory_size)continue;
        uint32_t flags=0;
        if(sections[i].characteristics&PE_SECTION_READ)flags|=EXEC_SEGMENT_READ;
        if(sections[i].characteristics&PE_SECTION_WRITE)flags|=EXEC_SEGMENT_WRITE;
        if(sections[i].characteristics&PE_SECTION_EXECUTE)flags|=EXEC_SEGMENT_EXEC;
        segments[count++]=(exec_segment_t){sections[i].raw_offset,
            info.image_base+sections[i].virtual_address,sections[i].raw_size,memory_size,flags};
    }
    uint64_t entry=info.image_base+info.entry_rva;
    if (!exec_map_image(node,segments,count,entry,cr3_out,stack_top_out)) goto fail;
    *entry_out=entry;
    kfree(sections);kfree(segments);return 1;
fail:
    if(sections)kfree(sections);if(segments)kfree(segments);return 0;
}
