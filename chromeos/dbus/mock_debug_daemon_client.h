// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_DEBUG_DAEMON_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_DEBUG_DAEMON_CLIENT_H_

#include "chromeos/dbus/debug_daemon_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockDebugDaemonClient : public DebugDaemonClient {
 public:
  MockDebugDaemonClient();
  virtual ~MockDebugDaemonClient();

  MOCK_METHOD2(GetDebugLogs, void(base::PlatformFile,
                                  const GetDebugLogsCallback&));
  MOCK_METHOD1(RequestStopSystemTracing,
      bool(const StopSystemTracingCallback&));
  MOCK_METHOD0(StartSystemTracing, void());
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_DEBUG_DAEMON_CLIENT_H_
