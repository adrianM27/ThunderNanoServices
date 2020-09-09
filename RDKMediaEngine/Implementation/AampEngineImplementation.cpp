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

#include "AampEngineImplementation.h"

namespace WPEFramework {

    namespace Plugin {

        SERVICE_REGISTRATION(AampEngineImplementation, 1, 0);

        AampEngineImplementation::AampEngineImplementation()
        : _adminLock()
        , _callback(nullptr)
        , _initialized(false)
        , _aampPlayer(nullptr)
        , _aampEventListener(nullptr)
        , _aampGstPlayerMainLoop(nullptr)
        {
            //temporary back door for AAMP configuration
            Core::SystemInfo::SetEnvironment(_T("AAMP_ENABLE_OPT_OVERRIDE"), _T("1"));
            //TODO: should be set accordingly to platform set-up
            Core::SystemInfo::SetEnvironment(_T("AAMP_ENABLE_WESTEROS_SINK"), _T("1"));
        }

        AampEngineImplementation::~AampEngineImplementation()
        {
            SYSLOG(Logging::Shutdown, (_T("Destructing")));
            _adminLock.Lock();
            DeinitializePlayerInstance();
            ASSERT(_aampPlayer == nullptr);
            ASSERT(_aampEventListener == nullptr);
            ASSERT(_aampGstPlayerMainLoop == nullptr);
        }

        uint32_t AampEngineImplementation::Create(const string& id)
        {
            SYSLOG(Logging::Notification, (_T("Create with id: %s"), id.c_str()));
            _adminLock.Lock();
            if(id == _id && _initialized)
            {
                _adminLock.Unlock();
                return Core::ERROR_NONE;
            }

            if(_initialized)
            {
                DeinitializePlayerInstance();
                _initialized = false;
            }

            uint32_t result = InitializePlayerInstance();
            if(result == Core::ERROR_NONE)
            {
                _initialized = true;
                _id = id;
            }

            _adminLock.Unlock();

            return result;
        }

        uint32_t AampEngineImplementation::Destroy(const string& id)
        {
            SYSLOG(Logging::Notification, (_T("Destroy with id=%s"), id.c_str()));
            _adminLock.Lock();
            if(_initialized)
            {
                DeinitializePlayerInstance();
                _initialized = false;
            }

            _id.clear();
            _adminLock.Unlock();
            return Core::ERROR_NONE;
        }

        uint32_t AampEngineImplementation::Load(const string& id, const string& url, bool autoPlay)
        {
            SYSLOG(Logging::Notification, (_T("Load with id=%s, url=%s, autoPlay=%d"),
                    id.c_str(), url.c_str(), autoPlay));
            _adminLock.Lock();
            uint32_t result = AssertPlayerStateAndIdNoBlocking(id);
            if(result != Core::ERROR_NONE)
            {
                _adminLock.Unlock();
                return result;
            }

            ASSERT(_aampPlayer != nullptr);
            _aampPlayer->Tune(url.c_str(), autoPlay);

            _adminLock.Unlock();
            return Core::ERROR_NONE;
        }

        uint32_t AampEngineImplementation::Play(const string& id)
        {
            SYSLOG(Logging::Notification, (_T("Play with id=%s"), id.c_str()));
            _adminLock.Lock();
            uint32_t result = AssertPlayerStateAndIdNoBlocking(id);
            if(result != Core::ERROR_NONE)
            {
                _adminLock.Unlock();
                return result;
            }

            ASSERT(_aampPlayer != nullptr);
            _aampPlayer->SetRate(1);

            _adminLock.Unlock();
            return Core::ERROR_NONE;
        }

        uint32_t AampEngineImplementation::Pause(const string& id)
        {
            SYSLOG(Logging::Notification, (_T("Pause with id=%s"), id.c_str()));
            _adminLock.Lock();
            uint32_t result = AssertPlayerStateAndIdNoBlocking(id);
            if(result != Core::ERROR_NONE)
            {
                _adminLock.Unlock();
                return result;
            }

            ASSERT(_aampPlayer != nullptr);
            _aampPlayer->SetRate(0);

            _adminLock.Unlock();
            return Core::ERROR_NONE;
        }

        uint32_t AampEngineImplementation::SeekTo(const string& id, int32_t positionSec)
        {
            SYSLOG(Logging::Notification, (_T("SetPosition with id=%s"),id.c_str()));
            _adminLock.Lock();
            uint32_t result = AssertPlayerStateAndIdNoBlocking(id);
            if(result != Core::ERROR_NONE)
            {
                _adminLock.Unlock();
                return result;
            }

            ASSERT(_aampPlayer != nullptr);
            _aampPlayer->Seek(static_cast<double>(positionSec));

            _adminLock.Unlock();
            return Core::ERROR_NONE;
        }

