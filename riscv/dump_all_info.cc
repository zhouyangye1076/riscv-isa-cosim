#include "sim.h"
#include <iomanip>
void sim_t::dump_memlist(std::list<std::pair<reg_t,char*>>& mem_list,std::ofstream& mem_file){
    int last_page=0;
    for(auto p=mem_list.begin();p!=mem_list.end();p++){
        if(p->first-4096 != last_page){
            mem_file << "@" << std::setw(16) << std::setfill('0') << std::hex << (p->first/8) << std::endl;
        }
        last_page = p->first;
        for(int i=0;i<4096;i+=8){
            char* c = &(p->second[i]);
            uint64_t data=0;
            for(int j=7;j>=0;j--){
                data<<=8;
                data+=(uint8_t)c[j];
            }
            mem_file << std::setw(16) << std::setfill('0') << std::hex << data << std::endl;
        }
    }
}

void sim_t::dump_register(){
    std::ofstream reg_state("reginfo.bin",std::ios_base::binary);
    processor_t *core = get_core(0);
    int csr_index[]={
        CSR_SSTATUS,
        CSR_SIE,
        CSR_STVEC,
        CSR_SCOUNTEREN,
        CSR_SSCRATCH,
        CSR_SATP,
        CSR_MISA,
        CSR_MEDELEG,
        CSR_MIDELEG,
        CSR_MIE,
        CSR_MEPC,
        CSR_MTVEC,
        CSR_MCOUNTEREN,
        CSR_PMPCFG0,
        CSR_PMPCFG2,
        CSR_PMPADDR0, 
        CSR_PMPADDR1, 
        CSR_PMPADDR2, 
        CSR_PMPADDR3, 
        CSR_PMPADDR4, 
        CSR_PMPADDR5, 
        CSR_PMPADDR6, 
        CSR_PMPADDR7, 
        CSR_PMPADDR8, 
        CSR_PMPADDR9, 
        CSR_PMPADDR10,
        CSR_PMPADDR11,
        CSR_PMPADDR12,
        CSR_PMPADDR13,
        CSR_PMPADDR14,
        CSR_PMPADDR15
    };
    static const char* csr_name[]={
        "sstatus",
        "sie",
        "stvec",
        "scounteren",
        "sscratch",
        "satp",
        "misa",
        "medeleg",
        "medeleg",
        "mie",
        "mepc",
        "mtvec",
        "mcounteren",
        "pmpcfg0",
        "pmpcfg2",
        "pmpaddr0",
        "pmpaddr1",
        "pmpaddr2",
        "pmpaddr3",
        "pmpaddr4",
        "pmpaddr5",
        "pmpaddr6",
        "pmpaddr7",
        "pmpaddr8",
        "pmpaddr9",
        "pmpaddr10",
        "pmpaddr11",
        "pmpaddr12",
        "pmpaddr13",
        "pmpaddr14",
        "pmpaddr15",
    };
    for(size_t i=0;i<(sizeof(csr_index)/sizeof(csr_index[0]));i++){
        if(core->get_state()->csrmap.find(csr_index[i])==core->get_state()->csrmap.end()){
            std::printf("no csr %d\n",csr_index[i]);
            continue;
        }
        reg_t csr_val=(core->get_state()->csrmap[csr_index[i]])->read();
        std::printf("dump csr %s:\t%016llx\n",csr_name[i],csr_val);
        reg_state.write((char*)&csr_val,sizeof(csr_val));
    }
    
    reg_t mstatus=core->get_state()->mstatus->read();
    reg_t priv=core->get_state()->prv;
    mstatus=((mstatus&~((uint64_t)0b11<<11))|((priv&0b11)<<11));
    mstatus|=(mstatus&(0b1<<3))<<4;
    reg_state.write((char*)&mstatus,sizeof(mstatus));
    std::printf("dump mstatus:\t%016llx\n",mstatus);
    std::printf("set the MPP to priv,set the MIE to PMIE\n");

    reg_t pc=core->get_state()->pc;
    reg_state.write((char*)&pc,sizeof(pc));
    std::printf("dump pc:\t%016llx\n",pc);
    std::printf("set the PC to MEPC\n");

    for(int i=1;i<32;i++){
        reg_t reg=core->get_state()->XPR[i];
        std::printf("dump reg %d:\t%016llx\n",i,reg);
        reg_state.write((char*)&reg,sizeof(reg));
    }

    reg_state.close();
}