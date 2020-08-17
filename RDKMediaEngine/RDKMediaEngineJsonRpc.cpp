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

#include "RDKMediaEngine.h"
#include "Module.h"
#include <string>

namespace WPEFramework {

namespace Plugin {

void RDKMediaEngine::RegisterAll()
{
    Register(_T("create"), &RDKMediaEngine::create, this);
    Register(_T("load"), &RDKMediaEngine::load, this);
    Register(_T("play"), &RDKMediaEngine::play, this);
    Register(_T("pause"), &RDKMediaEngine::pause, this);
    Register(_T("setPosition"), &RDKMediaEngine::setPosition, this);
    Register(_T("stop"), &RDKMediaEngine::stop, this);
}

void RDKMediaEngine::UnregisterAll()
{
    Unregister(_T("create"));
    Unregister(_T("load"));
    Unregister(_T("play"));
    Unregister(_T("pause"));
    Unregister(_T("setPosition"));
    Unregister(_T("stop"));
}

uint32_t RDKMediaEngine::create(const JsonObject& parameters, JsonObject& response)
{
    SYSLOG(Trace::Error, (_T("create")));
    const char *keyType = "type";
    if(!parameters.HasLabel(keyType)
            || parameters[keyType].Content() != Core::JSON::Variant::type::STRING
            || parameters[keyType].String() != "ip")
    {
        return Core::ERROR_BAD_REQUEST;
    }

    uint32_t newId = _id + 1;
    uint32_t result = _mediaEngine->Create(newId);
    if(result == Core::ERROR_NONE)
    {
        _id = newId;
        response[_T("id")] = _id;
    }

    return result;
}

uint32_t RDKMediaEngine::load(const JsonObject& parameters, JsonObject& response)
{
    SYSLOG(Trace::Error, (_T("load")));
    const char *keyId = "id";
    const char *keyUrl = "url";

    if(!parameters.HasLabel(keyId)
            || parameters[keyId].Content() != Core::JSON::Variant::type::NUMBER
            || parameters[keyId].Number() != _id)
    {
        return Core::ERROR_BAD_REQUEST;
    }

    if(!parameters.HasLabel(keyUrl))
    {
        return Core::ERROR_BAD_REQUEST;
    }

    string uri = parameters[keyUrl].Value();
    SYSLOG(Trace::Error, (_T("loading with id %d uri = %s"),_id, uri.c_str()));
    return _mediaEngine->Load(_id, uri);
}

uint32_t RDKMediaEngine::play(const JsonObject& parameters, JsonObject& response)
{
    SYSLOG(Trace::Error, (_T("play")));
    const char *keyId = "id";
    if(!parameters.HasLabel(keyId)
            || parameters[keyId].Content() != Core::JSON::Variant::type::NUMBER
            || parameters[keyId].Number() != _id)
    {
        return Core::ERROR_BAD_REQUEST;
    }
    return _mediaEngine->Play(_id);
}

uint32_t RDKMediaEngine::pause(const JsonObject& parameters, JsonObject& response)
{
    SYSLOG(Trace::Error, (_T("pause")));
    const char *keyId = "id";
    if(!parameters.HasLabel(keyId)
            || parameters[keyId].Content() != Core::JSON::Variant::type::NUMBER
            || parameters[keyId].Number() != _id)
    {
        return Core::ERROR_BAD_REQUEST;
    }
    return _mediaEngine->Pause(_id);
}

uint32_t RDKMediaEngine::setPosition(const JsonObject& parameters, JsonObject& response)
{
    SYSLOG(Trace::Error, (_T("setPosition")));
    const char *keyId = "id";
    if(!parameters.HasLabel(keyId)
            || parameters[keyId].Content() != Core::JSON::Variant::type::NUMBER
            || parameters[keyId].Number() != _id)
    {
        return Core::ERROR_BAD_REQUEST;
    }

    const char *keyPositionSec = "positionSec";
    if(!parameters.HasLabel(keyPositionSec)
            || parameters[keyPositionSec].Content() != Core::JSON::Variant::type::NUMBER)
    {
        return Core::ERROR_BAD_REQUEST;
    }

    return _mediaEngine->SetPosition(_id,parameters[keyPositionSec].Number());
}

uint32_t RDKMediaEngine::stop(const JsonObject& parameters, JsonObject& response)
{
    SYSLOG(Trace::Error, (_T("create")));
    const char *keyId = "id";
    if(!parameters.HasLabel(keyId)
            || parameters[keyId].Content() != Core::JSON::Variant::type::NUMBER
            || parameters[keyId].Number() != _id)
    {
        return Core::ERROR_BAD_REQUEST;
    }
    return _mediaEngine->Stop(_id);
}

void RDKMediaEngine::onMediaEngineEvent(const uint32_t id, const string &eventName, const string &parameters)
{
    SYSLOG(Trace::Error, (_T("OnMediaEngineEvent %d %s %s"), id, eventName.c_str(), parameters.c_str()));
    string idString = std::to_string(id);
    JsonObject parametersJson(parameters);

    Notify(eventName, parametersJson, [&](const string& designator) -> bool {
            const string designator_id = designator.substr(0, designator.find('.'));
            return (idString == designator_id);
        });

}

}//Plugin

}//WPEFramework
