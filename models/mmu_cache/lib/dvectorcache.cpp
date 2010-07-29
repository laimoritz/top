/***********************************************************************/
/* Project:    HW-SW SystemC Co-Simulation SoC Validation Platform     */
/*                                                                     */
/* File:       dvectorcache.cpp - Implementation of a data             */
/*             cache. The cache can be configured direct mapped or     */
/*             set associative. Set-size, line-size and replacement    */
/*             strategy can be defined through constructor arguments.  */
/*                                                                     */
/*                                                                     */
/* Modified on $Date$   */
/*          at $Revision$                                         */
/*                                                                     */
/* Principal:  European Space Agency                                   */
/* Author:     VLSI working group @ IDA @ TUBS                         */
/* Maintainer: Thomas Schuster                                         */
/***********************************************************************/

#include "dvectorcache.h"

// constructor
// args: sysc module name, pointer to AHB read/write methods (of parent), delay on read hit, delay on read miss (incr), number of sets, setsize in kb, linesize in b, replacement strategy  
dvectorcache::dvectorcache(sc_core::sc_module_name name, 
			   mmu_cache_if &_parent,
			   mmu_if *_mmu,
			   int mmu_en,
			   sc_core::sc_time dcache_hit_read_response_delay, 
			   sc_core::sc_time dcache_miss_read_response_delay, 
			   sc_core::sc_time dcache_write_response_delay,
			   int sets, 
			   int setsize, 
			   int linesize, 
			   int repl) : sc_module(name),
				       m_sets(sets),
				       m_setsize(setsize),
				       m_linesize(linesize),
				       m_offset_bits((unsigned int)log2(linesize)),
				       m_number_of_vectors((m_setsize << 10) / linesize),
				       m_idx_bits((unsigned int)log2(m_number_of_vectors)),
				       m_tagwidth(32-m_idx_bits-m_offset_bits),
				       m_repl(repl),
				       m_mmu_en(mmu_en),
				       m_dcache_hit_read_response_delay(dcache_hit_read_response_delay),
				       m_dcache_miss_read_response_delay(dcache_miss_read_response_delay),
				       m_dcache_write_response_delay(dcache_write_response_delay)
  
{

  // todo: assertions on parameters

  // initialize cache line allocator
  memset(&m_default_cacheline, 0, sizeof(t_cache_line));

  // create the cache sets
  for (unsigned int i=0; i < m_sets; i++) {

    DUMP(this->name(),"Create cache set " << i);
    std::vector<t_cache_line> *cache_set = new std::vector<t_cache_line>(m_number_of_vectors , m_default_cacheline);

    cache_mem.push_back(cache_set);

    // create one cache_line struct per set
    t_cache_line *current_cacheline = new t_cache_line;
    m_current_cacheline.push_back(current_cacheline);
  }

  DUMP(this->name(), " ******************************************************************************* ");
  DUMP(this->name(), " * Created cache memory with following parameters:                               ");
  DUMP(this->name(), " * number of cache sets " << m_sets);
  DUMP(this->name(), " * size of each cache set " << m_setsize << " kb");
  DUMP(this->name(), " * bytes per line " << m_linesize << " (offset bits: " << m_offset_bits << ")");
  DUMP(this->name(), " * number of cache lines per set " << m_number_of_vectors << " (index bits: " << m_idx_bits << ")");
  DUMP(this->name(), " * Width of cache tag in bits " << m_tagwidth);
  DUMP(this->name(), " * Replacement strategy: " << m_repl);
  DUMP(this->name(), " ******************************************************************************* ");
  
  // set up configuration register
  CACHE_CONFIG_REG = (m_mmu_en << 3);
  // config register contains linesize in words
  CACHE_CONFIG_REG |= ((m_linesize >> 2) << 16);
  CACHE_CONFIG_REG |= (m_setsize << 20);
  CACHE_CONFIG_REG |= (m_sets << 24);
  CACHE_CONFIG_REG |= (m_repl << 28);

  // hook up to top level (amba if)
  m_parent = &_parent;

  // hook up to mmu
  m_mmu = _mmu;
}

