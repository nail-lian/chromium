// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/mac/mock_launchd.h"

#include <CoreFoundation/CoreFoundation.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/mac/launchd.h"
#include "chrome/common/service_process_util.h"
#include "testing/gtest/include/gtest/gtest.h"

static sockaddr_un* throwaway_sockaddr_un;
static const size_t kMaxPipeNameLength =
    sizeof(throwaway_sockaddr_un->sun_path);

// static
bool MockLaunchd::MakeABundle(const FilePath& dst,
                              const std::string& name,
                              FilePath* bundle_root,
                              FilePath* executable) {
  *bundle_root = dst.Append(name + std::string(".app"));
  FilePath contents = bundle_root->AppendASCII("Contents");
  FilePath mac_os = contents.AppendASCII("MacOS");
  *executable = mac_os.Append(name);
  FilePath info_plist = contents.Append("Info.plist");

  if (!file_util::CreateDirectory(mac_os)) {
    return false;
  }
  const char *data = "#! testbundle\n";
  int len = strlen(data);
  if (file_util::WriteFile(*executable, data, len) != len) {
    return false;
  }
  if (chmod(executable->value().c_str(), 0555) != 0) {
    return false;
  }

  chrome::VersionInfo version_info;

  const char* info_plist_format =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
          "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
      "<plist version=\"1.0\">\n"
      "<dict>\n"
      "  <key>CFBundleDevelopmentRegion</key>\n"
      "  <string>English</string>\n"
      "  <key>CFBundleExecutable</key>\n"
      "  <string>%s</string>\n"
      "  <key>CFBundleIdentifier</key>\n"
      "  <string>com.test.%s</string>\n"
      "  <key>CFBundleInfoDictionaryVersion</key>\n"
      "  <string>6.0</string>\n"
      "  <key>CFBundleShortVersionString</key>\n"
      "  <string>%s</string>\n"
      "  <key>CFBundleVersion</key>\n"
      "  <string>1</string>\n"
      "</dict>\n"
      "</plist>\n";
  std::string info_plist_data =
      base::StringPrintf(info_plist_format,
                         name.c_str(),
                         name.c_str(),
                         version_info.Version().c_str());
  len = info_plist_data.length();
  if (file_util::WriteFile(info_plist, info_plist_data.c_str(), len) != len) {
    return false;
  }
  const UInt8* bundle_root_path =
      reinterpret_cast<const UInt8*>(bundle_root->value().c_str());
  base::mac::ScopedCFTypeRef<CFURLRef> url(
      CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
                                              bundle_root_path,
                                              bundle_root->value().length(),
                                              true));
  base::mac::ScopedCFTypeRef<CFBundleRef> bundle(
      CFBundleCreate(kCFAllocatorDefault, url));
  return bundle.get();
}

MockLaunchd::MockLaunchd(const FilePath& file, MessageLoop* loop,
                         bool create_socket, bool as_service)
    : file_(file),
      message_loop_(loop),
      create_socket_(create_socket),
      as_service_(as_service),
      restart_called_(false),
      remove_called_(false),
      job_called_(false),
      checkin_called_(false),
      write_called_(false),
      delete_called_(false) {
  std::string pipe_suffix("_SOCKET");
  FilePath socket_path = file_;
  while (socket_path.value().length() + pipe_suffix.length() >
         kMaxPipeNameLength - 2) {
    socket_path = socket_path.DirName();
  }
  pipe_name_ = socket_path.value() + pipe_suffix;
}

MockLaunchd::~MockLaunchd() {
}

CFDictionaryRef MockLaunchd::CopyExports() {
  if (!create_socket_) {
    ADD_FAILURE();
    return NULL;
  }

  CFStringRef env_var =
      base::mac::NSToCFCast(GetServiceProcessLaunchDSocketEnvVar());
  base::mac::ScopedCFTypeRef<CFStringRef> socket_path(
      CFStringCreateWithCString(kCFAllocatorDefault, pipe_name_.c_str(),
                                kCFStringEncodingUTF8));
  const void *keys[] = { env_var };
  const void *values[] = { socket_path };
  COMPILE_ASSERT(arraysize(keys) == arraysize(values), array_sizes_must_match);
  return CFDictionaryCreate(kCFAllocatorDefault,
                            keys,
                            values,
                            arraysize(keys),
                            &kCFTypeDictionaryKeyCallBacks,
                            &kCFTypeDictionaryValueCallBacks);
}

CFDictionaryRef MockLaunchd::CopyJobDictionary(CFStringRef label) {
  if (!as_service_) {
    scoped_ptr<MultiProcessLock> running_lock(
        TakeNamedLock(pipe_name_, false));
    if (running_lock.get())
      return NULL;
  }

  CFStringRef program = CFSTR(LAUNCH_JOBKEY_PROGRAM);
  CFStringRef program_pid = CFSTR(LAUNCH_JOBKEY_PID);
  const void *keys[] = { program, program_pid };
  base::mac::ScopedCFTypeRef<CFStringRef> path(
      base::SysUTF8ToCFStringRef(file_.value()));
  int process_id = base::GetCurrentProcId();
  base::mac::ScopedCFTypeRef<CFNumberRef> pid(
      CFNumberCreate(NULL, kCFNumberIntType, &process_id));
  const void *values[] = { path, pid };
  COMPILE_ASSERT(arraysize(keys) == arraysize(values), array_sizes_must_match);
  return CFDictionaryCreate(kCFAllocatorDefault,
                            keys,
                            values,
                            arraysize(keys),
                            &kCFTypeDictionaryKeyCallBacks,
                            &kCFTypeDictionaryValueCallBacks);
}

