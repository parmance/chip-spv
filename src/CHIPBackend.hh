/**
 * @file CHIPBackend.hh
 * @author Paulius Velesko (pvelesko@gmail.com)
 * @brief CHIPBackend class definition. CHIP backends are to inherit from this
 * base class and override desired virtual functions. Overrides for this class
 * are expected to be minimal with primary overrides being done on lower-level
 * classes such as CHIPContext consturctors, etc.
 * @version 0.1
 * @date 2021-08-19
 *
 * @copyright Copyright (c) 2021
 *
 */
#ifndef CHIP_BACKEND_H
#define CHIP_BACKEND_H

#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <mutex>
#include <string>
#include <vector>
#include <queue>
#include <stack>

#include "spirv.hh"
#include "hip/hip_runtime_api.h"
#include "hip/spirv_hip.hh"

#include "CHIPDriver.hh"
#include "logging.hh"
#include "macros.hh"
#include "CHIPException.hh"

class CHIPEventMonitor;

enum class CHIPQueueType : unsigned int {
  Blocking = hipStreamDefault,
  NonBlocking = hipStreamNonBlocking
};

enum class CHIPManagedMemFlags : unsigned int {
  AttachHost = hipMemAttachHost,
  AttachGlobal = hipMemAttachGlobal
};

class CHIPCallbackData {
protected:
  virtual ~CHIPCallbackData() = default;

public:
  CHIPQueue *ChipQueue;
  CHIPEvent *GpuReady;
  CHIPEvent *CpuCallbackComplete;
  CHIPEvent *GpuAck;

  hipError_t Status;
  void *CallbackArgs;
  hipStreamCallback_t CallbackF;

  CHIPCallbackData(hipStreamCallback_t CallbackF, void *CallbackArgs,
                   CHIPQueue *ChipQueue);

  void execute(hipError_t ResultFromDependency) {
    CallbackF(ChipQueue, ResultFromDependency, CallbackArgs);
  }
};

class CHIPEventMonitor {
  typedef void *(*THREADFUNCPTR)(void *);

protected:
  CHIPEventMonitor() = default;
  virtual ~CHIPEventMonitor() = default;
  pthread_t Thread_;

public:
  volatile bool Stop = false;

  void join() { pthread_join(Thread_, nullptr); }
  static void *monitorWrapper(void *Arg) {
    auto Monitor = (CHIPEventMonitor *)Arg;
    Monitor->monitor();
    return 0;
  }
  virtual void monitor(){};

  void start() {
    auto Res = pthread_create(&Thread_, 0, monitorWrapper, (void *)this);
    if (Res)
      CHIPERR_LOG_AND_THROW("Failed to create thread", hipErrorTbd);
    logDebug("Thread Created with ID : {}", Thread_);
  }
};

class CHIPTexture {
protected:
  // delete default constructor since texture needs both image and sampler
  CHIPTexture() = delete;

  CHIPTexture(intptr_t Image, intptr_t Sampler)
      : Image(Image), Sampler(Sampler) {}

public:
  intptr_t Image;
  intptr_t Sampler;
  hipTextureObject_t TexObj;
  hipTextureObject_t get() { return TexObj; }
};

template <class T> std::string resultToString(T Err);

enum class CHIPMemoryType : unsigned { Host = 0, Device = 1, Shared = 2 };
class CHIPEventFlags {
  bool BlockingSync_ = false;
  bool DisableTiming_ = false;
  bool Interprocess_ = false;

public:
  CHIPEventFlags() = default;
  CHIPEventFlags(unsigned Flags) {
    if (Flags & hipEventBlockingSync)
      BlockingSync_ = true;
    if (Flags & hipEventDisableTiming)
      DisableTiming_ = true;
    if (Flags & hipEventInterprocess)
      Interprocess_ = true;
  }

  bool isDefault() {
    return !BlockingSync_ && !DisableTiming_ && !Interprocess_;
  };
  bool isBlockingSync() { return BlockingSync_; };
  bool isDisableTiming() { return DisableTiming_; };
  bool isInterprocess() { return Interprocess_; };
};

struct AllocationInfo {
  void *BasePtr;
  size_t Size;
};

/**
 * @brief Class for keeping track of device allocations.
 *
 */
class CHIPAllocationTracker {
private:
  std::mutex Mtx_;
  std::unordered_map<void *, void *> HostToDev_;
  std::unordered_map<void *, void *> DevToHost_;
  std::string Name_;
  std::set<void *> PtrSet_;

  std::unordered_map<void *, AllocationInfo> DevToAllocInfo_;

public:
  /**
   * @brief Associate a host pointer with a device pointer. @see hipHostRegister
   *
   * @param HostPtr
   */
  void registerHostPointer(void *HostPtr, void *DevPtr) {
    HostToDev_[HostPtr] = DevPtr;
    DevToHost_[DevPtr] = HostPtr;
  }

  void *getAssociatedHostPtr(void *DevPtr) {
    if (!DevToHost_.count(DevPtr))
      return nullptr;

    return DevToHost_[DevPtr];
  }

  void unregsiterHostPointer(void *HostPtr) {
    if (HostToDev_.count(HostPtr) == 0)
      CHIPERR_LOG_AND_THROW("Tried to unregister a host variable which was not "
                            "registered with this device",
                            hipErrorTbd);
    HostToDev_.erase(HostPtr);
  }

  size_t GlobalMemSize, TotalMemSize, MaxMemUsed;
  /**
   * @brief Construct a new CHIPAllocationTracker object
   *
   * @param GlobalMemSize Total available global memory on the device
   * @param Name name for this allocation tracker for logging. Normally device
   * name
   */
  CHIPAllocationTracker(size_t GlobalMemSize, std::string Name);

  /**
   * @brief Destroy the CHIPAllocationTracker object
   *
   */
  ~CHIPAllocationTracker();

  /**
   * @brief Get the Name object
   *
   * @return std::string
   */
  std::string getName();

  /**
   * @brief Get allocation_info based on host pointer
   *
   * @return allocation_info contains the base pointer and allocation size;
   */
  AllocationInfo *getByHostPtr(const void *);
  /**
   * @brief Get allocation_info based on device pointer
   *
   * @return allocation_info contains the base pointer and allocation size;
   */
  AllocationInfo *getByDevPtr(const void *);

  /**
   * @brief Reserve memory for an allocation.
   * This method is run prior to allocations to keep track of how much memory is
   * available on the device
   *
   * @param bytes
   * @return true Reservation successful
   * @return false Not enough available memory for reservation of this size.
   */
  bool reserveMem(size_t Bytes);