// destructor
dvectorcache::~dvectorcache() {

}

// external interface functions
// ----------------------------

// read from cache
void dvectorcache::read(unsigned int address, unsigned int *data, sc_core::sc_time *t) {

  int set_select = -1;
  int cache_hit = -1;

  // todo: handle cached/uncached access
  unsigned int asi = 0;

  // extract index and tag from address
  unsigned int tag    = (address >> (m_idx_bits+m_offset_bits));
  unsigned int idx    = ((address << m_tagwidth) >> (m_tagwidth+m_offset_bits));
  unsigned int offset = ((address << (32-m_offset_bits)) >> (32-m_offset_bits));

  DUMP(this->name(),"READ ACCESS idx: " << std::hex << idx << " tag: " << std::hex << tag << " offset: " << std::hex << offset);

  // lookup all cachesets
  for (unsigned int i=0; i < m_sets; i++){

    m_current_cacheline[i] = lookup(i, idx);

    //DUMP(this->name(), "Set :" << i << " atag: " << (*m_current_cacheline[i]).tag.atag << " valid: " << (*m_current_cacheline[i]).tag.valid << " entry: " << (*m_current_cacheline[i]).entry[offset>>2].i);

    // asi == 1 forces cache miss
    if (asi != 1) {

      // check the cache tag
      if ((*m_current_cacheline[i]).tag.atag == tag) {

        //DUMP(this->name(), "Correct atag found in set " << i);
     
        // check the valid bit (math.h pow is mapped to the coproc, hence it should be pretty fast)
        if ((*m_current_cacheline[i]).tag.valid & (unsigned int)(pow((double)2,(double)(offset >> 2))) != 0) {

          DUMP(this->name(),"Cache Hit in Set " << i);
	
          // write data pointer
          *data = (*m_current_cacheline[i]).entry[offset >> 2].i;
	
          // increment time
          *t+=m_dcache_hit_read_response_delay;

          // valid data in set i
          cache_hit = i;
          break;
        }	
        else {
	
          DUMP(this->name(),"Tag Hit but data not valid in set " << i);
        }
      }
      else {
      
        DUMP(this->name(),"Cache miss in set " << i);
      }
    }
    else {
      
      DUMP(this->name(),"ASI force cache miss");
    
    }
  }
  
  // in case no matching tag was found or data is not valid:
  // -------------------------------------------------------
  // read miss - On a data cache read miss to a cachable location 4 bytes of data
  // are loaded into the cache from main memory.
  if (cache_hit==-1) {

    // increment time
    *t += m_dcache_miss_read_response_delay; 

    // do we have a mmu
    if (m_mmu_en == 1) {

      // mmu enabled: forward request to mmu
      m_mmu->dtlb_read(address, data, 4);
      DUMP(this->name(),"Received data from MMU" << std::hex << *data);

    } else {

      // direct access to ahb interface
      m_parent->amba_read(address, data, 4);
      DUMP(this->name(),"Received data from main memory " << std::hex << *data);
    }

    // !!!! The replacement mechanism still needs to be verified.
    // This is only a first shot.

    // check for unvalid data which can be replaced without harm
    for (unsigned int i=0; i<m_sets; i++){

      if (((*m_current_cacheline[i]).tag.valid & (unsigned int)(pow((double)2,(double)(offset >> 2)))) == 0) {

	// select unvalid data for replacement
	set_select = i;
	DUMP(this->name(), "Set " << set_select << " has no valid data - will use for refill.");
	break;
      }
    }

    // in case there is no free set anymore
    if (set_select == -1) {

      // select set according to replacement strategy
      set_select = replacement_selector(m_repl);
      DUMP(this->name(),"Set " << set_select << " selected for refill by replacement selector.");
      
    }

    // fill in the new data
    (*m_current_cacheline[set_select]).entry[offset >> 2].i = *data;

    // fill in the new atag
    (*m_current_cacheline[set_select]).tag.atag  = tag;
 
    // switch on the valid bit
    (*m_current_cacheline[set_select]).tag.valid |= (unsigned int)(pow((double)2,(double)(offset >> 2)));

    //DUMP(this->name(),"Updated entry: " << std::hex << (*m_current_cacheline[set_select]).entry[offset >> 2].i << " valid bits: " << std::hex << (*m_current_cacheline[set_select]).tag.valid);
  }
}