CFDictionaryRef MockLaunchd::CopyDictionaryByCheckingIn(CFErrorRef* error) {
  checkin_called_ = true;
  CFStringRef program = CFSTR(LAUNCH_JOBKEY_PROGRAM);
  CFStringRef program_args = CFSTR(LAUNCH_JOBKEY_PROGRAMARGUMENTS);
  base::mac::ScopedCFTypeRef<CFStringRef> path(
      base::SysUTF8ToCFStringRef(file_.value()));
  const void *array_values[] = { path.get() };
  base::mac::ScopedCFTypeRef<CFArrayRef> args(
      CFArrayCreate(kCFAllocatorDefault,
                    array_values,
                    1,
                    &kCFTypeArrayCallBacks));

  if (!create_socket_) {
    const void *keys[] = { program, program_args };
    const void *values[] = { path, args };
    COMPILE_ASSERT(arraysize(keys) == arraysize(values),
                   array_sizes_must_match);
    return CFDictionaryCreate(kCFAllocatorDefault,
                              keys,
                              values,
                              arraysize(keys),
                              &kCFTypeDictionaryKeyCallBacks,
                              &kCFTypeDictionaryValueCallBacks);
  }

  CFStringRef socket_key = CFSTR(LAUNCH_JOBKEY_SOCKETS);
  int local_pipe = -1;
  EXPECT_TRUE(as_service_);

  // Create unix_addr structure.
  struct sockaddr_un unix_addr = {0};
  unix_addr.sun_family = AF_UNIX;
  size_t path_len =
      base::strlcpy(unix_addr.sun_path, pipe_name_.c_str(), kMaxPipeNameLength);
  DCHECK_EQ(pipe_name_.length(), path_len);
  unix_addr.sun_len = SUN_LEN(&unix_addr);

  CFSocketSignature signature;
  signature.protocolFamily = PF_UNIX;
  signature.socketType = SOCK_STREAM;
  signature.protocol = 0;
  size_t unix_addr_len = offsetof(struct sockaddr_un,
                                  sun_path) + path_len + 1;
  base::mac::ScopedCFTypeRef<CFDataRef> address(
      CFDataCreate(NULL, reinterpret_cast<UInt8*>(&unix_addr), unix_addr_len));
  signature.address = address;

  CFSocketRef socket =
      CFSocketCreateWithSocketSignature(NULL, &signature, 0, NULL, NULL);

  local_pipe = CFSocketGetNative(socket);
  EXPECT_NE(-1, local_pipe);
  if (local_pipe == -1) {
    if (error) {
      *error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainPOSIX,
                             errno, NULL);
    }
    return NULL;
  }

  base::mac::ScopedCFTypeRef<CFNumberRef> socket_fd(
      CFNumberCreate(NULL, kCFNumberIntType, &local_pipe));
  const void *socket_array_values[] = { socket_fd };
  base::mac::ScopedCFTypeRef<CFArrayRef> sockets(
      CFArrayCreate(kCFAllocatorDefault,
                    socket_array_values,
                    1,
                    &kCFTypeArrayCallBacks));
  CFStringRef socket_dict_key = CFSTR("ServiceProcessSocket");
  const void *socket_keys[] = { socket_dict_key };
  const void *socket_values[] = { sockets };
  COMPILE_ASSERT(arraysize(socket_keys) == arraysize(socket_values),
                 socket_array_sizes_must_match);
  base::mac::ScopedCFTypeRef<CFDictionaryRef> socket_dict(
      CFDictionaryCreate(kCFAllocatorDefault,
                         socket_keys,
                         socket_values,
                         arraysize(socket_keys),
                         &kCFTypeDictionaryKeyCallBacks,
                         &kCFTypeDictionaryValueCallBacks));
  const void *keys[] = { program, program_args, socket_key };
  const void *values[] = { path, args, socket_dict };
  COMPILE_ASSERT(arraysize(keys) == arraysize(values), array_sizes_must_match);
  return CFDictionaryCreate(kCFAllocatorDefault,
                            keys,
                            values,
                            arraysize(keys),
                            &kCFTypeDictionaryKeyCallBacks,
                            &kCFTypeDictionaryValueCallBacks);
}

bool MockLaunchd::RemoveJob(CFStringRef label, CFErrorRef* error) {
  remove_called_ = true;
  message_loop_->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  return true;
}

bool MockLaunchd::RestartJob(Domain domain,
                             Type type,
                             CFStringRef name,
                             CFStringRef session_type) {
  restart_called_ = true;
  message_loop_->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  return true;
}

CFMutableDictionaryRef MockLaunchd::CreatePlistFromFile(
    Domain domain,
    Type type,
    CFStringRef name)  {
  base::mac::ScopedCFTypeRef<CFDictionaryRef> dict(
      CopyDictionaryByCheckingIn(NULL));
  return CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, dict);
}

bool MockLaunchd::WritePlistToFile(Domain domain,
                                   Type type,
                                   CFStringRef name,
                                   CFDictionaryRef dict) {
  write_called_ = true;
  return true;
}

bool MockLaunchd::DeletePlist(Domain domain,
                              Type type,
                              CFStringRef name) {
  delete_called_ = true;
  return true;
}

void MockLaunchd::SignalReady() {
  ASSERT_TRUE(as_service_);
  running_lock_.reset(TakeNamedLock(pipe_name_, true));
}
