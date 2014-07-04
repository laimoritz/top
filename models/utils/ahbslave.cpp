// vim : set fileencoding=utf-8 expandtab noai ts=4 sw=4 :
/// @addtogroup utils
/// @{
/// @file ahbslave.cpp
///
///
/// @date 2010-2014
/// @copyright All rights reserved.
///            Any reproduction, use, distribution or disclosure of this
///            program, without the express, prior written consent of the
///            authors is strictly prohibited.
/// @author Thomas Schuster
///

#include "models/utils/ahbslave.h"

#ifndef MTI_SYSTEMC
// #include <greensocket/initiator/multi_socket.h>
#include <greenreg_ambasockets.h>
#endif

// Constructor
template<>
AHBSlave<sc_core::sc_module>::AHBSlave(
    sc_core::sc_module_name nm,
    uint8_t hindex,
    uint8_t vendor,
    uint8_t device,
    uint8_t version,
    uint8_t irq,
    amba::amba_layer_ids ambaLayer,
    uint32_t bar0,
    uint32_t bar1,
    uint32_t bar2,
    uint32_t bar3) :
  sc_module(nm),
  AHBDevice(hindex,
    vendor,
    device,
    version,
    irq,
    bar0,
    bar1,
    bar2,
    bar3),
  ahb("ahb", ::amba::amba_AHB, ambaLayer, false  /* Arbitration */),
  m_RequestPEQ("RequestPEQ"),
  m_ResponsePEQ("ResponsePEQ"),
  busy(false),
  m_performance_counters("performance_counters"),
  m_reads("bytes_read", 0llu, m_performance_counters),
  m_writes("bytes_written", 0llu, m_performance_counters) {
  // Register transport functions to sockets
  ahb.register_b_transport(this, &AHBSlave::b_transport);
  ahb.register_transport_dbg(this, &AHBSlave::transport_dbg);

  if (ambaLayer == amba::amba_AT) {
    // Register non-blocking transport for AT
    ahb.register_nb_transport_fw(this, &AHBSlave::nb_transport_fw);

    // Thread for modeling AHB pipeline delay
    SC_THREAD(requestThread);

    // Thread for interfacing functional part of the model
    // in AT mode.
    SC_THREAD(responseThread);
  }
  sc_module *self = dynamic_cast<sc_module *>(this);
  if (self) {
    m_api = gs::cnf::GCnf_Api::getApiInstance(self);
  } else {
    v::error << name() << "A AHBDevice instance must also inherit from sc_module when it gets instantiated. "
             << "To ensure the performance counter will work correctly" << v::endl;
  }
}

#ifndef MTI_SYSTEMC

template<>
AHBSlave<gs::reg::gr_device>::AHBSlave(
    sc_core::sc_module_name nm,
    uint8_t hindex,
    uint8_t vendor,
    uint8_t device,
    uint8_t version,
    uint8_t irq,
    amba::amba_layer_ids ambaLayer,
    uint32_t bar0,
    uint32_t bar1,
    uint32_t bar2,
    uint32_t bar3) :
  gr_device(nm, gs::reg::ALIGNED_ADDRESS, 16, NULL),
  AHBDevice(hindex,
    vendor,
    device,
    version,
    irq,
    bar0,
    bar1,
    bar2,
    bar3),
  ahb("ahb", ::amba::amba_AHB, ambaLayer, false  /* Arbitration */),
  m_RequestPEQ("RequestPEQ"),
  m_ResponsePEQ("ResponsePEQ"),
  busy(false),
  m_performance_counters("performance_counters"),
  m_reads("bytes_read", 0llu, m_performance_counters), m_writes("bytes_written", 0llu, m_performance_counters) {
  // Register transport functions to sockets
  ahb.register_b_transport(this, &AHBSlave::b_transport);
  ahb.register_transport_dbg(this, &AHBSlave::transport_dbg);

  if (ambaLayer == amba::amba_AT) {
    // Register non-blocking transport for AT
    ahb.register_nb_transport_fw(this, &AHBSlave::nb_transport_fw);

    // Thread for modeling AHB pipeline delay
    SC_THREAD(requestThread);

    // Thread for interfacing functional part of the model
    // in AT mode.
    SC_THREAD(responseThread);
  }
  sc_module *self = dynamic_cast<sc_module *>(this);
  if (self) {
    m_api = gs::cnf::GCnf_Api::getApiInstance(self);
  } else {
    v::error << name() << "A AHBDevice instance must also inherit from sc_module when it gets instantiated. "
             << "To ensure the performance counter will work correctly" << v::endl;
  }
}

#endif
/// @}