// write to/through cache:
// -----------------------
// The write policy for stores is write-through with no-allocate on write miss.
// - on hits it writes to cache and main memory;
// - on misses it updates the block in main memory not bringing that block to the cache;
//   Subsequent writes to the block will update main memory because Write Through policy is employed. 
//   So, some time is saved not bringing the block in the cache on a miss because it appears useless anyway.

void dvectorcache::write(unsigned int address, unsigned int * data, unsigned int *byt, sc_core::sc_time * t) {

  // extract index and tag from address
  unsigned int tag    = (address >> (m_idx_bits+m_offset_bits));
  unsigned int idx    = ((address << m_tagwidth) >> (m_tagwidth+m_offset_bits));
  unsigned int offset = ((address << (32-m_offset_bits)) >> (32-m_offset_bits));

  DUMP(this->name(),"WRITE ACCESS with idx: " << std::hex << idx << " tag: " << std::hex << tag << " offset: " << std::hex << offset);

  // lookup all cachesets
  for (unsigned int i=0; i < m_sets; i++){

    m_current_cacheline[i] = lookup(i, idx);

    //DUMP(this->name(), "Set :" << i << " atag: " << (*m_current_cacheline[i]).tag.atag << " valid: " << (*m_current_cacheline[i]).tag.valid << " entry: " << (*m_current_cacheline[i]).entry[offset>>2].i);

    // check the cache tag
    if ((*m_current_cacheline[i]).tag.atag == tag) {

      //DUMP(this->name(),"Correct atag found in set " << i);
     
      // check the valid bit (math.h pow is mapped to the coproc, hence it should be pretty fast)
      if ((*m_current_cacheline[i]).tag.valid & (unsigned int)(pow((double)2,(double)(offset >> 2))) != 0) {

	DUMP(this->name(),"Cache Hit in Set " << i);
	
	// write data to cache (todo: impl. byte access)
	(*m_current_cacheline[i]).entry[offset >> 2].i = *data;
	
	// valid bit is already set
	
	// increment time
	*t+=m_dcache_write_response_delay;

	break;
      }	
      else {
	
	DUMP(this->name(),"Tag Hit but data not valid in set " << i);
      }

    }
    else {
      
      DUMP(this->name(),"Cache miss in set " << i);

    }
  }

  // write data to main memory
  // todo: - implement byte access
  //       - implement write buffer

  // The write buffer (WRB) consists of 3x32bit registers. It is used to temporarily
  // hold store data until it is sent to the destination device. For half-word
  // or byte stores, the data has to be properly aligned for writing to word-
  // addressed device, before writing the WRB.

  // check whether there is a MMU
  if (m_mmu_en == 1) {

    // mmu enabled: forward request to mmu
    m_mmu->dtlb_write(address, data, 4);

  } else {

    // direct access to ahb interface
    m_parent->amba_write(address, data, 4);
  }

}

// call to flush cache
void dvectorcache::flush(sc_core::sc_time *t) {

  unsigned int adr;

  // for all cache sets
  for (unsigned int set=0; set < m_sets; set++){

    // and all cache lines
    for (unsigned int line=0; line<m_number_of_vectors; line++) { 
      
      m_current_cacheline[set] = lookup(set, line);

      // and all cache line entries
      for (unsigned int entry=0; entry < (m_linesize >> 2); entry++) {

	// check for valid data
	if ((*m_current_cacheline[set]).tag.valid & (1 << entry)) {

	  // construct address from tag
	  adr = ((*m_current_cacheline[set]).tag.atag << (m_idx_bits+m_offset_bits));
	  adr |= (line << m_offset_bits);
	  adr |= (entry << 2);

	  DUMP(this->name(),"FLUSH set: " << set << " line: " << line << " addr: " << std::hex << adr << " data: " << std::hex << (*m_current_cacheline[set]).entry[entry].i);

	  // and writeback
	  m_parent->amba_write(adr, &(*m_current_cacheline[set]).entry[entry].i,4);
	  
	}
      }
    }
  }
}