  /**
   * @brief Release some of the reserved memory. Called by free()
   *
   * @param bytes
   * @return true
   * @return false
   */
  bool releaseMemReservation(size_t Bytes);

  /**
   * @brief Record the pointer received from CHIPContext::allocate_()
   *
   * @param dev_ptr
   */
  void recordAllocation(void *DevPtr, size_t Size);
};

class CHIPDeviceVar {
private:
  std::string Name_; /// Device side variable name.
                     /// Address to variable's storage. Note that the address is
                     /// a pointer given by CHIPContext::allocate.
  void *DevAddr_ = nullptr;
  size_t Size_ = 0;
  /// The alignment requirement of the variable.
  // NOTE: The alignment infromation is not carried in __hipRegisterVar() calls
  // It have to be queried via shadow kernels.
  size_t Alignment_ = 0;
  /// Tells if the variable has an initializer. NOTE: Variables are
  /// initialized via a shadow kernel.
  bool HasInitializer_ = false;

public:
  CHIPDeviceVar(std::string Name, size_t Size);
  ~CHIPDeviceVar();

  void *getDevAddr() const { return DevAddr_; }
  void setDevAddr(void *Addr) { DevAddr_ = Addr; }
  std::string getName() const { return Name_; }
  size_t getSize() const { return Size_; }
  size_t getAlignment() const { return Alignment_; }
  void setAlignment(size_t TheAlignment) {
    assert(Alignment_ && "Invalid alignment");
    Alignment_ = TheAlignment;
  }
  bool hasInitializer() const { return HasInitializer_; }
  void markHasInitializer(bool State = true) { HasInitializer_ = State; }
};

// fw declares
class CHIPExecItem;
class CHIPQueue;
class CHIPContext;
class CHIPDevice;

class CHIPEvent {
protected:
  event_status_e EventStatus_;
  CHIPEventFlags Flags_;

  // reference count
  size_t *Refc_;

  /**
   * @brief Events are always created with a context
   *
   */
  CHIPContext *ChipContext_;

  /**
   * @brief hidden default constructor for CHIPEvent. Only derived class
   * constructor should be called.
   *
   */
  CHIPEvent() = default;

public:
  CHIPEventFlags getFlags() { return Flags_; }
  std::mutex Mtx;
  std::string Msg;
  size_t getCHIPRefc() { return *Refc_; }
  virtual void takeOver(CHIPEvent *Other){};
  virtual void decreaseRefCount() {
    // logDebug("CHIPEvent::decreaseRefCount() {} refc {}->{}", Msg.c_str(),
    //          *Refc_, *Refc_ - 1);
    (*Refc_)--;
    // Destructor to be called by event monitor once backend is done using it
  }
  virtual void increaseRefCount() {
    // logDebug("CHIPEvent::increaseRefCount() {} refc {}->{}", Msg.c_str(),
    //          *Refc_, *Refc_ + 1);
    (*Refc_)++;
  }
  virtual ~CHIPEvent() = default;
  // Optionally provide a field for origin of this event
  /**
   * @brief CHIPEvent constructor. Must always be created with some context.
   *
   */
  CHIPEvent(CHIPContext *Ctx, CHIPEventFlags Flags = CHIPEventFlags());
  /**
   * @brief Get the Context object
   *
   * @return CHIPContext* pointer to context on which this event was created
   */
  CHIPContext *getContext() { return ChipContext_; }

  /**
   * @brief Query the state of this event and update it's status
   * Each backend must override this method with implementation specific calls
   * e.x. clGetEventInfo()
   *
   * @return true event was in recording state, state might have changed
   * @return false event was not in recording state
   */
  virtual bool updateFinishStatus() = 0;

  /**
   * @brief Check if this event is recording or already recorded
   *
   * @return true event is recording/recorded
   * @return false event is in init or invalid state
   */
  bool isRecordingOrRecorded() {
    return EventStatus_ >= EVENT_STATUS_RECORDING;
  }

  /**
   * @brief check if this event is done recording
   *
   * @return true recoded
   * @return false not recorded
   */
  bool isFinished() { return (EventStatus_ == EVENT_STATUS_RECORDED); }

  /**
   * @brief Get the Event Status object
   *
   * @return event_status_e current event status
   */
  event_status_e getEventStatus() { return EventStatus_; }

  std::string getEventStatusStr() {
    switch (EventStatus_) {
    case EVENT_STATUS_INIT:
      return "EVENT_STATUS_INIT";
    case EVENT_STATUS_RECORDING:
      return "EVENT_STATUS_RECORDING";
    case EVENT_STATUS_RECORDED:
      return "EVENT_STATUS_RECORDED";
    default:
      return "INVALID_EVENT_STATUS";
    };
  }

  /**
   * @brief Enqueue this event in a given CHIPQueue
   *
   * @param chip_queue_ CHIPQueue in which to enque this event
   * @return true
   * @return false
   */
  virtual void recordStream(CHIPQueue *ChipQueue);
  /**
   * @brief Wait for this event to complete
   *
   * @return true
   * @return false
   */
  virtual bool wait() = 0;

  /**
   * @brief Calculate absolute difference between completion timestamps of this
   * event and other
   *
   * @param other
   * @return float
   */
  virtual float getElapsedTime(CHIPEvent *Other) = 0;

  /**
   * @brief Toggle this event from the host.
   *
   */
  virtual void hostSignal() = 0;
};

/**
 * @brief Module abstraction. Contains global variables and kernels. Can be
 * extracted from FatBinary or loaded at runtime.
 * OpenCL - ClProgram
 * Level Zero - zeModule
 * ROCclr - amd::Program
 * CUDA - CUmodule
 */
class CHIPModule {
  /// Flag for the allocation state of the device variables. True if
  /// all variables have space allocated for this module for the
  /// device this module is attached to. False implies that
  /// DeviceVariablesAllocated false.
  bool DeviceVariablesAllocated_ = false;
  /// Flag for the initialization state of the device variables. True
  /// if all variables are initialized for this module for the device
  /// this module is attached to.
  bool DeviceVariablesInitialized_ = false;

  OpenCLFunctionInfoMap FuncInfos_;

protected:
  uint8_t *FuncIL_;
  size_t IlSize_;
  std::mutex Mtx_;
  // Global variables
  std::vector<CHIPDeviceVar *> ChipVars_;
  // Kernels
  std::vector<CHIPKernel *> ChipKernels_;
  /// Binary representation extracted from FatBinary
  std::string Src_;
  // Kernel JIT compilation can be lazy
  std::once_flag Compiled_;

  int32_t *BinaryData_;

