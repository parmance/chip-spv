#ifndef MACROS_HH
#define MACROS_HH

#include "logging.hh"
#include "iostream"
#ifdef CHIP_ABORT_ON_UNIMPL
#define UNIMPLEMENTED(x)                                                       \
  logCritical("{}: Called a function which is not implemented", __FUNCTION__); \
  std::abort();
#else
#define UNIMPLEMENTED(x)                                                   \
  logWarn("{}: Called a function which is not implemented", __FUNCTION__); \
  return x;
#endif

#define RETURN(x)                  \
  do {                             \
    hipError_t err = (x);          \
    Backend->tls_last_error = err; \
    return err;                    \
  } while (0)

#define ERROR_IF(cond, err)                                                  \
  if (cond) do {                                                             \
      logError("Error {} at {}:{} code {}", err, __FILE__, __LINE__, #cond); \
      Backend->tls_last_error = err;                                         \
      return err;                                                            \
  } while (0)

#define ERROR_CHECK_DEVNUM(device)                                         \
  ERROR_IF(((device < 0) || ((size_t)device >= Backend->getNumDevices())), \
           hipErrorInvalidDevice)

#define ERROR_CHECK_DEVHANDLE(device)                      \
  auto I = std::find(Backend->getDevices().begin(),        \
                     Backend->getDevices().end(), device); \
  ERROR_IF(I == Backend->getDevices().end(), hipErrorInvalidDevice)

#endif