#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <serialization/annotations.hpp>
#include <serialization/promise.hpp>
#include "kernel.hpp"

namespace waos {

  struct process_event {    
    kernel::process_id process;
    virtual ~process_event() {}
  };

  struct [[=serialization::dynamic()]] process_killed_event : process_event {  
    virtual ~process_killed_event() {}    
  };

  struct [[=serialization::dynamic()]] process_error_event : process_event {
    virtual ~process_error_event() {}
    std::string error;
  };

  struct process_event_pointer {
    std::shared_ptr<process_event> event_pointer;
  };

}