  /**
   * @brief hidden default constuctor. Only derived type constructor should be
   * called.
   *
   */
  CHIPModule() = default;

public:
  /**
   * @brief Destroy the CHIPModule object
   *
   */
  ~CHIPModule();
  /**
   * @brief Construct a new CHIPModule object.
   * This constructor should be implemented by the derived class (specific
   * backend implementation). Call to this constructor should result in a
   * populated chip_kernels vector.
   *
   * @param module_str string prepresenting the binary extracted from FatBinary
   */
  CHIPModule(std::string *ModuleStr);
  /**
   * @brief Construct a new CHIPModule object using move semantics
   *
   * @param module_str string from which to move resources
   */
  CHIPModule(std::string &&ModuleStr);

  /**
   * @brief Add a CHIPKernel to this module.
   * During initialization when the FatBinary is consumed, a CHIPModule is
   * constructed for every device. SPIR-V kernels reside in this module. This
   * method is called called via the constructor during this initialization
   * phase. Modules can also be loaded from a file during runtime, however.
   *
   * @param kernel CHIPKernel to be added to this module.
   */
  void addKernel(CHIPKernel *Kernel);

  /**
   * @brief Wrapper around compile() called via std::call_once
   *
   * @param chip_dev device for which to compile the kernels
   */
  void compileOnce(CHIPDevice *ChipDev);
  /**
   * @brief Kernel JIT compilation can be lazy. This is configured via Cmake
   * LAZY_JIT option. If LAZY_JIT is set to true then this module won't be
   * compiled until the first call to one of its kernels. If LAZY_JIT is set to
   * false(default) then this method should be called in the constructor;
   *
   * This method should populate this modules chip_kernels vector. These
   * kernels would have a name extracted from the kernel but no associated host
   * function pointers.
   *
   */
  virtual void compile(CHIPDevice *ChipDev) = 0;
  /**
   * @brief Get the Global Var object
   * A module, along with device kernels, can also contain global variables.
   *
   * @param name global variable name
   * @return CHIPDeviceVar*
   */
  virtual CHIPDeviceVar *getGlobalVar(const char *VarName);

  /**
   * @brief Get the Kernel object
   *
   * @param name name of the corresponding host function
   * @return CHIPKernel*
   */
  CHIPKernel *getKernel(std::string Name);

  /**
   * @brief Checks if the module has a kernel with the given name.
   *
   * @param name the name of the kernel
   * @return true in case the kernels is found
   */
  bool hasKernel(std::string Name) const;

  /**
   * @brief Get the Kernels object
   *
   * @return std::vector<CHIPKernel*>&
   */
  std::vector<CHIPKernel *> &getKernels();

  /**
   * @brief Get the Kernel object
   *
   * @param host_f_ptr host-side function pointer
   * @return CHIPKernel*
   */
  CHIPKernel *getKernel(const void *HostFPtr);

  /**
   * @brief consume SPIRV and fill in OCLFuncINFO
   *
   */
  void consumeSPIRV();

  /**
   * @brief Record a device variable
   *
   * Takes ownership of the variable.
   */
  void addDeviceVariable(CHIPDeviceVar *DevVar) { ChipVars_.push_back(DevVar); }

  std::vector<CHIPDeviceVar *> &getDeviceVariables() { return ChipVars_; }

  hipError_t allocateDeviceVariablesNoLock(CHIPDevice *Device,
                                           CHIPQueue *Queue);
  void initializeDeviceVariablesNoLock(CHIPDevice *Device, CHIPQueue *Queue);
  void invalidateDeviceVariablesNoLock();
  void deallocateDeviceVariablesNoLock(CHIPDevice *Device);

  OCLFuncInfo *findFunctionInfo(const std::string &FName);
};

/**
 * @brief Contains information about the function on the host and device
 */
class CHIPKernel {
protected:
  /**
   * @brief hidden default constructor. Only derived type constructor should be
   * called.
   *
   */
  CHIPKernel(std::string HostFName, OCLFuncInfo *FuncInfo);
  /// Name of the function
  std::string HostFName_;
  /// Pointer to the host function
  const void *HostFPtr_;
  /// Pointer to the device function
  const void *DevFPtr_;

  OCLFuncInfo *FuncInfo_;

public:
  ~CHIPKernel();

  /**
   * @brief Get the Name object
   *
   * @return std::string
   */
  std::string getName();

  /**
   * @brief Get the Func Info object
   *
   * @return OCLFuncInfo&
   */
  OCLFuncInfo *getFuncInfo();
  /**
   * @brief Get the associated host pointer to a host function
   *
   * @return const void*
   */
  const void *getHostPtr();
  /**
   * @brief Get the associated funciton pointer on the device
   *
   * @return const void*
   */
  const void *getDevPtr();

  /**
   * @brief Get the Name object
   *
   * @return std::string
   */
  void setName(std::string HostFName);
  /**
   * @brief Get the associated host pointer to a host function
   *
   * @return const void*
   */
  void setHostPtr(const void *HostFPtr);
  /**
   * @brief Get the associated funciton pointer on the device
   *
   * @return const void*
   */
  void setDevPtr(const void *DevFPtr);
};

/**
 * @brief Contains kernel arguments and a queue on which to execute.
 * Prior to kernel launch, the arguments are setup via
 * CHIPBackend::configureCall(). Because of this, we get the kernel last so the
 * kernel so the launch() takes a kernel argument as opposed to queue receiving
 * a CHIPExecItem containing the kernel and arguments
 *
 */
class CHIPExecItem {
protected:
  size_t SharedMem_;
  // Structures for old HIP launch API.
  std::vector<uint8_t> ArgData_;
  std::vector<std::tuple<size_t, size_t>> OffsetSizes_;

  dim3 GridDim_;
  dim3 BlockDim_;

  CHIPKernel *ChipKernel_;
  CHIPQueue *ChipQueue_;

  // Structures for new HIP launch API.
  void **ArgsPointer_ = nullptr;

public:
  size_t getNumArgs() { return getKernel()->getFuncInfo()->ArgTypeInfo.size(); }
  void **getArgsPointer() { return ArgsPointer_; }
  /**
   * @brief Deleted default constructor
   * Doesn't make sense for CHIPExecItem to exist without arguments
   *
   */
  CHIPExecItem() = delete;
  /**
   * @brief Construct a new CHIPExecItem object
   *
   * @param grid_dim_
   * @param block_dim_
   * @param shared_mem_
   * @param chip_queue_
   */
  CHIPExecItem(dim3 GirdDim, dim3 BlockDim, size_t SharedMem,
               hipStream_t ChipQueue);

