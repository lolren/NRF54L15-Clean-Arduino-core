#ifndef BLUEFRUIT_COMPAT_INTERNALFILESYSTEM_H_
#define BLUEFRUIT_COMPAT_INTERNALFILESYSTEM_H_

class InternalFSCompat {
 public:
  bool begin() { return true; }
};

static InternalFSCompat InternalFS;

#endif  // BLUEFRUIT_COMPAT_INTERNALFILESYSTEM_H_
