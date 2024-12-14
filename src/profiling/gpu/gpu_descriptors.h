

#ifndef SRC_PROFILING_GPU_PROC_DESCRIPTORS_H_
#define SRC_PROFILING_GPU_PROC_DESCRIPTORS_H_

#include <sys/types.h>

#include <map>

#include "perfetto/base/task_runner.h"
#include "perfetto/ext/base/scoped_file.h"
#include "perfetto/ext/base/unix_socket.h"

namespace perfetto {

// Callback interface for receiving /dev/maliX file descriptors (proc-fds)
class DevDescriptorDelegate {
 public:
  virtual void OnDevDescriptors(card_id_t card_id,
                                 base::ScopedFile gpu_fd) = 0;

  virtual ~DevDescriptorDelegate();
};

class DevDescriptorGetter {
 public:
  virtual void GetDescriptorsForCardId(card_id_t card_id) = 0;
  virtual void SetDelegate(DevDescriptorDelegate* delegate) = 0;
  virtual bool RequiresDelayedRequest() { return false; }

  virtual ~DevDescriptorGetter();
};

// Directly opens /dev/maliX files. 
class DirectDescriptorGetter : public DevDescriptorGetter {
 public:
  void GetDescriptorsForCardId(pid_t pid) override;
  void SetDelegate(DevDescriptorDelegate* delegate) override;

  ~DirectDescriptorGetter() override;

 private:
  DevDescriptorDelegate* delegate_ = nullptr;
};


}  // namespace perfetto

#endif  // SRC_PROFILING_GPU_PROC_DESCRIPTORS_H_