  /**
   * @brief Destroy the CHIPExecItem object
   *
   */
  ~CHIPExecItem();

  /**
   * @brief Get the Kernel object
   *
   * @return CHIPKernel* Kernel to be executed
   */
  CHIPKernel *getKernel();
  /**
   * @brief Get the Queue object
   *
   * @return CHIPQueue*
   */
  CHIPQueue *getQueue();

  std::vector<uint8_t> getArgData();

  /**
   * @brief Get the Grid object
   *
   * @return dim3
   */
  dim3 getGrid();

  /**
   * @brief Get the Block object
   *
   * @return dim3
   */
  dim3 getBlock();

  /**
   * @brief Get the SharedMem
   *
   * @return size_t
   */
  size_t getSharedMem();

  /**
   * @brief Setup a single argument.
   * gets called by hipSetupArgument calls to which are emitted by hip-clang.
   *
   * @param arg
   * @param size
   * @param offset
   */
  void setArg(const void *Arg, size_t Size, size_t Offset);

  /**
   * @brief Set the Arg Pointer object for launching kernels via new HIP API
   *
   * @param args Pointer to a array of pointers, each pointing to an
   *             individual argument.
   */
  void setArgPointer(void **Args) { ArgsPointer_ = Args; }

  /**
   * @brief Sets up the kernel arguments via backend API calls.
   * Called after all the arugments are setup either via hipSetupArg() (old HIP
   * kernel launch API)
   * Or after hipLaunchKernel (new HIP kernel launch API)
   *
   */
  void setupAllArgs();

  void setKernel(CHIPKernel *Kernel) { this->ChipKernel_ = Kernel; }
};

/**
 * @brief Compute device class
 */
class CHIPDevice {
protected:
  std::string DeviceName_;
  std::mutex Mtx_;
  CHIPContext *Ctx_;
  std::vector<CHIPQueue *> ChipQueues_;
  int ActiveQueueId_ = 0;
  std::once_flag PropsPopulated_;

  hipDeviceAttribute_t Attrs_;
  hipDeviceProp_t HipDeviceProps_;

  size_t TotalUsedMem_;
  size_t MaxUsedMem_;
  size_t MaxMallocSize_ = 0;

  /// Maps host-side shadow variables to the corresponding device variables.
  std::unordered_map<const void *, CHIPDeviceVar *> DeviceVarLookup_;

  int Idx_ = -1; // Initialized with a value indicating unset ID.

public:
  size_t getMaxMallocSize() {
    if (MaxMallocSize_ < 1)
      CHIPERR_LOG_AND_THROW("MaxMallocSize was not set", hipErrorTbd);
    return MaxMallocSize_;
  }
  /// Registered modules and a mapping from module binary blob pointers
  /// to the associated CHIPModule.
  std::unordered_map<const std::string *, CHIPModule *> ChipModules;

  CHIPAllocationTracker *AllocationTracker = nullptr;

  /**
   * @brief Construct a new CHIPDevice object
   *
   */
  CHIPDevice(CHIPContext *Ctx, int DeviceIdx);

  /**
   * @brief Construct a new CHIPDevice object
   *
   */
  CHIPDevice();

  /**
   * @brief Destroy the CHIPDevice object
   *
   */
  ~CHIPDevice();

  /**
   * @brief Get the Kernels object
   *
   * @return std::vector<CHIPKernel*>&
   */
  std::vector<CHIPKernel *> getKernels();

  /**
   * @brief Get the Modules object
   *
   * @return std::vector<CHIPModule*>&
   */
  std::unordered_map<const std::string *, CHIPModule *> &getModules();

  /**
   * @brief Use a backend to populate device properties such as memory
   * available, frequencies, etc.
   */
  void populateDeviceProperties();

  /**
   * @brief Use a backend to populate device properties such as memory
   * available, frequencies, etc.
   */
  virtual void populateDevicePropertiesImpl() = 0;

  /**
   * @brief Query the device for properties
   *
   * @param prop
   */
  void copyDeviceProperties(hipDeviceProp_t *Prop);

  /**
   * @brief Use the host function pointer to retrieve the kernel
   *
   * @param hostPtr
   * @return CHIPKernel* CHIPKernel associated with this host pointer
   */
  CHIPKernel *findKernelByHostPtr(const void *HostPtr);

  /**
   * @brief Get the context object
   *
   * @return CHIPContext* pointer to the CHIPContext object this CHIPDevice
   * was created with
   */
  CHIPContext *getContext();

  /**
   * @brief Construct an additional queue for this device
   *
   * @param flags
   * @param priority
   * @return CHIPQueue* pointer to the newly created queue (can also be found
   * in chip_queues vector)
   */
  virtual CHIPQueue *addQueueImpl(unsigned int Flags, int Priority) = 0;

  CHIPQueue *addQueue(unsigned int Flags, int Priority);

  /**
   * @brief Add a queue to this device
   *
   * @param chip_queue_  CHIPQueue to be added
   */
  void addQueue(CHIPQueue *ChipQueue);
  /**
   * @brief Get the Queues object
   *
   * @return std::vector<CHIPQueue*>
   */
  std::vector<CHIPQueue *> getQueues();
  /**
   * @brief HIP API allows for setting the active device, not the active queue
   * so active device's active queue is always it's 0th/default/primary queue
   *
   * @return CHIPQueue*
   */
  CHIPQueue *getActiveQueue();
  /**
   * @brief Remove a queue from this device's queue vector
   *
   * @param q
   * @return true
   * @return false
   */
  bool removeQueue(CHIPQueue *ChipQueue);

  /**
   * @brief Get the integer ID of this device as it appears in the Backend's
   * chip_devices list
   *
   * @return int
   */
  int getDeviceId();
  /**
   * @brief Get the device name
   *
   * @return std::string
   */
  std::string getName();

  /**
   * @brief Destroy all allocations and reset all state on the current device in
   the current process.
   *
   */
  virtual void reset() = 0;

  /**
   * @brief Query for a specific device attribute. Implementation copied from
   * HIPAMD.
   *
   * @param attr attribute to query
   * @return int attribute value. In case invalid query returns -1;
   */
  int getAttr(hipDeviceAttribute_t Attr);

  /**
   * @brief Get the total global memory available for this device.
   *
   * @return size_t
   */
  size_t getGlobalMemSize();

  /**
   * @brief Set the Cache Config object
   *
   * @param cfg configuration
   */
  virtual void setCacheConfig(hipFuncCache_t Cfg);