// ------------------------------
// About diagnostic cache access:
// ------------------------------
// Tags and data in the instruction and data cache can be accessed throuch ASI address space
// 0xC, 0xD, 0xE and 0xf by executing LDA and STA instructions. Address bits making up the cache
// offset will be used to index the tag to be accessed while the least significant bits of the
// bits making up the address tag will be used to index the cache set.
//
// In multi-way caches, the address of the tags and data of the ways are concatenated. The address
// of a tag or data is thus:
//
// ADDRESS = WAY & LINE & DATA & "00"
//
// Example: the tag for line 2 in way 1 of a 2x4 Kbyte cache with 16 byte line would be.
// 
// A[13:12] = 1 (WAY); A[11:5] = 2 (TAG) -> TAG Address = 0x1040

// read data cache tags (ASI 0xe)
// -------------------------------------
// Diagnostic read of tags is possible by executing an LDA instruction with ASI = 0xC for 
// instruction cache tags and ASI = 0xe for data cache tags. A cache line and set are indexed by
// the address bits making up the cache offset and the least significant bits of the address bits
// making up the address tag. 

void dvectorcache::read_cache_tag(unsigned int address, unsigned int * data, sc_core::sc_time *t) {

  unsigned int tmp;

  unsigned int set = (address >> (m_idx_bits + 5)); 
  unsigned int idx = ((address << (32 - (m_idx_bits + 5))) >> (32 - m_idx_bits));

  // find the required cache line
  m_current_cacheline[set] = lookup(set, idx);

  // build bitmask from tag fields
  // (! The atag field starts bit 10. It is not MSB aligned as in the actual tag layout.)
  tmp =  (*m_current_cacheline[set]).tag.atag << 10;
  tmp |= (*m_current_cacheline[set]).tag.lrr  << 9;
  tmp |= (*m_current_cacheline[set]).tag.lock << 8;
  tmp |= (*m_current_cacheline[set]).tag.valid;

  DUMP(this->name(),"Diagnostic tag read set: " << std::hex << set << " idx: " << std::hex << idx << " - tag: " << std::hex << tmp);

  // handover bitmask pointer (the tag)
  *data = tmp;

  // increment time
  *t+=m_dcache_hit_read_response_delay;

}


// write data cache tags (ASI 0xe)
// --------------------------------------
// The tags can be directly written by executing a STA instruction with ASI = 0xC for the instruction
// cache tags and ASI = 0xE for the data cache tags. The cache line and set are indexed by the 
// address bits making up the cache offset and the least significant bits of the address bits making
// up the address tag. D[31:10] is written into the ATAG field and the valid bits are written with
// the D[7:0] of the write data. Bit D[9] is written into the LRR bit (if enabled) and D[8] is
// written into the lock bit (if enabled). 
void dvectorcache::write_cache_tag(unsigned int address, unsigned int * data, sc_core::sc_time *t) {

  unsigned int set = (address >> (m_idx_bits + 5)); 
  unsigned int idx = ((address << (32 - (m_idx_bits + 5))) >> (32 - m_idx_bits));

  // find the required cache line
  m_current_cacheline[set] = lookup(set, idx);

  // update the tag with write data
  // (! The atag field is expected to start at bit 10. Not MSB aligned as in tag layout.)
  (*m_current_cacheline[set]).tag.atag  = *data >> 10;
  (*m_current_cacheline[set]).tag.lrr   = ((*data & 0x100) >> 9);
  (*m_current_cacheline[set]).tag.lock  = ((*data & 0x080) >> 8);
  (*m_current_cacheline[set]).tag.valid = (*data & 0xff);

  DUMP(this->name(),"Diagnostic tag write set: " << std::hex << set << " idx: " << std::hex << idx << " atag: " 
       << std::hex << (*m_current_cacheline[set]).tag.atag << " lrr: " << std::hex << (*m_current_cacheline[set]).tag.lrr 
       << " lock: " << std::hex << (*m_current_cacheline[set]).tag.lock << " valid: " << std::hex << (*m_current_cacheline[set]).tag.valid);

  // increment time
  *t+=m_dcache_hit_read_response_delay;

}

