/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/

#include "RDKMediaEngine.h"

namespace WPEFramework {

    namespace Plugin {

        SERVICE_REGISTRATION(RDKMediaEngine, 1, 0);

        RDKMediaEngine::RDKMediaEngine()
        : _connectionId(0)
        , _notification(this)
        , _mediaEngine(nullptr)
        , _mediaEngineSink(this)
        , _id()
        , _idRefCnt(0)
        {
            RegisterAll();
        }

        RDKMediaEngine::~RDKMediaEngine()
        {
            UnregisterAll();
        }

        const string RDKMediaEngine::Initialize(PluginHost::IShell* service)
        {
            string message;
            _connectionId = 0;
            _service = service;

            // Register the Process::Notification stuff. The Remote process might die before we get a
            // change to "register" the sink for these events !!! So do it ahead of instantiation.
            _service->Register(&_notification);

            _mediaEngine = _service->Root<Exchange::IMediaEngine>(_connectionId, 2000, _T("AampEngineImplementation"));

            if (_mediaEngine) {
                SYSLOG(Logging::Startup, (_T("Successfully instantiated RDK Media Engine")));

                // Register events callback
                _mediaEngine->RegisterCallback(&_mediaEngineSink);

            } else {
                SYSLOG(Trace::Error, (_T("RDK Media engine could not be initialized.")));
                message = _T("RDK Media engine could not be initialized.");
                _service->Unregister(&_notification);
            }

            return message;
        }

        void RDKMediaEngine::Deinitialize(PluginHost::IShell* service)
        {
            SYSLOG(Logging::Shutdown, (_T("Deinitializing ")));
            ASSERT(_service == service);
            ASSERT(_mediaEngine != nullptr);

            service->Unregister(&_notification);

            if (_mediaEngine->Release() != Core::ERROR_DESTRUCTION_SUCCEEDED) {

                ASSERT(_connectionId != 0);

                SYSLOG(Trace::Error, (_T("OutOfProcess Plugin is not properly destructed. PID: %d"), _connectionId));

                RPC::IRemoteConnection* connection(_service->RemoteConnection(_connectionId));

                // The connection can disappear in the meantime...
                if (connection != nullptr) {

                    // But if it did not dissapear in the meantime, forcefully terminate it. Shoot to kill :-)
                    connection->Terminate();
                    connection->Release();
                }
            }

            _mediaEngine = nullptr;
            _service = nullptr;
        }

        string RDKMediaEngine::Information() const
        {
            // No additional info to report.
            return (string());
        }

        void RDKMediaEngine::Deactivated(RPC::IRemoteConnection* connection)
        {
            // This can potentially be called on a socket thread, so the deactivation (wich in turn kills this object) must be done
            // on a seperate thread. Also make sure this call-stack can be unwound before we are totally destructed.
            if (_connectionId == connection->Id()) {
                SYSLOG(Logging::Shutdown, (_T("Deactivating")));
                ASSERT(_service != nullptr);
                Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(_service, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
            }
        }
    }
}