  /**
   * @brief Get the cache configuration for this device
   *
   * @return hipFuncCache_t
   */
  virtual hipFuncCache_t getCacheConfig();

  /**
   * @brief Configure shared memory for this device
   *
   * @param config
   */
  virtual void setSharedMemConfig(hipSharedMemConfig Cfg);

  /**
   * @brief Get the shared memory configuration for this device
   *
   * @return hipSharedMemConfig
   */
  virtual hipSharedMemConfig getSharedMemConfig();

  /**
   * @brief Setup the cache configuration for the device to use when executing
   * this function
   *
   * @param func
   * @param config
   */
  virtual void setFuncCacheConfig(const void *Func, hipFuncCache_t Cfg);

  /**
   * @brief Check if the current device has same PCI bus ID as the one given by
   * input
   *
   * @param pciDomainID
   * @param pciBusID
   * @param pciDeviceID
   * @return true
   * @return false
   */
  bool hasPCIBusId(int PciDomainID, int PciBusID, int PciDeviceID);

  /**
   * @brief Get peer-accesability between this and another device
   *
   * @param peerDevice
   * @return int
   */
  int getPeerAccess(CHIPDevice *PeerDevice);

  /**
   * @brief Set access between this and another device
   *
   * @param peer
   * @param flags
   * @param canAccessPeer
   * @return hipError_t
   */
  hipError_t setPeerAccess(CHIPDevice *Peer, int Flags, bool CanAccessPeer);

  /**
   * @brief Get the total used global memory
   *
   * @return size_t
   */
  size_t getUsedGlobalMem();

  /**
   * @brief Get the global variable that came from a FatBinary module
   *
   * @param var host pointer to the variable
   * @return CHIPDeviceVar*
   */
  CHIPDeviceVar *getDynGlobalVar(const void *Var) { UNIMPLEMENTED(nullptr); }

  /**
   * @brief Get the global variable that came from a FatBinary module
   *
   * @param var Pointer to host side shadow variable.
   * @return CHIPDeviceVar*
   */
  CHIPDeviceVar *getStatGlobalVar(const void *Var);

  /**
   * @brief Get the global variable
   *
   * @param var Pointer to host side shadow variable.
   * @return CHIPDeviceVar* if not found returns nullptr
   */
  CHIPDeviceVar *getGlobalVar(const void *Var);

  /**
   * @brief Take the module source, compile the kernels and associate the host
   * function pointer with a kernel whose name matches host function name
   *
   * @param module_str Binary representation of the SPIR-V module
   * @param host_f_ptr host function pointer
   * @param host_f_name host function name
   */
  void registerFunctionAsKernel(std::string *ModuleStr, const void *HostFPtr,
                                const char *HostFName);

  void registerDeviceVariable(std::string *ModuleStr, const void *HostPtr,
                              const char *Name, size_t Size);

  virtual CHIPModule *addModule(std::string *ModuleStr) = 0;

  virtual CHIPTexture *
  createTexture(const hipResourceDesc *ResDesc, const hipTextureDesc *TexDesc,
                const struct hipResourceViewDesc *ResViewDesc) = 0;

  virtual void destroyTexture(CHIPTexture *TextureObject) = 0;

  hipError_t allocateDeviceVariables();
  void initializeDeviceVariables();
  void invalidateDeviceVariables();
  void deallocateDeviceVariables();
};

/**
 * @brief Context class
 * Contexts contain execution queues and are created on top of a single or
 * multiple devices. Provides for creation of additional queues, events, and
 * interaction with devices.
 */
class CHIPContext {
protected:
  std::vector<CHIPDevice *> ChipDevices_;
  std::vector<CHIPQueue *> ChipQueues_;
  std::vector<void *> AllocatedPtrs_;

  unsigned int Flags_;

public:
  std::vector<CHIPEvent *> Events;
  std::mutex Mtx;
  /**
   * @brief Construct a new CHIPContext object
   *
   */
  CHIPContext();
  /**
   * @brief Destroy the CHIPContext object
   *
   */
  virtual ~CHIPContext();

  virtual void syncQueues(CHIPQueue *TargetQueue);

  /**
   * @brief Add a device to this context
   *
   * @param dev pointer to CHIPDevice object
   * @return true if device was added successfully
   * @return false upon failure
   */
  void addDevice(CHIPDevice *Dev);
  /**
   * @brief Add a queue to this context
   *
   * @param q CHIPQueue to be added
   */
  void addQueue(CHIPQueue *ChipQueue);

  /**
   * @brief Get this context's CHIPDevices
   *
   * @return std::vector<CHIPDevice*>&
   */
  std::vector<CHIPDevice *> &getDevices();

  /**
   * @brief Get the this contexts CHIPQueues
   *
   * @return std::vector<CHIPQueue*>&
   */
  std::vector<CHIPQueue *> &getQueues();

  /**
   * @brief Find a queue. If a null pointer is passed, return the Active Queue
   * (active devices's primary queue). If this queue is not found in this
   * context then return nullptr
   *
   * @param stream CHIPQueue to find
   * @return hipStream_t
   */
  hipStream_t findQueue(hipStream_t Stream);

  /**
   * @brief Allocate data.
   * Calls reserveMem() to keep track memory used on the device.
   * Calls CHIPContext::allocate_(size_t size, size_t alignment,
   * CHIPMemoryType mem_type) with allignment = 0 and allocation type = Shared
   *
   *
   * @param size size of the allocation
   * @return void* pointer to allocated memory
   */
  void *allocate(size_t Size);

  /**
   * @brief Allocate data.
   * Calls reserveMem() to keep track memory used on the device.
   * Calls CHIPContext::allocate_(size_t size, size_t alignment,
   * CHIPMemoryType mem_type) with allignment = 0
   *
   * @param size size of the allocation
   * @param mem_type type of the allocation: Host, Device, Shared
   * @return void* pointer to allocated memory
   */
  void *allocate(size_t Size, CHIPMemoryType MemType);

  /**
   * @brief Allocate data.
   * Calls reserveMem() to keep track memory used on the device.
   * Calls CHIPContext::allocate_(size_t size, size_t alignment,
   * CHIPMemoryType mem_type)
   *
   * @param size size of the allocation
   * @param alignment allocation alignment in bytes
   * @param mem_type type of the allocation: Host, Device, Shared
   * @return void* pointer to allocated memory
   */
  void *allocate(size_t Size, size_t Alignment, CHIPMemoryType MemType);

