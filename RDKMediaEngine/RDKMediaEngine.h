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

#pragma once

#include "Module.h"
#include <interfaces/IMediaEngine.h>

namespace WPEFramework {
    namespace Plugin {

        class RDKMediaEngine : public PluginHost::IPlugin, public PluginHost::JSONRPC {
        private:
            RDKMediaEngine(const RDKMediaEngine&) = delete;
            RDKMediaEngine& operator=(const RDKMediaEngine&) = delete;

            class Notification : public RPC::IRemoteConnection::INotification {
            private:
                Notification() = delete;
                Notification(const Notification&) = delete;
                Notification& operator=(const Notification&) = delete;

            public:
                explicit Notification(RDKMediaEngine* parent)
                : _parent(*parent)
                {
                    ASSERT(parent != nullptr);
                }
                virtual ~Notification()
                {
                }

            public:
                virtual void Activated(RPC::IRemoteConnection* connection)
                {
                }
                virtual void Deactivated(RPC::IRemoteConnection* connection)
                {
                    _parent.Deactivated(connection);
                }

                BEGIN_INTERFACE_MAP(Notification)
                INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                END_INTERFACE_MAP

            private:
                RDKMediaEngine& _parent;
            };

            class MediaEngineSink : public Exchange::IMediaEngine::ICallback
            {
            private:
                MediaEngineSink() = delete;
                MediaEngineSink(const MediaEngineSink&) = delete;
                MediaEngineSink& operator=(const MediaEngineSink&) = delete;

            public:
                MediaEngineSink(RDKMediaEngine* parent)
            : _parent(*parent)
            {
                    ASSERT(parent != nullptr);
            }

                ~MediaEngineSink() override
                {
                }

            public:
                void Event(const string& id, const string &eventName, const string &parametersJson) override
                {
                    _parent.onMediaEngineEvent(id, eventName, parametersJson);
                }

                BEGIN_INTERFACE_MAP(MediaEngineSink)
                INTERFACE_ENTRY(Exchange::IMediaEngine::ICallback)
                END_INTERFACE_MAP

            private:
                RDKMediaEngine& _parent;
            };

            public:
            RDKMediaEngine();
            ~RDKMediaEngine() override;

            // JsonRpc handlers
            uint32_t create(const JsonObject& parameters, JsonObject& response);
            uint32_t release(const JsonObject& parameters, JsonObject& response);
            uint32_t load(const JsonObject& parameters, JsonObject& response);
            uint32_t play(const JsonObject& parameters, JsonObject& response);
            uint32_t pause(const JsonObject& parameters, JsonObject& response);
            uint32_t seekTo(const JsonObject& parameters, JsonObject& response);
            uint32_t stop(const JsonObject& parameters, JsonObject& response);
            uint32_t initConfig(const JsonObject& parameters, JsonObject& response);
            uint32_t initDRMConfig(const JsonObject& parameters, JsonObject& response);

            void onMediaEngineEvent(const string& id, const string &eventName, const string &parameters);

            public:
            BEGIN_INTERFACE_MAP(RDKMediaEngine)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
            END_INTERFACE_MAP

            // IPlugin methods
            const string Initialize(PluginHost::IShell* service) override;
            void Deinitialize(PluginHost::IShell* service) override;
            string Information() const override;

            private:
            void Deactivated(RPC::IRemoteConnection* connection);

            // JsonRpc
            void RegisterAll();
            void UnregisterAll();

            private:
            uint32_t _connectionId;
            PluginHost::IShell* _service;
            Core::Sink<Notification> _notification;

            // Media Engine with AAMP implementation running in separate process
            //TODO: consider list of different MediaEngines
            Exchange::IMediaEngine* _mediaEngine;
            Core::Sink<MediaEngineSink> _mediaEngineSink;
            //TODO: for multi-instance case consider map of ID's
            string _id;
            uint32_t _idRefCnt;
        };

    } //namespace Plugin
} //namespace WPEFramework
