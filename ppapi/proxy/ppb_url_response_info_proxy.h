// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_URL_RESPONSE_INFO_PROXY_H_
#define PPAPI_PROXY_PPB_URL_RESPONSE_INFO_PROXY_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/host_resource.h"

struct PPB_URLResponseInfo;

namespace pp {
namespace proxy {

struct PPBFileRef_CreateInfo;
class SerializedVarReturnValue;

class PPB_URLResponseInfo_Proxy : public InterfaceProxy {
 public:
  PPB_URLResponseInfo_Proxy(Dispatcher* dispatcher,
                            const void* target_interface);
  virtual ~PPB_URLResponseInfo_Proxy();

  static const Info* GetInfo();

  // URLResponseInfo objects are actually created and returned by the
  // URLLoader. This function allows the URLLoader to convert a new
  // HostResource representing a response info to a properly tracked
  // URLReponseInfo PluginResource. Returns the plugin resource ID for the
  // new resource.
  static PP_Resource CreateResponseForResource(
      const ppapi::HostResource& resource);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgGetProperty(const ppapi::HostResource& response,
                        int32_t property,
                        SerializedVarReturnValue result);
  void OnMsgGetBodyAsFileRef(const ppapi::HostResource& response,
                             PPBFileRef_CreateInfo* result);

  DISALLOW_COPY_AND_ASSIGN(PPB_URLResponseInfo_Proxy);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PPB_URL_RESPONSE_INFO_PROXY_H_