  /**
   * @brief Allocate data. Pure virtual function - to be overriden by each
   * backend. This member function is the one that's called by all the
   * publically visible CHIPContext::allocate() variants
   *
   * @param size size of the allocation.
   * @param alignment allocation alignment in bytes
   * @param mem_type type of the allocation: Host, Device, Shared
   * @return void*
   */
  virtual void *allocateImpl(size_t Size, size_t Alignment,
                             CHIPMemoryType MemType) = 0;

  /**
   * @brief Free memory
   *
   * @param ptr pointer to the memory location to be deallocated. Internally
   * calls CHIPContext::free_()
   * @return true Success
   * @return false Failure
   */
  hipError_t free(void *Ptr);

  /**
   * @brief Free memory
   * To be overriden by the backend
   *
   * @param ptr
   * @return true
   * @return false
   */
  virtual void freeImpl(void *Ptr) = 0;

  /**
   * @brief Finish all the queues in this context
   *
   */
  void finishAll();

  /**
   * @brief For a given device pointer, return the base address of the
   * allocation to which it belongs to along with the allocation size
   *
   * @param pbase device base pointer to which dptr belongs to
   * @param psize size of the allocation with which pbase was created
   * @param dptr device pointer
   * @return hipError_t
   */
  virtual hipError_t findPointerInfo(hipDeviceptr_t *Base, size_t *Size,
                                     hipDeviceptr_t Ptr);

  /**
   * @brief Get the flags set on this context
   *
   * @return unsigned int context flags
   */
  unsigned int getFlags();

  /**
   * @brief Set the flags for this context
   *
   * @param flags flags to set on this context
   */
  void setFlags(unsigned int Flags);

  /**
   * @brief Reset this context.
   *
   */
  void reset();

  /**
   * @brief Retain this context.
   * TODO: What does it mean to retain a context?
   *
   * @return CHIPContext*
   */
  CHIPContext *retain();
};

/**
 * @brief Primary object to interact with the backend
 */
class CHIPBackend {
protected:
  /**
   * @brief ChipModules stored in binary representation.
   * During compilation each translation unit is parsed for functions that are
   * marked for execution on the device. These functions are then compiled to
   * device code and stored in binary representation.
   *  */
  std::vector<std::string *> ModulesStr_;
  std::mutex Mtx_;

  CHIPContext *ActiveCtx_;
  CHIPDevice *ActiveDev_;
  CHIPQueue *ActiveQ_;

public:
  std::mutex CallbackStackMtx;
  std::vector<CHIPEvent *> Events;
  std::mutex EventsMtx;

  std::queue<CHIPCallbackData *> CallbackStack;
  /**
   * @brief Keep track of pointers allocated on the device. Used to get info
   * about allocaitons based on device poitner in case that findPointerInfo() is
   * not overriden
   *
   */
  // Adds -std=c++17 requirement
  inline static thread_local hipError_t TlsLastError;

  std::stack<CHIPExecItem *> ChipExecStack;
  std::vector<CHIPContext *> ChipContexts;
  std::vector<CHIPQueue *> ChipQueues;
  std::vector<CHIPDevice *> ChipDevices;

  /**
   * @brief User defined compiler options to pass to the JIT compiler
   *
   */
  std::string CustomJitFlags;

  /**
   * @brief Get the default compiler flags for the JIT compiler
   *
   * @return std::string flags to pass to JIT compiler
   */
  virtual std::string getDefaultJitFlags() = 0;

  /**
   * @brief Get the jit options object
   * return CHIP_JIT_FLAGS if it is set, otherwise return default options as
   * defined by CHIPBackend<implementation>::getDefaultJitFlags()
   *
   * @return std::string flags to pass to JIT compiler
   */
  std::string getJitFlags();

  // TODO
  // key for caching compiled modules. To get a cached compiled module on a
  // particular device you must make sure that you have a module which matches
  // the host funciton pointer and also that this module was compiled for the
  // same device model.
  // typedef  std::pair<const void*, std::string> ptr_dev;
  // /**
  //  * @brief
  //  *
  //  */
  // std::unordered_map<ptr_dev, CHIPModule*> host_f_ptr_to_chipmodule_map;

  /**
   * @brief Construct a new CHIPBackend object
   *
   */
  CHIPBackend();
  /**
   * @brief Destroy the CHIPBackend objectk
   *
   */
  virtual ~CHIPBackend();

  /**
   * @brief Initialize this backend with given environment flags
   *
   * @param platform_str
   * @param device_type_str
   * @param device_ids_str
   */
  void initialize(std::string PlatformStr, std::string DeviceTypeStr,
                  std::string DeviceIdStr);

  /**
   * @brief Initialize this backend with given environment flags
   *
   * @param platform_str
   * @param device_type_str
   * @param device_ids_str
   */
  virtual void initializeImpl(std::string PlatformStr,
                              std::string DeviceTypeStr,
                              std::string DeviceIdStr) = 0;

  /**
   * @brief
   *
   */
  virtual void uninitialize();

  /**
   * @brief Get the Queues object
   *
   * @return std::vector<CHIPQueue*>&
   */
  std::vector<CHIPQueue *> &getQueues();
  /**
   * @brief Get the Active Queue object
   *
   * @return CHIPQueue*
   */
  CHIPQueue *getActiveQueue();
  /**
   * @brief Get the Active Context object. Returns the context of the active
   * queue.
   *
   * @return CHIPContext*
   */
  CHIPContext *getActiveContext();
  /**
   * @brief Get the Active Device object. Returns the device of the active
   * queue.
   *
   * @return CHIPDevice*
   */
  CHIPDevice *getActiveDevice();
  /**
   * @brief Set the active device. Sets the active queue to this device's
   * first/default/primary queue.
   *
   * @param chip_dev
   */
  void setActiveDevice(CHIPDevice *ChipDevice);

  std::vector<CHIPDevice *> &getDevices();
  /**
   * @brief Get the Num Devices object
   *
   * @return size_t
   */
  size_t getNumDevices();
  /**
   * @brief Get the vector of registered modules (in string/binary format)
   *
   * @return std::vector<std::string*>&
   */
  std::vector<std::string *> &getModulesStr();
  /**
   * @brief Add a context to this backend.
   *
   * @param ctx_in
   */
  void addContext(CHIPContext *ChipContext);
  /**
   * @brief Add a context to this backend.
   *
   * @param q_in
   */
  void addQueue(CHIPQueue *ChipQueue);
  /**
   * @brief  Add a device to this backend.
   *
   * @param dev_in
   */
  void addDevice(CHIPDevice *ChipDevice);
  /**
   * @brief
   *
   * @param mod_str
   */
  void registerModuleStr(std::string *ModuleStr);
  /**
   * @brief
   *
   * @param mod_str
   */
  void unregisterModuleStr(std::string *ModuleStr);
  /**
   * @brief Configure an upcoming kernel call
   *
   * @param grid
   * @param block
   * @param shared
   * @param q
   * @return hipError_t
   */
  hipError_t configureCall(dim3 GridDim, dim3 BlockDim, size_t SharedMem,
                           hipStream_t ChipQueue);
  /**
   * @brief Set the Arg object
   *
   * @param arg
   * @param size
   * @param offset
   * @return hipError_t
   */
  hipError_t setArg(const void *Arg, size_t Size, size_t Offset);