        uint32_t AampEngineImplementation::Stop(const string& id)
        {
            SYSLOG(Logging::Notification, (_T("Stop with id=%s"), id.c_str()));
            _adminLock.Lock();
            uint32_t result = AssertPlayerStateAndIdNoBlocking(id);
            if(result != Core::ERROR_NONE)
            {
                _adminLock.Unlock();
                return result;
            }

            ASSERT(_aampPlayer != nullptr);
            _aampPlayer->Stop();

            _adminLock.Unlock();
            return Core::ERROR_NONE;
        }

        uint32_t AampEngineImplementation::InitConfig(const string& id, const string& configurationJson)
        {
            SYSLOG(Logging::Notification, (_T("InitConfig with id=%s and config=%s"),
                    id.c_str(), configurationJson.c_str()));
            _adminLock.Lock();
            //TODO: no parameter supported right now
            _adminLock.Unlock();
            return Core::ERROR_NONE;
        }

        uint32_t AampEngineImplementation::InitDRMConfig(const string& id, const string& configurationJson)
        {
            SYSLOG(Logging::Notification, (_T("InitDRMConfig with id=%s and config=%s"),
                    id.c_str(), configurationJson.c_str()));
            _adminLock.Lock();
            //TODO: no parameter supported right now
            _adminLock.Unlock();
            return Core::ERROR_NONE;
        }

        void AampEngineImplementation::RegisterCallback(IMediaEngine::ICallback* callback)
        {
            SYSLOG(Logging::Notification, (_T("RegisterCallback")));
            _adminLock.Lock();

            if (_callback != nullptr) {
                _callback->Release();
            }
            if (callback != nullptr) {
                callback->AddRef();
            }
            _callback = callback;
            _adminLock.Unlock();
        }

        void AampEngineImplementation::SendEvent(const string& eventName, const string& parameters)
        {
            SYSLOG(Logging::Notification, (_T("SendEvent for id=%s, eventName=%s, parameters=%s"),
                    _id.c_str(), eventName.c_str(), parameters.c_str()));
            _adminLock.Lock();
            if(!_callback)
            {
                SYSLOG(Trace::Error, (_T("SendEvent: callback is null")));
                _adminLock.Unlock();
                return;
            }
            _callback->Event(_id, eventName, parameters);
            _adminLock.Unlock();
        }

        // Thread overrides

        uint32_t AampEngineImplementation::Worker()
        {
            if (_aampGstPlayerMainLoop) {
                g_main_loop_run(_aampGstPlayerMainLoop); // blocks
                g_main_loop_unref(_aampGstPlayerMainLoop);
                _aampGstPlayerMainLoop = nullptr;
            }
            return WPEFramework::Core::infinite;
        }

        uint32_t AampEngineImplementation::InitializePlayerInstance()
        {
            ASSERT(_aampPlayer == nullptr);
            ASSERT(_aampEventListener == nullptr);
            ASSERT(_aampGstPlayerMainLoop == nullptr);

            gst_init(0, nullptr);
            _aampPlayer = new PlayerInstanceAAMP();
            if(_aampPlayer == nullptr)
            {
                return Core::ERROR_GENERAL;
            }

            _aampEventListener = new AampEventListener(this);
            if(_aampEventListener == nullptr)
            {
                //cleanup
                DeinitializePlayerInstance();
                return Core::ERROR_GENERAL;
            }

            _aampPlayer->RegisterEvents(_aampEventListener);
            _aampPlayer->SetReportInterval(1000 /* ms */);

            _aampGstPlayerMainLoop = g_main_loop_new(nullptr, false);

            // Run thread with _aampGstPlayerMainLoop
            Run();

            return Core::ERROR_NONE;
        }

        uint32_t AampEngineImplementation::AssertPlayerStateAndIdNoBlocking(const string& id)
        {
            if(!_initialized)
            {
                SYSLOG(Trace::Error, (_T("Player is uninitialized, call Create method first!")));
                return Core::ERROR_ILLEGAL_STATE;
            }

            if(_id != id)
            {
                SYSLOG(Trace::Error, (_T("Instace ID is incorrect! Current: %s, requested %s"),
                        _id.c_str(), id.c_str()));
                return Core::ERROR_UNAVAILABLE;
            }

            return Core::ERROR_NONE;
        }

