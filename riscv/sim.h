// See LICENSE for license details.

#ifndef _RISCV_SIM_H
#define _RISCV_SIM_H

#include "cfg.h"
#include "debug_module.h"
#include "devices.h"
#include "log_file.h"
#include "processor.h"
#include "simif.h"

#include <fesvr/htif.h>
#include <list>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <sys/types.h>

class mmu_t;
class remote_bitbang_t;
class socketif_t;

// this class encapsulates the processors and memory in a RISC-V machine.
class sim_t : public htif_t, public simif_t
{
public:
  sim_t(const cfg_t *cfg, bool halted,
        std::vector<std::pair<reg_t, abstract_mem_t*>> mems,
        std::vector<const device_factory_t*> plugin_device_factories,
        const std::vector<std::string>& args,
        const debug_module_config_t &dm_config, const char *log_path,
        bool dtb_enabled, const char *dtb_file,
        bool socket_enabled,
        FILE *cmd_file); // needed for command line option --cmd
  ~sim_t();

  // run the simulation to completion
  int run();
  void set_debug(bool value);
  void set_histogram(bool value);
  void add_device(reg_t addr, std::shared_ptr<abstract_device_t> dev);

  // Configure logging
  //
  // If enable_log is true, an instruction trace will be generated. If
  // enable_commitlog is true, so will the commit results
  void configure_log(bool enable_log, bool enable_commitlog);

  void set_procs_debug(bool value);
  void set_remote_bitbang(remote_bitbang_t* remote_bitbang) {
    this->remote_bitbang = remote_bitbang;
  }
  const char* get_dts() { return dts.c_str(); }
  processor_t* get_core(size_t i) { return procs.at(i); }
  abstract_interrupt_controller_t* get_intctrl() const { assert(plic.get()); return plic.get(); }
  virtual const cfg_t &get_cfg() const override { return *cfg; }

  virtual const std::map<size_t, processor_t*>& get_harts() const override { return harts; }

  // Callback for processors to let the simulation know they were reset.
  virtual void proc_reset(unsigned id) override;

  static const size_t INTERLEAVE = 5000;
  static const size_t INSNS_PER_RTC_TICK = 100; // 10 MHz clock for 1 BIPS core
  static const size_t CPU_HZ = 1000000000; // 1GHz CPU

private:
  isa_parser_t isa;
  const cfg_t * const cfg;
  std::vector<std::pair<reg_t, abstract_mem_t*>> mems;
  std::vector<processor_t*> procs;
  std::map<size_t, processor_t*> harts;
  std::pair<reg_t, reg_t> initrd_range;
  std::string dts;
  std::string dtb;
  bool dtb_enabled;
  std::vector<std::shared_ptr<abstract_device_t>> devices;
  std::shared_ptr<clint_t> clint;
  std::shared_ptr<plic_t> plic;
  bus_t bus;
  log_file_t log_file;

  FILE *cmd_file; // pointer to debug command input file

  socketif_t *socketif;
  std::ostream sout_; // used for socket and terminal interface

  processor_t* get_core(const std::string& i);
  void step(size_t n); // step through simulation
  size_t current_step;
  size_t current_proc;
  bool debug;
  bool histogram_enabled; // provide a histogram of PCs
  bool log;
  remote_bitbang_t* remote_bitbang;
  std::optional<std::function<void()>> next_interactive_action;

  // memory-mapped I/O routines
  virtual char* addr_to_mem(reg_t paddr) override;
  virtual bool mmio_load(reg_t paddr, size_t len, uint8_t* bytes) override;
  virtual bool mmio_store(reg_t paddr, size_t len, const uint8_t* bytes) override;
  void set_rom();

  virtual const char* get_symbol(uint64_t paddr) override;

  // presents a prompt for introspection into the simulation
  void interactive();

  // functions that help implement interactive()
  void interactive_help(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_quit(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_run(const std::string& cmd, const std::vector<std::string>& args, bool noisy);
  void interactive_run_noisy(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_run_silent(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_vreg(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_reg(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_freg(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_fregh(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_fregs(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_fregd(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_pc(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_priv(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_mem(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_str(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_dumpmems(const std::string& cmd, const std::vector<std::string>& args);
  
  struct reg_info_mem{
    std::list<std::pair<reg_t,char*>>::iterator page;
    uint64_t base;
    uint32_t* inst_page;
    uint64_t* data_page;
    int inst_index;
    int data_index;
    static const int inst_page_len=2800;
    static const int inst_end=inst_page_len/4;
    static const int data_end=(4096-inst_page_len)/8;
    static uint64_t mem_base;
    reg_info_mem(std::list<std::pair<reg_t,char*>>::iterator p){
      page=p;
      inst_index=0;
      data_index=0;
      base=p->first+mem_base;
      inst_page=(uint32_t*)p->second;
      data_page=(uint64_t*)(&p->second[inst_page_len]);
    }
    int inst_remain(){return inst_end-inst_index;}
    int data_remain(){return data_end-data_index;}
    void write_inst(uint32_t inst){inst_page[inst_index++]=inst;}
    void write_data(uint64_t data){data_page[data_index++]=data;}
    uint64_t get_data_addr(){return (uint64_t)((char*)&data_page[data_index]-(char*)&inst_page[0])+base;}
    bool write_li(int regindex,uint64_t imm);
    bool write_jal_abs(uint64_t abs);
    bool write_mv_csr(int csrindex,uint64_t imm);
    bool write_store(uint64_t addr,uint64_t imm);
  };
  void interactive_dumpallinfo(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_dumpmeminfo(const std::string& cmd, const std::vector<std::string>& args);
  void dump_memlist(std::list<std::pair<reg_t,char*>>& mem_list,std::ofstream& mem_file);
  void add_reg_info(std::list<std::pair<reg_t,char*>>& mem_list,std::list<char*>& free_list);
  reg_info_mem find_useless_page(std::list<std::pair<reg_t,char*>>::iterator begin,\
    std::list<std::pair<reg_t,char*>>& mem_list,std::list<char*>& free_list);
  void dump_regfile(int regindex,uint64_t imm,reg_info_mem& p,std::list<std::pair<reg_t,char*>>& mem_list,std::list<char*>& free_list);
  void dump_csrfile(int csrindex,uint64_t imm,reg_info_mem& p,std::list<std::pair<reg_t,char*>>& mem_list,std::list<char*>& free_list);

  void interactive_mtime(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_mtimecmp(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_until(const std::string& cmd, const std::vector<std::string>& args, bool noisy);
  void interactive_until_silent(const std::string& cmd, const std::vector<std::string>& args);
  void interactive_until_noisy(const std::string& cmd, const std::vector<std::string>& args);
  reg_t get_reg(const std::vector<std::string>& args);
  freg_t get_freg(const std::vector<std::string>& args, int size);
  reg_t get_mem(const std::vector<std::string>& args);
  reg_t get_pc(const std::vector<std::string>& args);

  friend class processor_t;
  friend class mmu_t;

  // htif
  virtual void reset() override;
  virtual void idle() override;
  virtual void read_chunk(addr_t taddr, size_t len, void* dst) override;
  virtual void write_chunk(addr_t taddr, size_t len, const void* src) override;
  virtual size_t chunk_align() override { return 8; }
  virtual size_t chunk_max_size() override { return 8; }
  virtual endianness_t get_target_endianness() const override;

public:
  // Initialize this after procs, because in debug_module_t::reset() we
  // enumerate processors, which segfaults if procs hasn't been initialized
  // yet.
  debug_module_t debug_module;
};

extern volatile bool ctrlc_pressed;

#endif
