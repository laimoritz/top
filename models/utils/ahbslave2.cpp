#include "ahbslave2.h"

using namespace std;
using namespace sc_core;
using namespace tlm;

// Constructor
template<>
AHBSlave<sc_module>::AHBSlave(sc_module_name nm, 
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
  ahb("ahb", ::amba::amba_AHB, ambaLayer, false /* Arbitration */ ),
  m_RequestPEQ("RequestPEQ"),
  m_ResponsePEQ("ResponsePEQ"), 
  busy(false),
  clockcycle(sc_core::sc_time(10, SC_NS)),
  m_wait_states(1) {
  
  // Register transport functions to sockets
  ahb.register_b_transport(this, &AHBSlave::b_transport);
  ahb.register_transport_dbg(this, &AHBSlave::transport_dbg);

  if(ambaLayer == amba::amba_AT) {

    // Register non-blocking transport for AT    
    ahb.register_nb_transport_fw(this, &AHBSlave::nb_transport_fw);
        
    SC_THREAD(wait_state_count);

    // Thread for modeling AHB pipeline delay
    SC_THREAD(requestThread);
        
    // Thread for interfacing functional part of the model
    // in AT mode.
    SC_THREAD(responseThread);
  }
}

template<>
AHBSlave<gs::reg::gr_device>::AHBSlave(sc_module_name nm, 
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
  ahb("ahb", ::amba::amba_AHB, ambaLayer, false /* Arbitration */ ),
  m_RequestPEQ("RequestPEQ"), 
  m_ResponsePEQ("ResponsePEQ"), 
  busy(false),
  clockcycle(sc_core::sc_time(10, SC_NS)),
  m_wait_states(1){
  
  // Register transport functions to sockets
  ahb.register_b_transport(this, &AHBSlave::b_transport);
  ahb.register_transport_dbg(this, &AHBSlave::transport_dbg);

  if(ambaLayer == amba::amba_AT) {

    // Register non-blocking transport for AT    
    ahb.register_nb_transport_fw(this, &AHBSlave::nb_transport_fw);

    SC_THREAD(wait_state_count);

    // Thread for modeling AHB pipeline delay
    SC_THREAD(requestThread);
        
    // Thread for interfacing functional part of the model
    // in AT mode.
    SC_THREAD(responseThread);
        
  }
}
