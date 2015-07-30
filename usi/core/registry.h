// vim : set fileencoding=utf-8 expandtab noai ts=4 sw=4 :
/// @addtogroup pysc
/// @{
/// @file registry.h
/// 
/// @date 2013-2015
/// @copyright All rights reserved.
///            Any reproduction, use, distribution or disclosure of this
///            program, without the express, prior written consent of the 
///            authors is strictly prohibited.
/// @author Rolf Meyer
#ifndef PYSC_USI_CORE_REGISTRY_H_
#define PYSC_USI_CORE_REGISTRY_H_

#include <Python.h>
#include <systemc.h>
#include "core/common/sc_find.h"

class PyScModule {
  public:
    typedef void (*init_f)(void);
    PyScModule(init_f funct = NULL);
    operator PyThreadState*() { return module_thread; };
    operator PyInterpreterState*() { return module_thread->interp; };
    static void registerEmbedded();
    bool embedded;
  private:
    static PyScModule *reg;
    void initModule();

    PyThreadState *module_thread;
    init_f funct;
    PyScModule *next;
};

#define PYSC_REGISTER_MODULE(name) \
  extern "C" { \
    void init_##name(void); \
  }; \
  static PyScModule __pysc_module(init_##name); \
  volatile PyScModule *__pysc_module_##name = &__pysc_module;

#define PYSC_THIS_MODULE() \
  (__pysc_module)

#define PyScThisModule() \
  (__pysc_module)

#define PYSC_INIT_MODULES() \
  PyScModule::registerEmbedded();

#define PYSC_HAS_MODULE(name) \
  extern PyScModule *__pysc_module_##name; \
  __pysc_module_##name->embedded = true;

class PyScObjectGenerator {
  public:
    typedef PyObject *(*generator_f)(sc_core::sc_object *, std::string);
    PyScObjectGenerator(generator_f funct = NULL);

    static PyObject *find_object_by_ptr(sc_core::sc_object *obj);
    static sc_core::sc_object *find_object_by_name(std::string name);

  private:
    static PyScObjectGenerator *reg;
    generator_f funct;
    PyScObjectGenerator *next;
};

#define PYSC_REGISTER_OBJECT_GENERATOR(funct) \
  static PyScObjectGenerator __pysc_generator_##funct##__(&funct); \
  volatile PyScObjectGenerator *__pysc_generator_##funct = &__pysc_generator_##funct##__;

#define PYSC_REGISTER_OBJECT(type) \
PyObject *find_##type##_object(sc_core::sc_object *obj, std::string name) { \
  PyObject *result = NULL; \
  type *instance = dynamic_cast<type *>(obj); \
  if(instance) { \
    result = SWIG_NewPointerObj(SWIG_as_voidptr(instance), SWIGTYPE_p_##type, 0); \
  } \
  return result; \
} \
PYSC_REGISTER_OBJECT_GENERATOR(find_##type##_object);

#ifdef MTI_SYSTEMC
#define USI_MODULE_EXPORT(w, load) \
  extern "C" void* mti__##w(const char* sc_module_name, int is_boundary_top) \
  { \
    sc_core::mti_is_elab_boundary_top = is_boundary_top; \
    try \
    { \
      USI_HAS_MODULE(systemc_); \
      USI_HAS_MODULE(delegate); \
      /*USI_HAS_MODULE(scireg);*/ \
      usi_init(0, NULL); \
      load(); \
      usi_start_of_initialization(); \
      w *result = new w(sc_module_name); \
      usi_end_of_initialization(); \
      return result; \
    } \
    catch( const std::exception& rep ) \
    { \
      mti_PrintScMessage((char*)rep.what()); \
      mti_ScError(); \
    } \
    catch( const char* s ) \
    { \
      sc_core::message_function(s); \
      mti_ScError(); \
    } \
    catch( ... ) \
    { \
      sc_core::message_function("UNKNOWN SYSTEMC EXCEPTION OCCURED"); \
      mti_ScError(); \
    } \
    return NULL; \
  }
#endif

#endif // PYSC_USI_REGISTRY_H_
/// @}