  /**
   * @brief Register this function as a kernel for all devices initialized
   * in this backend
   *
   * @param module_str
   * @param host_f_ptr
   * @param host_f_name
   * @return true
   * @return false
   */
  virtual bool registerFunctionAsKernel(std::string *ModuleStr,
                                        const void *HostFPtr,
                                        const char *HostFName);

  void registerDeviceVariable(std::string *ModuleStr, const void *HostPtr,
                              const char *Name, size_t Size);

  /**
   * @brief Return a device which meets or exceeds the requirements
   *
   * @param props
   * @return CHIPDevice*
   */
  CHIPDevice *findDeviceMatchingProps(const hipDeviceProp_t *Props);

  /**
   * @brief Find a given queue in this backend.
   *
   * @param q queue to find
   * @return CHIPQueue* return queue or nullptr if not found
   */
  CHIPQueue *findQueue(CHIPQueue *ChipQueue);

  /**
   * @brief Add a CHIPModule to every initialized device
   *
   * @param chip_module pointer to CHIPModule object
   * @return hipError_t
   */
  // CHIPModule* addModule(std::string* module_src);
  /**
   * @brief Remove this module from every device
   *
   * @param chip_module pointer to the module which is to be removed
   * @return hipError_t
   */
  // hipError_t removeModule(CHIPModule* chip_module);

  /************Factories***************/

  virtual CHIPTexture *createCHIPTexture(intptr_t Image, intptr_t Sampler) = 0;
  virtual CHIPQueue *createCHIPQueue(CHIPDevice *ChipDev) = 0;

  /**
   * @brief Create an Event, adding it to the Backend Event list.
   *
   * @param ChipCtx Context in which to create the event in
   * @param Flags Events falgs
   * @param UserEvent Is this a user event? If so, increase refcount to 2 to
   * prevent it from being garbage collected.
   * @return CHIPEvent* Event
   */
  virtual CHIPEvent *createCHIPEvent(CHIPContext *ChipCtx,
                                     CHIPEventFlags Flags = CHIPEventFlags(),
                                     bool UserEvent = false) = 0;

  /**
   * @brief Create a Callback Obj object
   * Each backend must implement this function which calls a derived
   * CHIPCallbackData constructor
   * @return CHIPCallbackData* pointer to newly allocated CHIPCallbackData
   * object.
   */
  virtual CHIPCallbackData *createCallbackData(hipStreamCallback_t Callback,
                                               void *UserData,
                                               CHIPQueue *ChipQ) = 0;

  virtual CHIPEventMonitor *createCallbackEventMonitor() = 0;
  virtual CHIPEventMonitor *createStaleEventMonitor() = 0;

  /**
 * @brief Get the Callback object

 * @param callback_data pointer to callback object
 * @return true callback object available
 * @return false callback object not available
 */
  bool getCallback(CHIPCallbackData **CallbackData) {
    // std::lock_guard<std::mutex> Lock(Mtx_);

    // bool Res = false;
    // logDebug("Elements in callback stack: {}", CallbackStack.size());
    // if (this->CallbackStack.size()) {
    //   *CallbackData = CallbackStack.at(CallbackStack.begin());
    //   if (*CallbackData == nullptr)
    //     return Res;
    //   CallbackStack.();

    //   Res = true;
    // }

    // return Res;
    return false;
  }
};

/**
 * @brief Queue class for submitting kernels to for execution
 */
class CHIPQueue {
protected:
  int Priority_;
  unsigned int Flags_;
  CHIPQueueType QueueType_;
  /// Device on which this queue will execute
  CHIPDevice *ChipDevice_;
  /// Context to which device belongs to
  CHIPContext *ChipContext_;

  CHIPEventMonitor *EventMonitor_ = nullptr;

  /** Keep track of what was the last event submitted to this queue. Required
   * for enforcing proper queue syncronization as per HIP/CUDA API. */
  CHIPEvent *LastEvent_ = nullptr;

public:
  // I want others to be able to lock this queue?
  std::mutex Mtx;

  virtual CHIPEvent *getLastEvent() = 0;
  void setLastEvent(CHIPEvent *ChipEv) {
    std::lock_guard<std::mutex> Lock(ChipEv->Mtx);
    ChipEv->increaseRefCount();
    LastEvent_ = ChipEv;
  }

  /**
   * @brief Construct a new CHIPQueue object
   *
   * @param chip_dev
   */
  CHIPQueue(CHIPDevice *ChipDev);
  /**
   * @brief Construct a new CHIPQueue object
   *
   * @param chip_dev
   * @param flags
   */
  CHIPQueue(CHIPDevice *ChipDev, unsigned int Flags);
  /**
   * @brief Construct a new CHIPQueue object
   *
   * @param chip_dev
   * @param flags
   * @param priority
   */
  CHIPQueue(CHIPDevice *ChipDev, unsigned int Flags, int Priority);
  /**
   * @brief Destroy the CHIPQueue object
   *
   */
  virtual ~CHIPQueue();

  CHIPQueueType getQueueType() { return QueueType_; }
  virtual void updateLastEvent(CHIPEvent *ChipEv) {
    if (LastEvent_ != nullptr)
      std::lock_guard<std::mutex> LockLast(LastEvent_->Mtx);

    std::lock_guard<std::mutex> LockNew(ChipEv->Mtx);
    assert(ChipEv);
    if (ChipEv == LastEvent_)
      return;
    // logDebug("CHIPQueue::updateLastEvent()");
    if (LastEvent_ != nullptr)
      LastEvent_->decreaseRefCount();
    ChipEv->increaseRefCount();
    LastEvent_ = ChipEv;
  }

  /**
   * @brief Blocking memory copy
   *
   * @param dst Destination
   * @param src Source
   * @param size Transfer size
   * @return hipError_t
   */
  virtual CHIPEvent *memCopyImpl(void *Dst, const void *Src, size_t Size);
  hipError_t memCopy(void *Dst, const void *Src, size_t Size);