// read data cache entry/data (ASI 0xf)
// -------------------------------------------
// Similar to instruction tag read, a data sub-block may be read by executing an LDA instruction
// with ASI = 0xD for instruction cache data and ASI = 0xF for data cache data.
// The sub-block to be read in the indexed cache line and set is selected by A[4:2].
void dvectorcache::read_cache_entry(unsigned int address, unsigned int * data, sc_core::sc_time *t) {

  unsigned int set = (address >> (m_idx_bits + 5)); 
  unsigned int idx = ((address << (32 - (m_idx_bits + 5))) >> (32 - m_idx_bits));
  unsigned int sb  = (address & 0x1f) >> 2;

  // find the required cache line
  m_current_cacheline[set] = lookup(set, idx);

  *data = (*m_current_cacheline[set]).entry[sb].i;

  DUMP(this->name(),"Diagnostic data read set: " << std::hex << set << " idx: " << std::hex << idx
       << " sub-block: " << sb << " - data: " << std::hex << *data);

  // increment time
  *t+=m_dcache_hit_read_response_delay;

}

// write data cache entry/data (ASI 0xd)
// --------------------------------------------
// A data sub-block can be directly written by executing a STA instruction with ASI = 0xD for the
// instruction cache data and ASI = 0xF for the data cache data. The sub-block to be read in 
// indexed cache line and set is selected by A[4:2]. 
void dvectorcache::write_cache_entry(unsigned int address, unsigned int * data, sc_core::sc_time *t) {

  unsigned int set = (address >> (m_idx_bits + 5)); 
  unsigned int idx = ((address << (32 - (m_idx_bits + 5))) >> (32 - m_idx_bits));
  unsigned int sb  = (address & 0x1f) >> 2;

  // find the required cache line
  m_current_cacheline[set] = lookup(set, idx);

  (*m_current_cacheline[set]).entry[sb].i = *data;

  DUMP(this->name(),"Diagnostic data write set: " << std::hex << set << " idx: " << std::hex << idx
       << " sub-block: " << sb << " - data: " << std::hex << *data);

  // increment time
  *t+=m_dcache_hit_read_response_delay;

}

// read cache configuration register
unsigned int dvectorcache::read_config_reg(sc_core::sc_time *t) {

  *t+=m_dcache_hit_read_response_delay;

  return(CACHE_CONFIG_REG);

}


// internal behavioral functions
// -----------------------------

// reads a cache line from a cache set
t_cache_line * dvectorcache::lookup(unsigned int set, unsigned int idx) {

  // return the cache line from the selected set
  return (&(*cache_mem[set])[idx]);

}

unsigned int dvectorcache::replacement_selector(unsigned int mode) {
  
  // random replacement
  if (mode == 0) {

    // todo: check RTL for implementation details
    return(rand() % m_sets);
  } 
  else {

    DUMP(this->name(),"LRU not implemented yet!!");
  }

  return 0;
}


// debug and helper functions
// --------------------------

// displays cache lines at stdout for debug
void dvectorcache::dbg_out(unsigned int line) {

  t_cache_line dbg_cacheline;

  for(unsigned int i=0; i<m_sets;i++) {

    // read the cacheline from set
    dbg_cacheline = (*cache_mem[i])[line];

    // display the tag 
    DUMP(this->name(), "SET: " << i << " ATAG: 0x" << std::hex << dbg_cacheline.tag.atag << " VALID: 0x" << std::hex << dbg_cacheline.tag.valid);

    // display all entries
    for (unsigned int j = 0; j < (m_linesize >> 2); j++) {

      DUMP(this->name(), "Entry: " << j << " - " << std::hex << dbg_cacheline.entry[j].i);

    }
  }
}
  


