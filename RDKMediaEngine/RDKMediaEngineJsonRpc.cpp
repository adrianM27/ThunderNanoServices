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

        // Helper Logging defines
#define LOGINFOMETHOD() do { std::string json; parameters.ToString(json); \
        SYSLOG(Logging::Notification,(_T("%s: params=%s"), __FUNCTION__, json.c_str())); } while(0)

#define LOGTRACEMETHODFIN()  do { std::string json; \
        response.ToString(json); \
        SYSLOG(Logging::Notification,(_T("%s: response=%s"), __FUNCTION__, json.c_str())); } while(0)

        void RDKMediaEngine::RegisterAll()
        {
            Register(_T("create"), &RDKMediaEngine::create, this);
            Register(_T("release"), &RDKMediaEngine::release, this);
            Register(_T("load"), &RDKMediaEngine::load, this);
            Register(_T("play"), &RDKMediaEngine::play, this);
            Register(_T("pause"), &RDKMediaEngine::pause, this);
            Register(_T("seekTo"), &RDKMediaEngine::seekTo, this);
            Register(_T("stop"), &RDKMediaEngine::stop, this);
            Register(_T("initConfig"), &RDKMediaEngine::initConfig, this);
            Register(_T("initDRMConfig"), &RDKMediaEngine::initDRMConfig, this);
        }

        void RDKMediaEngine::UnregisterAll()
        {
            Unregister(_T("create"));
            Unregister(_T("release"));
            Unregister(_T("load"));
            Unregister(_T("play"));
            Unregister(_T("pause"));
            Unregister(_T("setPosition"));
            Unregister(_T("stop"));
            Unregister(_T("initConfig"));
            Unregister(_T("initDRMConfig"));
        }

        uint32_t RDKMediaEngine::create(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            const char *keyId = "id";
            if(!parameters.HasLabel(keyId)
                    || parameters[keyId].Content() != Core::JSON::Variant::type::STRING
                    || parameters[keyId].String().empty())
            {
                return Core::ERROR_BAD_REQUEST;
            }

            uint32_t result = Core::ERROR_NONE;
            string newId = parameters[keyId].String();
            if(newId == _id)
            {
                _idRefCnt++;
                response[_T("success")] = true;
            }
            else if( (result = _mediaEngine->Create(newId)) == Core::ERROR_NONE )
            {
                _id = newId;
                _idRefCnt = 1;
                response[_T("success")] = true;
            }
            else
            {
                _id.clear();
                _idRefCnt = 0;
                response[_T("success")] = false;
            }

            LOGTRACEMETHODFIN();
            return result;
        }

        uint32_t RDKMediaEngine::release(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            const char *keyId = "id";

            if(!parameters.HasLabel(keyId)
                    || parameters[keyId].Content() != Core::JSON::Variant::type::STRING
                    || parameters[keyId].String() != _id)
            {
                return Core::ERROR_BAD_REQUEST;
            }

            uint32_t result = Core::ERROR_NONE;
            if(_idRefCnt > 1)
            {
                _idRefCnt--;
                response[_T("success")] = true;
            }
            else if(_idRefCnt == 1
                    && (result = _mediaEngine->Destroy(_id)) == Core::ERROR_NONE)
            {
                _idRefCnt = 0;
                _id.clear();
                response[_T("success")] = true;
            }
            else
            {
                response[_T("success")] = false;
            }

            LOGTRACEMETHODFIN();
            return result;
        }

        uint32_t RDKMediaEngine::load(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            const char *keyId = "id";
            const char *keyUrl = "url";
            const char *keyAutoPlay = "autoplay";

            if(!parameters.HasLabel(keyId)
                    || parameters[keyId].Content() != Core::JSON::Variant::type::STRING
                    || parameters[keyId].String() != _id)
            {
                return Core::ERROR_BAD_REQUEST;
            }

            if(!parameters.HasLabel(keyUrl))
            {
                return Core::ERROR_BAD_REQUEST;
            }
            string url = parameters[keyUrl].Value();

            bool autoPlay = true;
            if(parameters.HasLabel(keyAutoPlay))
            {
                autoPlay = parameters[keyUrl].Boolean();
            }

            SYSLOG(Logging::Notification, ("autoPlay val: %d", autoPlay) );
            uint32_t retCode = _mediaEngine->Load(_id, url, autoPlay);
            if(retCode == Core::ERROR_NONE)
            {
                response[_T("success")] = true;
            }
            else
            {
                response[_T("success")] = false;
            }

            LOGTRACEMETHODFIN();
            return retCode;
        }

        uint32_t RDKMediaEngine::play(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            const char *keyId = "id";
            if(!parameters.HasLabel(keyId)
                    || parameters[keyId].Content() != Core::JSON::Variant::type::STRING
                    || parameters[keyId].String() != _id)
            {
                return Core::ERROR_BAD_REQUEST;
            }

            uint32_t retCode = _mediaEngine->Play(_id);
            if(retCode == Core::ERROR_NONE)
            {
                response[_T("success")] = true;
            }
            else
            {
                response[_T("success")] = false;
            }

            LOGTRACEMETHODFIN();
            return retCode;
        }

        uint32_t RDKMediaEngine::pause(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            const char *keyId = "id";
            if(!parameters.HasLabel(keyId)
                    || parameters[keyId].Content() != Core::JSON::Variant::type::STRING
                    || parameters[keyId].String() != _id)
            {
                return Core::ERROR_BAD_REQUEST;
            }

            uint32_t retCode = _mediaEngine->Pause(_id);
            if(retCode == Core::ERROR_NONE)
            {
                response[_T("success")] = true;
            }
            else
            {
                response[_T("success")] = false;
            }

            LOGTRACEMETHODFIN();
            return retCode;
        }

        uint32_t RDKMediaEngine::seekTo(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            const char *keyId = "id";
            if(!parameters.HasLabel(keyId)
                    || parameters[keyId].Content() != Core::JSON::Variant::type::STRING
                    || parameters[keyId].String() != _id)
            {
                return Core::ERROR_BAD_REQUEST;
            }

            const char *keyPositionSec = "positionSec";
            if(!parameters.HasLabel(keyPositionSec)
                    || parameters[keyPositionSec].Content() != Core::JSON::Variant::type::NUMBER)
            {
                return Core::ERROR_BAD_REQUEST;
            }

            float newPos = static_cast<float>(parameters[keyPositionSec].Number());
            SYSLOG(Logging::Notification,(_T("  newPos=%f"),newPos));
            uint32_t retCode = _mediaEngine->SeekTo(_id, parameters[keyPositionSec].Number());
            if(retCode == Core::ERROR_NONE)
            {
                response[_T("success")] = true;
            }
            else
            {
                response[_T("success")] = false;
            }

            LOGTRACEMETHODFIN();
            return retCode;
        }

        uint32_t RDKMediaEngine::stop(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            const char *keyId = "id";
            if(!parameters.HasLabel(keyId)
                    || parameters[keyId].Content() != Core::JSON::Variant::type::STRING
                    || parameters[keyId].String() != _id)
            {
                return Core::ERROR_BAD_REQUEST;
            }

            uint32_t retCode = _mediaEngine->Stop(_id);
            if(retCode == Core::ERROR_NONE)
            {
                response[_T("success")] = true;
            }
            else
            {
                response[_T("success")] = false;
            }

            LOGTRACEMETHODFIN();
            return retCode;
        }

        uint32_t RDKMediaEngine::initConfig(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            const char *keyId = "id";
            const char *keyConfig = "config";
            if(!parameters.HasLabel(keyId)
                    || parameters[keyId].Content() != Core::JSON::Variant::type::STRING
                    || parameters[keyId].String() != _id)
            {
                return Core::ERROR_BAD_REQUEST;
            }

            if(!parameters.HasLabel(keyConfig)
                    || parameters[keyId].Content() != Core::JSON::Variant::type::OBJECT)
            {
                return Core::ERROR_BAD_REQUEST;
            }

            string config = parameters[keyId].Value();

            uint32_t retCode = _mediaEngine->InitConfig(_id, config);
            if(retCode == Core::ERROR_NONE)
            {
                response[_T("success")] = true;
            }
            else
            {
                response[_T("success")] = false;
            }

            LOGTRACEMETHODFIN();
            return retCode;
        }

        uint32_t RDKMediaEngine::initDRMConfig(const JsonObject& parameters, JsonObject& response)
        {
            LOGINFOMETHOD();
            const char *keyId = "id";
            const char *keyConfig = "config";
            if(!parameters.HasLabel(keyId)
                    || parameters[keyId].Content() != Core::JSON::Variant::type::STRING
                    || parameters[keyId].String() != _id)
            {
                return Core::ERROR_BAD_REQUEST;
            }

            if(!parameters.HasLabel(keyConfig)
                    || parameters[keyId].Content() != Core::JSON::Variant::type::OBJECT)
            {
                return Core::ERROR_BAD_REQUEST;
            }

            string config = parameters[keyId].Value();

            uint32_t retCode = _mediaEngine->InitDRMConfig(_id, config);
            if(retCode == Core::ERROR_NONE)
            {
                response[_T("success")] = true;
            }
            else
            {
                response[_T("success")] = false;
            }

            LOGTRACEMETHODFIN();
            return retCode;
        }

        void RDKMediaEngine::onMediaEngineEvent(const string& id, const string &eventName, const string &parametersJson)
        {
            SYSLOG(Logging::Notification, (_T("OnMediaEngineEvent: id=%s, eventName=%s, parameters=%s"),
                    id.c_str(), eventName.c_str(), parametersJson.c_str()));
            JsonObject parametersJsonObjWithId;
            parametersJsonObjWithId[id.c_str()] = JsonObject(parametersJson);


            // Notify to all with:
            // params : { "<id>" : { <parametersJson> } }
            Notify(eventName.c_str(), parametersJsonObjWithId);

            // Notify to certain "id"s with:
            // params : { <parametersJson> }
            // TODO: Currently we cannot listen to this event from ThunderJS (?)
//            Notify(eventName.c_str(), parametersJsonObj, [&](const string& designator) -> bool {
//                const string designator_id = designator.substr(0, designator.find('.'));
//                return (idString == designator_id);
//            });
        }

    }//Plugin
}//WPEFramework