  /**
   * @brief Non-blocking memory copy
   *
   * @param dst Destination
   * @param src Source
   * @param size Transfer size
   * @return hipError_t
   */
  virtual CHIPEvent *memCopyAsyncImpl(void *Dst, const void *Src,
                                      size_t Size) = 0;
  hipError_t memCopyAsync(void *Dst, const void *Src, size_t Size);

  /**
   * @brief Blocking memset
   *
   * @param dst
   * @param size
   * @param pattern
   * @param pattern_size
   */
  virtual CHIPEvent *memFillImpl(void *Dst, size_t Size, const void *Pattern,
                                 size_t PatternSize);
  virtual void memFill(void *Dst, size_t Size, const void *Pattern,
                       size_t PatternSize);

  /**
   * @brief Non-blocking mem set
   *
   * @param dst
   * @param size
   * @param pattern
   * @param pattern_size
   */
  virtual CHIPEvent *memFillAsyncImpl(void *Dst, size_t Size,
                                      const void *Pattern,
                                      size_t PatternSize) = 0;
  virtual void memFillAsync(void *Dst, size_t Size, const void *Pattern,
                            size_t PatternSize);

  // The memory copy 2D support
  virtual CHIPEvent *memCopy2DImpl(void *Dst, size_t DPitch, const void *Src,
                                   size_t Pitch, size_t Width, size_t Height);
  virtual void memCopy2D(void *Dst, size_t DPitch, const void *Src,
                         size_t SPitch, size_t Width, size_t Height);

  virtual CHIPEvent *memCopy2DAsyncImpl(void *Dst, size_t DPitch,
                                        const void *Src, size_t SPitch,
                                        size_t Width, size_t Height) = 0;
  virtual void memCopy2DAsync(void *Dst, size_t DPitch, const void *Src,
                              size_t SPitch, size_t Width, size_t Height);

  // The memory copy 3D support
  virtual CHIPEvent *memCopy3DImpl(void *Dst, size_t DPitch, size_t DSPitch,
                                   const void *Src, size_t Spitch,
                                   size_t SSPitch, size_t Width, size_t Height,
                                   size_t Depth);
  virtual void memCopy3D(void *Dst, size_t DPitch, size_t DSPitch,
                         const void *Src, size_t SPitch, size_t SSPitch,
                         size_t Width, size_t Height, size_t Depth);

  virtual CHIPEvent *memCopy3DAsyncImpl(void *Dst, size_t DPitch,
                                        size_t DSPitch, const void *Src,
                                        size_t SPitch, size_t SSPitch,
                                        size_t Width, size_t Height,
                                        size_t Depth) = 0;
  virtual void memCopy3DAsync(void *Dst, size_t DPitch, size_t DSPitch,
                              const void *Src, size_t SPitch, size_t SSPitch,
                              size_t Width, size_t Height, size_t Depth);

  // Memory copy to texture object, i.e. image
  virtual CHIPEvent *memCopyToTextureImpl(CHIPTexture *TexObj, void *Src) = 0;
  virtual void memCopyToTexture(CHIPTexture *TexObj, void *Src);

  /**
   * @brief Submit a CHIPExecItem to this queue for execution. CHIPExecItem
   * needs to be complete - contain the kernel and arguments
   *
   * @param exec_item
   * @return hipError_t
   */
  virtual CHIPEvent *launchImpl(CHIPExecItem *ExecItem) = 0;
  virtual void launch(CHIPExecItem *ExecItem);

  /**
   * @brief Get the Device obj
   *
   * @return CHIPDevice*
   */

  CHIPDevice *getDevice();
  /**
   * @brief Wait for this queue to finish.
   *
   */

  virtual void finish() = 0;
  /**
   * @brief Check if the queue is still actively executing
   *
   * @return true
   * @return false
   */

  bool query(); // TODO Depends on Events
  /**
   * @brief Get the Priority Range object defining the bounds for
   * hipStreamCreateWithPriority
   *
   * @param lower_or_upper 0 to get lower bound, 1 to get upper bound
   * @return int bound
   */

  int getPriorityRange(int LowerOrUpper); // TODO CHIP
  /**
   * @brief Insert an event into this queue
   *
   * @param e
   * @return true
   * @return false
   */
  virtual CHIPEvent *
  enqueueBarrierImpl(std::vector<CHIPEvent *> *EventsToWaitFor) = 0;
  virtual CHIPEvent *enqueueBarrier(std::vector<CHIPEvent *> *EventsToWaitFor);

  virtual CHIPEvent *enqueueMarkerImpl() = 0;
  CHIPEvent *enqueueMarker();

  /**
   * @brief Get the Flags object with which this queue was created.
   *
   * @return unsigned int
   */

  unsigned int getFlags(); // TODO CHIP
  /**
   * @brief Get the Priority object with which this queue was created.
   *
   * @return int
   */

  int getPriority(); // TODO CHIP
  /**
   * @brief Add a callback funciton to be called on the host after the specified
   * stream is done
   *
   * @param callback function pointer for a ballback function
   * @param userData
   * @return true
   * @return false
   */

  virtual bool addCallback(hipStreamCallback_t Callback, void *UserData);
  /**
   * @brief Insert a memory prefetch
   *
   * @param ptr
   * @param count
   * @return true
   * @return false
   */

  virtual CHIPEvent *memPrefetchImpl(const void *Ptr, size_t Count) = 0;
  void memPrefetch(const void *Ptr, size_t Count);

  /**
   * @brief Launch a kernel on this queue given a host pointer and arguments
   *
   * @param hostFunction
   * @param numBlocks
   * @param dimBlocks
   * @param args
   * @param sharedMemBytes
   */
  void launchHostFunc(const void *HostFunction, dim3 NumBlocks, dim3 DimBlocks,
                      void **Args, size_t SharedMemBytes);

  /**
   * @brief
   *
   * @param grid
   * @param block
   * @param sharedMemBytes
   * @param args
   * @param kernel
   * @return hipError_t
   */
  virtual void launchWithKernelParams(dim3 Grid, dim3 Block,
                                      unsigned int SharedMemBytes, void **Args,
                                      CHIPKernel *Kernel);

  /**
   * @brief
   *
   * @param grid
   * @param block
   * @param sharedMemBytes
   * @param extra
   * @param kernel
   * @return hipError_t
   */
  virtual void launchWithExtraParams(dim3 Grid, dim3 Block,
                                     unsigned int SharedMemBytes, void **Extra,
                                     CHIPKernel *Kernel);

  virtual void getBackendHandles(unsigned long *NativeInfo, int *Size) = 0;

  CHIPContext *getContext() { return ChipContext_; }
};

#endif
