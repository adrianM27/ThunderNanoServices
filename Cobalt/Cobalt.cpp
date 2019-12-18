#include "Cobalt.h"

namespace WPEFramework {
namespace Cobalt {

extern Exchange::IMemory* MemoryObserver(const uint32_t pid);
}

namespace Plugin {

SERVICE_REGISTRATION(Cobalt, 1, 0);

static Core::ProxyPoolType<Web::TextBody> _textBodies(2);
static Core::ProxyPoolType<Web::JSONBodyType<Cobalt::Data>> jsonBodyDataFactory(2);

/* encapsulated class Thread  */
/* virtual */const string Cobalt::Initialize(PluginHost::IShell *service) {
    Config config;
    string message;

    ASSERT(_service == nullptr);
    ASSERT(_cobalt == nullptr);
    ASSERT(_memory == nullptr);

    config.FromString(service->ConfigLine());

    _connectionId = 0;
    _service = service;
    _skipURL = _service->WebPrefix().length();

    // Register the Connection::Notification stuff. The Remote process might die
    // before we get a
    // change to "register" the sink for these events !!! So do it ahead of
    // instantiation.
    _service->Register(&_notification);
    _cobalt = _service->Root < Exchange::IBrowser
            > (_connectionId, 2000, _T("CobaltImplementation"));

    if (_cobalt != nullptr) {

        PluginHost::IStateControl *stateControl(
                _cobalt->QueryInterface<PluginHost::IStateControl>());
        if (stateControl == nullptr) {
            _cobalt->Release();
            _cobalt = nullptr;
        } else {

            _memory = WPEFramework::Cobalt::MemoryObserver(_connectionId);
            ASSERT(_memory != nullptr);

            _cobalt->Register(&_notification);
            stateControl->Register(&_notification);
            stateControl->Configure(_service);
            stateControl->Release();
        }
    }

    if (_cobalt == nullptr) {
        message = _T("Cobalt could not be instantiated.");
        _service->Unregister(&_notification);
        _service = nullptr;
    }

    return message;
}

/* virtual */void Cobalt::Deinitialize(PluginHost::IShell *service) {
    ASSERT(_service == service);
    ASSERT(_cobalt != nullptr);
    ASSERT(_memory != nullptr);

    PluginHost::IStateControl *stateControl(
            _cobalt->QueryInterface<PluginHost::IStateControl>());

    // Make sure the Activated and Deactivated are no longer called before we
    // start cleaning up..
    _service->Unregister(&_notification);
    _cobalt->Unregister(&_notification);
    _memory->Release();

    // In case Cobalt crashed, there is no access to the statecontrol interface,
    // check it !!
    if (stateControl != nullptr) {
        stateControl->Unregister(&_notification);
        stateControl->Release();
    } else {
        // On behalf of the crashed process, we will release the notification sink.
        _notification.Release();
    }

    if (_cobalt->Release() != Core::ERROR_DESTRUCTION_SUCCEEDED) {

        ASSERT(_connectionId != 0);
        TRACE_L1("Cobalt Plugin is not properly destructed. %d", _connectionId);

        RPC::IRemoteConnection *connection(
                _service->RemoteConnection(_connectionId));

        // The process can disappear in the meantime...
        if (connection != nullptr) {
            connection->Terminate();
            connection->Release();
        }
    }

    // Deinitialize what we initialized..
    _memory = nullptr;
    _cobalt = nullptr;
    _service = nullptr;
}

/* virtual */string Cobalt::Information() const {
    // No additional info to report.
    return (string());
}

/* virtual */void Cobalt::Inbound(Web::Request &request) {
    if (request.Verb == Web::Request::HTTP_POST) {
        // This might be a "launch" application thingy, make sure we receive the
        // proper info.
        request.Body(jsonBodyDataFactory.Element());
        //request.Body(_textBodies.Element());
    }
}

/* virtual */Core::ProxyType<Web::Response> Cobalt::Process(
        const Web::Request &request) {
    ASSERT(_skipURL <= request.Path.length());
    TRACE(Trace::Information, (string(_T("Received cobalt request"))));

    Core::ProxyType < Web::Response
            > result(PluginHost::Factories::Instance().Response());

    Core::TextSegmentIterator index(
            Core::TextFragment(request.Path, _skipURL,
                    request.Path.length() - _skipURL), false, '/');

    result->ErrorCode = Web::STATUS_BAD_REQUEST;
    result->Message = "Unknown error";

    if (request.Verb == Web::Request::HTTP_POST) {
        // We might be receiving a plugin download request.
        if ((index.Next() == true) && (index.Next() == true)
                && (_cobalt != nullptr)) {
            PluginHost::IStateControl *stateControl(
                    _cobalt->QueryInterface<PluginHost::IStateControl>());
            if (stateControl != nullptr) {
                if (index.Remainder() == _T("Suspend")) {
                    stateControl->Request(PluginHost::IStateControl::SUSPEND);
                } else if (index.Remainder() == _T("Resume")) {
                    stateControl->Request(PluginHost::IStateControl::RESUME);
                } else if ((index.Remainder() == _T("URL"))
                        && (request.HasBody() == true)
                        && (request.Body<const Data>()->URL.Value().empty()
                                == false)) {
                    _cobalt->SetURL(request.Body<const Data>()->URL.Value());
                }
                stateControl->Release();
            }
        }
    } else if (request.Verb == Web::Request::HTTP_GET) {
    }
    return result;
}

void Cobalt::LoadFinished(const string &URL) {
    string message(
            string("{ \"url\": \"") + URL + string("\", \"loaded\":true }"));
    TRACE(Trace::Information, (_T("LoadFinished: %s"), message.c_str()));
    _service->Notify(message);

    event_urlchange(URL, true);
}

void Cobalt::URLChanged(const string &URL) {
    string message(string("{ \"url\": \"") + URL + string("\" }"));
    TRACE(Trace::Information, (_T("URLChanged: %s"), message.c_str()));
    _service->Notify(message);

    event_urlchange(URL, false);
}

void Cobalt::Hidden(const bool hidden) {
    TRACE(Trace::Information,
            (_T("Hidden: %s }"), (hidden ? "true" : "false")));
    string message(
            string("{ \"hidden\": ") + (hidden ? _T("true") : _T("false"))
                    + string("}"));
    _hidden = hidden;
    _service->Notify(message);

    event_visibilitychange(hidden);
}

void Cobalt::StateChange(const PluginHost::IStateControl::state state) {
    switch (state) {
    case PluginHost::IStateControl::RESUMED:
        TRACE(Trace::Information,
                (string(_T("StateChange: { \"suspend\":false }"))));
        _service->Notify("{ \"suspended\":false }");
        event_statechange(false);
        break;
    case PluginHost::IStateControl::SUSPENDED:
        TRACE(Trace::Information,
                (string(_T("StateChange: { \"suspend\":true }"))));
        _service->Notify("{ \"suspended\":true }");
        event_statechange(true);
        break;
    case PluginHost::IStateControl::EXITED:
        // Exited by Cobalt app
        PluginHost::WorkerPool::Instance().Submit(
                PluginHost::IShell::Job::Create(_service,
                        PluginHost::IShell::DEACTIVATED,
                        PluginHost::IShell::REQUESTED));
        break;
    case PluginHost::IStateControl::UNINITIALIZED:
        break;
    default:
        ASSERT(false);
        break;
    }
}

void Cobalt::Deactivated(RPC::IRemoteConnection *connection) {
    if (connection->Id() == _connectionId) {

        ASSERT(_service != nullptr);
        PluginHost::WorkerPool::Instance().Submit(
                PluginHost::IShell::Job::Create(_service,
                        PluginHost::IShell::DEACTIVATED,
                        PluginHost::IShell::FAILURE));
    }
}
}
} // namespace