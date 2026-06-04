#include <common.h>
#include <elf.h> 
#include <stdio.h>
#include <stdlib.h>

#define MAXSIZE 1024

extern char *elf_file; 

typedef struct {
    char name[64]; 
    paddr_t addr;  
    uint32_t size;
} SymbolEntry;

SymbolEntry symbol_table[MAXSIZE]; 
int nr_sym = 0; 

void init_ftrace() {
    if (elf_file == NULL) {
        Log("No ELF file given, ftrace disabled.");
        return;
    }

    FILE *fp = fopen(elf_file, "rb");
    Assert(fp, "Can not open '%s'", elf_file);

    Log("Start parsing ELF file for ftrace: %s", elf_file);

// 1. 读取 ELF Header (拿封面)
    Elf32_Ehdr ehdr;
    fseek(fp, 0, SEEK_SET);
    if (fread(&ehdr, sizeof(Elf32_Ehdr), 1, fp) != 1) {
        Assert(0, "Fail to read ELF Header");
    }

    // 校验 Magic Number，确保传进来的真的是 ELF 文件
    if (*(uint32_t *)ehdr.e_ident != 0x464c457f) {
        Assert(0, "The given file is NOT a valid ELF file!");
    }

    // 2. 遍历 Section Headers，寻找符号表和字符串表 (翻目录)
    uint32_t symtab_offset = 0, symtab_size = 0;
    uint32_t strtab_offset = 0, strtab_size = 0;

    Elf32_Shdr shdr;
    fseek(fp, ehdr.e_shoff, SEEK_SET); // 跳到节头表起始位置
    
    for (int i = 0; i < ehdr.e_shnum; i++) {
        if (fread(&shdr, sizeof(Elf32_Shdr), 1, fp) != 1) {
            Assert(0, "Fail to read Section Header");
        }
        
        if (shdr.sh_type == SHT_SYMTAB) {
            symtab_offset = shdr.sh_offset;
            symtab_size = shdr.sh_size;
        } else if (shdr.sh_type == SHT_STRTAB && i != ehdr.e_shstrndx) {
            // 注意：一定要排除掉段表字符串表 (e_shstrndx)
            strtab_offset = shdr.sh_offset;
            strtab_size = shdr.sh_size;
        }
    }

    // 检查是否成功找到了两张表
    if (symtab_offset == 0 || strtab_offset == 0) {
        Log("Warning: Can not find .symtab or .strtab in this ELF file.");
        fclose(fp);
        return;
    }

    // 3. 提取字符串大字典 (把字典吸进内存)
    // 根据字典大小分配一块内存，记得最后要 free 掉
    char *strtab_data = (char *)malloc(strtab_size);
    Assert(strtab_data, "Fail to malloc memory for string table");
    
    fseek(fp, strtab_offset, SEEK_SET);
    if (fread(strtab_data, 1, strtab_size, fp) != strtab_size) {
        Assert(0, "Fail to read string table data");
    }

    // 4. 遍历符号表，把函数登记到你的小本本里
    int sym_count = symtab_size / sizeof(Elf32_Sym); // 计算一共有多少个符号
    Elf32_Sym sym;
    fseek(fp, symtab_offset, SEEK_SET);

    for (int i = 0; i < sym_count; i++) {
        if (fread(&sym, sizeof(Elf32_Sym), 1, fp) != 1) {
            Assert(0, "Fail to read Symbol Entry");
        }

        // 核心过滤：判断这个符号是不是函数 (STT_FUNC)
        if (ELF32_ST_TYPE(sym.st_info) == STT_FUNC) {
            if (nr_sym >= MAXSIZE) { // 防止越界
                Log("Warning: Symbol table is full, truncating...");
                break;
            }
            
            // 记录地址和大小
            symbol_table[nr_sym].addr = sym.st_value;
            symbol_table[nr_sym].size = sym.st_size;
            
            // 查字典拿名字：通过字典基地址加上偏移量 sym.st_name 得到字符串指针
            // 使用 strncpy 保证安全，防止越界覆盖
            strncpy(symbol_table[nr_sym].name, strtab_data + sym.st_name, 63);
            symbol_table[nr_sym].name[63] = '\0'; // 强制封口
            
            nr_sym++;
        }
    }

    // 打扫战场：大字典已经被我们抄到小本本里了，可以释放内存了
    free(strtab_data);

    // 汇报战果
    Log("Loaded %d functions from ELF successfully!", nr_sym);
    
    // 【调试技巧】: 取消下面这段注释，可以亲眼看看你解析出了哪些函数
    printf("========== FTRACE SYMBOL TABLE ==========\n");
    for (int i = 0; i < nr_sym; i++) {
        printf("[FTRACE] Addr: 0x%08x - 0x%08x | Size: %4d | Name: %s\n", 
               symbol_table[i].addr, 
               symbol_table[i].addr + symbol_table[i].size, 
               symbol_table[i].size,
               symbol_table[i].name);
    }
    printf("=========================================\n");

    fclose(fp);
}

// 新增一个变量来记录调用层级（为了打印出好看的缩进树状图）
static int call_depth = 0;

// 辅助函数：给定一个地址，去小本本里查它属于哪个函数
static char* get_func_name(paddr_t addr) {
    for (int i = 0; i < nr_sym; i++) {
        // 如果该地址落在某个函数的 [起始地址, 起始地址 + 大小) 范围内
        if (addr >= symbol_table[i].addr && addr < symbol_table[i].addr + symbol_table[i].size) {
            return symbol_table[i].name;
        }
    }
    return "???"; // 没查到
}

// 暴露给 CPU 的追踪接口
// pc: 当前指令的地址
// dnpc: 即将跳转去的目标地址
// type: 1 代表 Call， 2 代表 Ret
void ftrace_record(paddr_t pc, paddr_t dnpc, int type) {
    if (elf_file == NULL) return; // 没传 elf 就不追踪

    if (type == 1) { // 函数调用 (Call)
        char *func_name = get_func_name(dnpc); // Call 的时候，看我们要去哪个函数
        printf("[FTRACE] 0x%08x: ", pc);
        for (int i = 0; i < call_depth; i++) printf("  "); // 打印缩进
        printf("call [%s@0x%08x]\n", func_name, dnpc);
        call_depth++;
    } 
    else if (type == 2) { // 函数返回 (Ret)
        call_depth--;
        if (call_depth < 0) call_depth = 0; // 防止溢出
        char *func_name = get_func_name(pc); // Ret 的时候，看我们正从哪个函数离开
        printf("[FTRACE] 0x%08x: ", pc);
        for (int i = 0; i < call_depth; i++) printf("  "); // 打印缩进
        printf("ret  [%s]\n", func_name);
    }
}