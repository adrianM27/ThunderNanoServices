/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
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
 */

#pragma once

#include "Module.h"
#include <interfaces/IMediaEngine.h>
#include <gst/gst.h>
#include <main_aamp.h>
#include <interfaces/IStream.h>

namespace WPEFramework {

    namespace Plugin {

        class AampEngineImplementation : public Exchange::IMediaEngine, Core::Thread {

        public:
            AampEngineImplementation();
            ~AampEngineImplementation() override;

            // IMediaEngine Interfaces
            virtual uint32_t Create(const string& id) override;
            virtual uint32_t Destroy(const string& id);
            virtual uint32_t Load(const string& id, const string& url, bool autoPlay) override;
            virtual uint32_t Play(const string& id) override;
            virtual uint32_t Pause(const string& id) override;
            virtual uint32_t SeekTo(const string& id, int32_t positionSec) override;
            virtual uint32_t Stop(const string& id) override;
            virtual uint32_t InitConfig(const string& id, const string& configurationJson) override;
            virtual uint32_t InitDRMConfig(const string& id, const string& configurationJson) override;
            void RegisterCallback(IMediaEngine::ICallback* callback) override;

            BEGIN_INTERFACE_MAP(AampEngineImplementation)
            INTERFACE_ENTRY(Exchange::IMediaEngine)
            END_INTERFACE_MAP

        private:

            // Thread Interface
            uint32_t Worker() override;

            AampEngineImplementation(const AampEngineImplementation&) = delete;
            AampEngineImplementation& operator=(const AampEngineImplementation&) = delete;

            //Helper types/classes
            typedef struct _GMainLoop GMainLoop;
            class AampEventListener : public AAMPEventListener {
            public:
                AampEventListener() = delete;
                AampEventListener(const AampEventListener&) = delete;
                AampEventListener& operator=(const AampEventListener&) = delete;

                AampEventListener(AampEngineImplementation* player);
                ~AampEventListener() override;

                void Event(const AAMPEvent& event) override;
            private:

                void HandlePlaybackStartedEvent();
                void HandlePlaybackStateChangedEvent(const AAMPEvent& event);
                void HandlePlaybackProgressUpdateEvent(const AAMPEvent& event);
                void HandleBufferingChangedEvent(const AAMPEvent& event);
                void HandlePlaybackSpeedChanged(const AAMPEvent& event);
                void HandlePlaybackFailed(const AAMPEvent& event);

                AampEngineImplementation* _player;
            };

            void SendEvent(const string& eventName, const string& parameters);
            void DeinitializePlayerInstanceNoBlocking();
            uint32_t InitializePlayerInstanceNoBlocking();
            uint32_t AssertPlayerStateAndIdNoBlocking(const string& id);

            mutable Core::CriticalSection _adminLock;
            IMediaEngine::ICallback *_callback;

            //TODO: for multi-instances case consider separate instance class with thread
            bool _initialized;
            string _id;
            PlayerInstanceAAMP* _aampPlayer;
            AampEventListener *_aampEventListener;
            GMainLoop *_aampGstPlayerMainLoop;
        };

    } //Plugin
} //WPEFramework

