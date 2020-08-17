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
, _engine()
{
    ASSERT(_initialized == false);
    ASSERT(_aampPlayer == nullptr);
    ASSERT(_aampGstPlayerMainLoop == nullptr);
    SYSLOG(Trace::Error, (_T("AampEngineImplementation()")));
    _engine = Core::ProxyType<RPC::InvokeServer>::Create(&Core::IWorkerPool::Instance());
}

AampEngineImplementation::~AampEngineImplementation()
{
    ASSERT(_aampPlayer == nullptr);
    ASSERT(_aampEventListener == nullptr);
    SYSLOG(Trace::Error, (_T("~AampEngineImplementation()")));
}

uint32_t AampEngineImplementation::Create(uint32_t id)
{
    Core::SystemInfo::SetEnvironment(_T("AAMP_USE_WESTEROSSINK"), _T("true"));
    SYSLOG(Trace::Error, (_T("Create")));
    _adminLock.Lock();
    if(_initialized)
    {
        DeinitializePlayerInstanceNoBlocking();
        _initialized = false;
    }

    uint32_t result = InitializePlayerInstanceNoBlocking();
    if(result == Core::ERROR_NONE)
    {
        _initialized = true;
        _id = id;
    }

    _adminLock.Unlock();

    return result;
}

uint32_t AampEngineImplementation::Load(uint32_t id, const string& configuration)
{
    SYSLOG(Trace::Error, (_T("Load with uri = %s"), configuration.c_str()));
    _adminLock.Lock();
    SYSLOG(Trace::Error, (_T("1 Load with uri = %s"), configuration.c_str()));
    uint32_t result = AssertPlayerStateAndIdNoBlocking(id);
    if(result != Core::ERROR_NONE)
    {
        _adminLock.Unlock();
        return result;
    }

    SYSLOG(Trace::Error, (_T("2Load with uri = %s"), configuration.c_str()));
    ASSERT(_aampPlayer != nullptr);
    // Tune with autoPlay=false
    SYSLOG(Trace::Error, (_T("3Load with uri = %s"), configuration.c_str()));

    _aampPlayer->Tune(configuration.c_str(), false);

    SYSLOG(Trace::Error, (_T("4Load with uri = %s"), configuration.c_str()));
    _adminLock.Unlock();
    return Core::ERROR_NONE;
}

uint32_t AampEngineImplementation::Play(uint32_t id)
{
    SYSLOG(Trace::Error, (_T("Play")));
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

uint32_t AampEngineImplementation::Pause(uint32_t id)
{
    SYSLOG(Trace::Error, (_T("Pause")));
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

uint32_t AampEngineImplementation::SetPosition(uint32_t id, int32_t positionSec)
{
    SYSLOG(Trace::Error, (_T("SetPosition")));
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

uint32_t AampEngineImplementation::Stop(uint32_t id)
{
    SYSLOG(Trace::Error, (_T("Stop")));
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

void AampEngineImplementation::RegisterCallback(IMediaEngine::ICallback* callback)
{
    SYSLOG(Trace::Error, (_T("RegisterCallback")));
    _adminLock.Lock();
    _callback = callback;

    // Works fine here but not later(?)
    string eventName("test");
    string pld("{}");
    _callback->Event(_id, eventName, pld);
    _adminLock.Unlock();
}

void AampEngineImplementation::SendEvent(const string& eventName, const string& parameters)
{
    SYSLOG(Trace::Error, (_T("sending event %s with %s"), eventName.c_str(), parameters.c_str()));
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

uint32_t AampEngineImplementation::InitializePlayerInstanceNoBlocking()
{
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
        DeinitializePlayerInstanceNoBlocking();
        return Core::ERROR_GENERAL;
    }

    _aampPlayer->RegisterEvents(_aampEventListener);
    _aampPlayer->SetReportInterval(1000 /* ms */);

    _aampGstPlayerMainLoop = g_main_loop_new(nullptr, false);

    // Run thread with _aampGstPlayerMainLoop
    Run();

    return Core::ERROR_NONE;
}

uint32_t AampEngineImplementation::AssertPlayerStateAndIdNoBlocking(uint32_t id)
{
    if(!_initialized)
    {
        SYSLOG(Trace::Error, (_T("Player is uninitialized, call Create method first!")));
        return Core::ERROR_ILLEGAL_STATE;
    }

    if(_id != id)
    {
        SYSLOG(Trace::Error, (_T("Instace ID is incorrect! Current: %d, requested %d"),
                _id, id));
        return Core::ERROR_UNAVAILABLE;
    }

    return Core::ERROR_NONE;
}

void AampEngineImplementation::DeinitializePlayerInstanceNoBlocking()
{
    if(_aampPlayer)
    {
        _aampPlayer->Stop();
        _aampPlayer->RegisterEvents(nullptr);
        delete _aampPlayer;
        _aampPlayer = nullptr;
    }

    if(_aampEventListener)
    {
        delete _aampEventListener;
        _aampEventListener = nullptr;
    }

    Block();
    if(_aampGstPlayerMainLoop)
    {
        g_main_loop_quit(_aampGstPlayerMainLoop);
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
    SYSLOG(Trace::Error, (_T("Event: handling event: %d"), event.type));
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
        SYSLOG(Trace::Error, (_T("Event: AAMP event is not supported: %d"), event.type));
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
