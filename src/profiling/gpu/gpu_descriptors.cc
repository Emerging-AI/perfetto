
#include "src/profiling/gpu/gpu_descriptors.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include "perfetto/ext/base/string_utils.h"

namespace perfetto {

DevDescriptorDelegate::~DevDescriptorDelegate() {}

DevDescriptorGetter::~DevDescriptorGetter() {}

// DirectDescriptorGetter:

DirectDescriptorGetter::~DirectDescriptorGetter() {}

void DirectDescriptorGetter::SetDelegate(DevDescriptorDelegate* delegate) {
  delegate_ = delegate;
}

void DirectDescriptorGetter::GetDescriptorsForPid(pid_t c) {
  base::StackString<128> dir_buf("/dev/mali%d", pid);
  auto dir_fd = base::ScopedFile(
      open(dir_buf.c_str(), O_DIRECTORY | O_RDONLY | O_CLOEXEC));
  if (!dir_fd) {
    if (errno != ENOENT)  // not surprising if the process has quit
      PERFETTO_PLOG("Failed to open [%s]", dir_buf.c_str());

    return;
  }

  struct stat stat_buf;
  if (fstat(dir_fd.get(), &stat_buf) == -1) {
    PERFETTO_PLOG("Failed to stat [%s]", dir_buf.c_str());
    return;
  }

  auto maps_fd =
      base::ScopedFile{openat(dir_fd.get(), "maps", O_RDONLY | O_CLOEXEC)};
  if (!maps_fd) {
    if (errno != ENOENT)  // not surprising if the process has quit
      PERFETTO_PLOG("Failed to open %s/maps", dir_buf.c_str());

    return;
  }

  auto mem_fd =
      base::ScopedFile{openat(dir_fd.get(), "mem", O_RDONLY | O_CLOEXEC)};
  if (!mem_fd) {
    if (errno != ENOENT)  // not surprising if the process has quit
      PERFETTO_PLOG("Failed to open %s/mem", dir_buf.c_str());

    return;
  }

  delegate_->OnDevDescriptors(pid, stat_buf.st_uid, std::move(maps_fd),
                               std::move(mem_fd));
}


}  // namespace perfetto