        void AampEngineImplementation::DeinitializePlayerInstance()
        {
            if(_aampPlayer)
            {
                _aampPlayer->Stop();
            }

            _adminLock.Unlock();
            Block();
            if(_aampGstPlayerMainLoop)
            {
                g_main_loop_quit(_aampGstPlayerMainLoop);
            }

            Wait(Thread::BLOCKED | Thread::STOPPED, Core::infinite);

            _adminLock.Lock();

            if(_aampPlayer)
            {
                _aampPlayer->RegisterEvents(nullptr);
                delete _aampPlayer;
                _aampPlayer = nullptr;
            }

            if(_aampEventListener)
            {
                delete _aampEventListener;
                _aampEventListener = nullptr;
            }
        }

        AampEngineImplementation::AampEventListener::AampEventListener(AampEngineImplementation* player)
        : _player(player)
        {
            ASSERT(_player != nullptr);
        }

        AampEngineImplementation::AampEventListener::~AampEventListener()
        {
        }

        void AampEngineImplementation::AampEventListener::Event(const AAMPEvent& event)
        {
            SYSLOG(Logging::Notification, (_T("Event: handling event: %d"), event.type));
            switch(event.type)
            {
                case AAMP_EVENT_TUNED:
                    HandlePlaybackStartedEvent();
                    break;
                case AAMP_EVENT_TUNE_FAILED:
                    HandlePlaybackFailed(event);
                    break;
                case AAMP_EVENT_SPEED_CHANGED:
                    HandlePlaybackSpeedChanged(event);
                    break;
                case AAMP_EVENT_PROGRESS:
                    HandlePlaybackProgressUpdateEvent(event);
                    break;
                case AAMP_EVENT_STATE_CHANGED:
                    HandlePlaybackStateChangedEvent(event);
                    break;
                case AAMP_EVENT_BUFFERING_CHANGED:
                    HandleBufferingChangedEvent(event);
                    break;
                default:
                    SYSLOG(Logging::Notification, (_T("Event: AAMP event is not supported: %d"), event.type));
            }
        }

        void AampEngineImplementation::AampEventListener::HandlePlaybackStartedEvent()
        {
            _player->SendEvent(_T("playbackStarted"), string());
        }

        void AampEngineImplementation::AampEventListener::HandlePlaybackStateChangedEvent(const AAMPEvent& event)
        {
            JsonObject parameters;
            parameters[_T("state")] = static_cast<int>(event.data.stateChanged.state);

            string s;
            parameters.ToString(s);
            _player->SendEvent(_T("playbackStateChanged"), s);
        }

        void AampEngineImplementation::AampEventListener::HandlePlaybackProgressUpdateEvent(const AAMPEvent& event)
        {
            JsonObject parameters;
            parameters[_T("durationMiliseconds")] = static_cast<int>(event.data.progress.durationMiliseconds);
            parameters[_T("positionMiliseconds")] = static_cast<int>(event.data.progress.positionMiliseconds);
            parameters[_T("playbackSpeed")] = static_cast<int>(event.data.progress.playbackSpeed);
            parameters[_T("startMiliseconds")] = static_cast<int>(event.data.progress.startMiliseconds);
            parameters[_T("endMiliseconds")] = static_cast<int>(event.data.progress.endMiliseconds);

            string s;
            parameters.ToString(s);
            _player->SendEvent(_T("playbackProgressUpdate"), s);
        }

        void AampEngineImplementation::AampEventListener::HandleBufferingChangedEvent(const AAMPEvent& event)
        {
            JsonObject parameters;
            parameters[_T("buffering")] = event.data.bufferingChanged.buffering;

            string s;
            parameters.ToString(s);
            _player->SendEvent(_T("bufferingChanged"), s);
        }

        void AampEngineImplementation::AampEventListener::HandlePlaybackSpeedChanged(const AAMPEvent& event)
        {
            JsonObject parameters;
            parameters[_T("speed")] = event.data.speedChanged.rate;

            string s;
            parameters.ToString(s);
            _player->SendEvent(_T("playbackSpeedChanged"), s);
        }

        void AampEngineImplementation::AampEventListener::HandlePlaybackFailed(const AAMPEvent& event)
        {
            JsonObject parameters;
            parameters[_T("shouldRetry")] = event.data.mediaError.shouldRetry;
            parameters[_T("code")] = event.data.mediaError.code;
            parameters[_T("description")] = string(event.data.mediaError.description);

            string s;
            parameters.ToString(s);
            _player->SendEvent(_T("playbackFailed"), s);
        }


    }//Plugin
}//WPEFramework
