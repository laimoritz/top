//*********************************************************************
// Copyright 2010, Institute of Computer and Network Engineering,
//                 TU-Braunschweig
// All rights reserved
// Any reproduction, use, distribution or disclosure of this program,
// without the express, prior written consent of the authors is 
// strictly prohibited.
//
// University of Technology Braunschweig
// Institute of Computer and Network Engineering
// Hans-Sommer-Str. 66
// 38118 Braunschweig, Germany
//
// ESA SPECIAL LICENSE
//
// This program may be freely used, copied, modified, and redistributed
// by the European Space Agency for the Agency's own requirements.
//
// The program is provided "as is", there is no warranty that
// the program is correct or suitable for any purpose,
// neither implicit nor explicit. The program and the information in it
// contained do not necessarily reflect the policy of the 
// European Space Agency or of TU-Braunschweig.
//*********************************************************************
// Title:      localram.cpp
//
// ScssId:
//
// Origin:     HW-SW SystemC Co-Simulation SoC Validation Platform
//
// Purpose:    Implementation of a local RAM that
//             can be attached to the icache and dcache controllers.
//             The LocalRAM enables fast 0-waitstate access
//             to instructions or data.
//
// Method:
//
// Modified on $Date$
//          at $Revision$
//          by $Author$
//
// Principal:  European Space Agency
// Author:     VLSI working group @ IDA @ TUBS
// Maintainer: Thomas Schuster
// Reviewed:
//*********************************************************************

#include "localram.h"
#include "verbose.h"

/// constructor
localram::localram(sc_core::sc_module_name name, unsigned int lrsize,
                   unsigned int lrstart) :
                   sc_module(name), 
		   m_lrsize(lrsize<<10), 
		   m_lrstart(lrstart << 24),
		   sreads(0),
		   swrites(0),
		   sreads_byte(0),
		   swrites_byte(0)
{

    // Parameter check
    // ---------------
    // Scratchpad size max 512 kbyte
    assert(m_lrsize <= 524288);

    // Initialize allocator
    m_default_entry.i = 0;

    // Create the actual ram
    scratchpad = new t_cache_data[m_lrsize>>2];

    // Configuration report
    v::info << this->name() << " ******************************************************************************* " << v::endl;
    v::info << this->name() << " * Created localram with following parameters:                                   " << v::endl;
    v::info << this->name() << " * ------------------------------------------- " << v::endl;
    v::info << this->name() << " * lrstart (start address): " << std::hex << m_lrstart << v::endl;
    v::info << this->name() << " * lrsize: " << std::hex << m_lrsize  << " bytes" << v::endl;
    v::info << this->name() << " ******************************************************************************* "  << v::endl;

}

// Destructor
localram::~localram() {

    // free the memory
    delete (scratchpad);

}

// Read from scratchpad
bool localram::mem_read(unsigned int addr, unsigned int asi, unsigned char *data, unsigned int len,
			sc_core::sc_time *delay, unsigned int *debug, bool is_dbg) {

  if(!((addr - m_lrstart) < m_lrsize)) {

    v::error << name() << "Read with address " << hex << addr << " out of range!!" << v::endl;

  }

  // Byte offset
  unsigned int byt = addr & 0x3;

  // Copy data to payload pointer
  for (unsigned int i = 0; i < len; i++) {
    *(data + i) = scratchpad[(addr - m_lrstart) >> 2].c[byt + i];
    sreads_byte++;
  }

  v::debug << this->name() << "Read from address: " << std::hex << addr << v::endl;

  // Increment read counter (statistics)
  sreads++;

  // Update debug information
  SCRATCHPAD_SET(*debug);

  *delay = clockcycle;

  return true;

}

// Write to scratchpad
void localram::mem_write(unsigned int addr, unsigned int asi, unsigned char *data, unsigned int len,
			 sc_core::sc_time *delay, unsigned int *debug, bool is_dbg) {

  if(!((addr - m_lrstart) < m_lrsize)) {

    v::error << name() << "Write with address " << hex << addr << " out of range!!" << v::endl;

  }

  // byte offset
  unsigned int byt = addr & 0x3;

  // memcpy ??
  for (unsigned int i = 0; i < len; i++) {
    scratchpad[(addr - m_lrstart) >> 2].c[byt + i] = *(data + i);
    swrites_byte++;
  }

  v::debug << this->name() << "Write to address: " << std::hex << addr << v::endl;

  // Increment write counter (statistics)
  swrites++;

  // update debug information
  SCRATCHPAD_SET(*debug);
  
  *delay = clockcycle;

}

// Print execution statistic at end of simulation
void localram::end_of_simulation() {

  // Localram execution statistic
  v::report << name() << " ******************************************************* " << v::endl;
  v::report << name() << " * Scratchpad statisitics: " << v::endl;
  v::report << name() << " * -----------------------" << v::endl;
  v::report << name() << " * Read accesses:  " << sreads  << " (Bytes: " << sreads_byte << ")" << v::endl;
  v::report << name() << " * Write accesses: " << swrites << " (Bytes: " << swrites_byte << ")" << v::endl;
  v::report << name() << " ******************************************************* " << v::endl;

}

// Helper for setting clock cycle latency using sc_clock argument
void localram::clkcng(sc_core::sc_time &clk) {
  clockcycle = clk;
}
