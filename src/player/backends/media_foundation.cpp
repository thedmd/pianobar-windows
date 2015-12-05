/*
Copyright (c) 2015
Micha³ Cichoñ <thedmd@interia.pl>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/* receive/play audio stream */

/* based on DShow example player */

extern "C" {
# include "config.h"
# include "../player2_private.h"
}
#undef restrict
#undef inline
# include "media_foundation.h"
# include <mferror.h>
# include <cassert>
# include <cmath>

# pragma comment(lib, "mf.lib")
# pragma comment(lib, "mfplat.lib")
# pragma comment(lib, "mfuuid.lib")

# define SAFE_CALL(expression) do { hr = expression; if (FAILED(hr)) return hr; } while (false)

// Placement new is missing if we do not include <memory>
//static inline void* operator new(size_t size, void* ptr)
//{
//    return ptr;
//}

template <class Q>
static HRESULT GetEventObject(IMFMediaEvent *mediaEvent, Q** outObject)
{
    *outObject = nullptr;

    PROPVARIANT var;
    HRESULT hr = mediaEvent->GetValue(&var);
    if (SUCCEEDED(hr))
    {
        if (var.vt == VT_UNKNOWN)
            hr = var.punkVal->QueryInterface(outObject);
        else
            hr = MF_E_INVALIDTYPE;

        PropVariantClear(&var);
    }
    return hr;
}

//static HRESULT CreateMediaSource(PCWSTR pszURL, IMFMediaSource **ppSource);
static HRESULT CreatePlaybackTopology(IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, IMFTopology **ppTopology);
static float   MFTIMEToSeconds(MFTIME time)
{
    float seconds = (float)(time / 1e7);
    return seconds;
}

static void DefaultMediaPlayerEventCallback(MediaPlayer* player, com_ptr<IMFMediaEvent> mediaEvent, MediaEventType eventType)
{
    player->HandleEvent(mediaEvent);
}

HRESULT MediaPlayer::Create(MediaPlayerEventCallback eventCallback, MediaPlayer** outMediaPlayer)
{
    if (!outMediaPlayer)
        return E_POINTER;

    if (!eventCallback)
        eventCallback = DefaultMediaPlayerEventCallback;

    // auto mediaPlayer = new (std::nothrow) MediaPlayer(eventCallback);

    // Cheep object construction without throwing exceptions around.
    auto mediaPlayer = reinterpret_cast<MediaPlayer*>(malloc(sizeof(MediaPlayer)));
    if (!mediaPlayer)
        return E_OUTOFMEMORY;

    new (mediaPlayer) MediaPlayer(eventCallback);

    auto hr = mediaPlayer->Initialize();
    if (SUCCEEDED(hr))
        *outMediaPlayer = mediaPlayer;
    else
        mediaPlayer->Release();

    return hr;
}

MediaPlayer::MediaPlayer(MediaPlayerEventCallback eventCallback):
    m_RefCount(1),
    m_SourceResolver(nullptr),
    m_CancelCookie(nullptr),
    m_MediaSession(nullptr),
    m_MediaSource(nullptr),
    m_SimpleAudioVolume(nullptr),
    m_PresentationClock(nullptr),
    m_StreamAudioVolume(nullptr),
    m_SetMasterVolume(),
    m_MasterVolume(1.0f),
    m_ReplayGain(0.0f),
    m_EventCallback(eventCallback),
    m_UserData(nullptr),
    m_State(Closed),
    m_CloseEvent(nullptr)
{
}

MediaPlayer::~MediaPlayer()
{
    // If FALSE, the app did not call Shutdown().
    assert(m_MediaSession == nullptr);

    // When MediaPlayer calls IMediaEventGenerator::BeginGetEvent on the
    // media session, it causes the media session to hold a reference 
    // count on the MediaPlayer.

    // This creates a circular reference count between MediaPlayer and the
    // media session. Calling Shutdown breaks the circular reference
    // count.

    // If CreateInstance fails, the application will not call 
    // Shutdown. To handle that case, call Shutdown in the destructor.
    Shutdown();
}


ULONG STDMETHODCALLTYPE MediaPlayer::AddRef()
{
    return InterlockedIncrement(&m_RefCount);
}

ULONG STDMETHODCALLTYPE MediaPlayer::Release()
{
    auto count = InterlockedDecrement(&m_RefCount);
    if (count == 0)
    {
        // Do manually deleter job, since malloc was used to allocate memory
        this->~MediaPlayer();
        free(this);

        // delete this;
    }
    return count;
}

HRESULT STDMETHODCALLTYPE MediaPlayer::QueryInterface(REFIID iid, void** object)
{
    if (!object)
        return E_POINTER;

    *object = nullptr;
    if (iid == IID_IUnknown)
        *object = static_cast<IUnknown*>(this);
    else if (iid == IID_IMFAsyncCallback)
        *object = static_cast<IMFAsyncCallback*>(this);
    else
        return E_NOINTERFACE;

    AddRef();

    return S_OK;
}

HRESULT MediaPlayer::Initialize()
{
    auto hr = MFStartup(MF_VERSION);
    if (FAILED(hr))
        return hr;

    m_CloseEvent = CreateEvent(nullptr, false, false, nullptr);
    if (!m_CloseEvent)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        return hr;
    }

    return hr;
}

HRESULT MediaPlayer::Shutdown()
{
    auto hr = CloseSession();

    MFShutdown();

    if (m_CloseEvent)
    {
        CloseHandle(m_CloseEvent);
        m_CloseEvent = nullptr;
    }

    return hr;
}

HRESULT MediaPlayer::CreateSession()
{
    HRESULT hr = S_OK;

    SAFE_CALL(CloseSession());
    assert(m_State == Closed);
    SAFE_CALL(MFCreateMediaSession(nullptr, &m_MediaSession));
    SAFE_CALL(m_MediaSession->BeginGetEvent(this, nullptr));

    return hr;
}

HRESULT MediaPlayer::CloseSession()
{
    //  The IMFMediaSession::Close method is asynchronous, but the 
    //  CPlayer::CloseSession method waits on the MESessionClosed mediaEvent.
    //  
    //  MESessionClosed is guaranteed to be the last mediaEvent that the 
    //  media session fires.

    HRESULT hr = S_OK;

    if (m_SourceResolver)
    {
        if (m_CancelCookie)
        {
            SAFE_CALL(m_SourceResolver->CancelObjectCreation(m_CancelCookie));
            m_CancelCookie = nullptr;
        }

        m_SourceResolver = nullptr;
    }

    if (m_MediaSession)
    {
        SAFE_CALL(SetState(Closing));

        hr = m_MediaSession->Close();
        if (SUCCEEDED(hr))
        {
            auto waitResult = WaitForSingleObject(m_CloseEvent, 5000);
            assert(waitResult != WAIT_TIMEOUT);
        }
    }

    if (SUCCEEDED(hr))
    {
        // Shut down the media source. (Synchronous operation, no events.)
        if (m_MediaSource)
            m_MediaSource->Shutdown();

        // Shut down the media session. (Synchronous operation, no events.)
        if (m_MediaSession)
            m_MediaSession->Shutdown();
    }

    m_MediaSource = nullptr;
    m_MediaSession = nullptr;

    SAFE_CALL(SetState(Closed));

    return hr;
}

HRESULT MediaPlayer::OpenURL(const wchar_t* url)
{
    // 1. Create a new media session.
    // 2. Create the media source.
    // 3. Create the topology.
    // 4. Queue the topology [asynchronous]
    // 5. Start playback [asynchronous - does not happen in this method.]

    auto doOpenAsync = [this](const wchar_t* url)
    {
        HRESULT hr = S_OK;

        SAFE_CALL(CreateSession());

        SAFE_CALL(MFCreateSourceResolver(&m_SourceResolver));

        SAFE_CALL(m_SourceResolver->BeginCreateObjectFromURL(
            url,                        // URL of the source.
            MF_RESOLUTION_MEDIASOURCE,  // Create a source object.
            NULL,                       // Optional property store.
            &m_CancelCookie,            // Receives cookie for cancelation.
            this,                       // Callback for recieving events.
            nullptr));                  // User defined state.

        return hr;
    };

    auto hr = doOpenAsync(url);
    if (SUCCEEDED(hr))
        SAFE_CALL(SetState(OpenPending));
    else
        SAFE_CALL(SetState(Closed));

    return hr;
}

HRESULT MediaPlayer::ApplyTopology()
{
    HRESULT hr = S_OK;

    com_ptr<IMFTopology>               topology;
    com_ptr<IMFPresentationDescriptor> presentationDescriptor;

    SAFE_CALL(m_MediaSource->CreatePresentationDescriptor(&presentationDescriptor));
    SAFE_CALL(CreatePlaybackTopology(m_MediaSource, presentationDescriptor, &topology));
    SAFE_CALL(m_MediaSession->SetTopology(0, topology));

    return hr;
}

HRESULT MediaPlayer::StartPlayback()
{
    assert(m_MediaSession);

    PROPVARIANT start;
    PropVariantInit(&start);

    if (m_State == Stopped)
    {
        // After stop, always play from beginning
        start.vt             = VT_I8;
        start.uhVal.QuadPart = 0;
    }

    auto hr = m_MediaSession->Start(nullptr, &start);
    if (SUCCEEDED(hr))
    {
        // Note: Start is an asynchronous operation. However, we
        // can treat our state as being already started. If Start
        // fails later, we'll get an MESessionStarted mediaEvent with
        // an error code, and we will update our state then.
        SAFE_CALL(SetState(Started));
    }
    PropVariantClear(&start);

    return hr;
}

HRESULT MediaPlayer::Play()
{
    if (m_State != Paused && m_State != Stopped)
        return MF_E_INVALIDREQUEST;

    if (!m_MediaSession || !m_MediaSource)
        return E_UNEXPECTED;

    return StartPlayback();
}

HRESULT MediaPlayer::Pause()
{
    if (m_State != Started)
        return MF_E_INVALIDREQUEST;

    if (!m_MediaSession || !m_MediaSource)
        return E_UNEXPECTED;

    auto hr = m_MediaSession->Pause();
    if (SUCCEEDED(hr))
        SAFE_CALL(SetState(Paused));

    return hr;
}

HRESULT MediaPlayer::Stop()
{
    if (m_State != Started && m_State != Paused)
        return MF_E_INVALIDREQUEST;

    if (!m_MediaSession)
        return E_UNEXPECTED;

    auto hr = m_MediaSession->Stop();
    if (SUCCEEDED(hr))
        SAFE_CALL(SetState(Stopped));

    return hr;
}

HRESULT MediaPlayer::SetState(State state)
{
    if (m_State == state)
        return S_OK;

    auto previousState = m_State;

    m_State = state;

    HRESULT hr = S_OK;
    SAFE_CALL(OnStateChange(m_State, previousState));
    return hr;
}

MediaPlayer::State MediaPlayer::GetState() const
{
    return m_State;
}

void MediaPlayer::SetMasterVolume(float volume)
{
    m_MasterVolume = min(1.0f, max(0.0f, volume));
    if (FAILED(ApplyMasterVolume(volume)))
        m_SetMasterVolume = volume;
}

HRESULT MediaPlayer::ApplyMasterVolume(float volume)
{
    if (m_SimpleAudioVolume)
        return m_SimpleAudioVolume->SetMasterVolume(volume);
    else
        return E_FAIL;
}

float MediaPlayer::GetMasterVolume() const
{
    if (m_SimpleAudioVolume)
    {
        float masterVolume = 1.0f;
        if (SUCCEEDED(m_SimpleAudioVolume->GetMasterVolume(&masterVolume)))
            m_MasterVolume = masterVolume;
    }

    return m_MasterVolume;
}


void  MediaPlayer::SetReplayGain(float replayGain)
{
    m_ReplayGain = replayGain;
    ApplyReplayGain(replayGain);
}

HRESULT MediaPlayer::ApplyReplayGain(float replayGain)
{
    if (m_StreamAudioVolume)
    {
        HRESULT hr = S_OK;

        // Attenuation (dB) = 20 * log10(Level)
        const float attenuation = powf(10.0f, replayGain / 20.0f);
        const float volume      = max(0.0f, min(1.0f, attenuation));

        // This is poor-man method since positive gain is always clipped.
        // To fully implement replay gain use custom MFT to process
        // audio data.
        UINT32 channelCount = 0;
        SAFE_CALL(m_StreamAudioVolume->GetChannelCount(&channelCount));

        for (UINT32 i = 0; i < channelCount; ++i)
            hr = m_StreamAudioVolume->SetChannelVolume(i, volume);

        return hr;
    }   
    else
        return E_FAIL;
}

float MediaPlayer::GetReplayGain() const
{
    return m_ReplayGain;
}

optional<float> MediaPlayer::GetPresentationTime() const
{
    if (m_PresentationClock)
    {
        MFTIME presentationTime = 0;
        if (SUCCEEDED(m_PresentationClock->GetTime(&presentationTime)))
            return MFTIMEToSeconds(presentationTime);
    }

    return nullopt;
}

optional<float> MediaPlayer::GetDuration() const
{
    if (m_MediaSource)
    {
        com_ptr<IMFPresentationDescriptor> presentationDescriptor;

        if (SUCCEEDED(m_MediaSource->CreatePresentationDescriptor(&presentationDescriptor)))
        {
            MFTIME durationTime = 0;
            if (SUCCEEDED(presentationDescriptor->GetUINT64(MF_PD_DURATION, reinterpret_cast<UINT64*>(&durationTime))))
                return MFTIMEToSeconds(durationTime);
        }
    }

    return nullopt;
}

void MediaPlayer::SetUserData(void* userData)
{
    m_UserData = userData;
}

void* MediaPlayer::GetUserData() const
{
    return m_UserData;
}


HRESULT MediaPlayer::HandleEvent(IMFMediaEvent* mediaEvent)
{
    if (!mediaEvent)
        return E_POINTER;

    HRESULT hr = S_OK;
    HRESULT hrStatus = S_OK;

    MediaEventType eventType = MEUnknown;
    SAFE_CALL(mediaEvent->GetType(&eventType));
    SAFE_CALL(mediaEvent->GetStatus(&hrStatus));
    SAFE_CALL(hrStatus);

    switch (eventType)
    {
        case MESessionTopologyStatus: SAFE_CALL(OnTopologyStatus(mediaEvent));           break;
        case MEEndOfPresentation:     SAFE_CALL(OnPresentationEnded(mediaEvent));        break;
        case MENewPresentation:       SAFE_CALL(OnNewPresentation(mediaEvent));          break;
        default:                                                                         break;
    }

    SAFE_CALL(OnSessionEvent(mediaEvent, eventType));

    return hr;
}

HRESULT MediaPlayer::OnTopologyStatus(IMFMediaEvent* mediaEvent)
{
    UINT32 status = 0;
    HRESULT hr = S_OK;

    SAFE_CALL(mediaEvent->GetUINT32(MF_EVENT_TOPOLOGY_STATUS, &status));
    if (status == MF_TOPOSTATUS_READY)
        SAFE_CALL(StartPlayback());

    return hr;
}

HRESULT MediaPlayer::OnPresentationEnded(IMFMediaEvent* mediaEvent)
{
    HRESULT hr = S_OK;
    SAFE_CALL(SetState(Stopped));
    return hr;
}

HRESULT MediaPlayer::OnNewPresentation(IMFMediaEvent* mediaEvent)
{
    HRESULT hr = S_OK;

    com_ptr<IMFTopology>               topology;
    com_ptr<IMFPresentationDescriptor> presentationDescriptor;

    SAFE_CALL(GetEventObject(mediaEvent, &presentationDescriptor));
    SAFE_CALL(CreatePlaybackTopology(m_MediaSource, presentationDescriptor, &topology));
    SAFE_CALL(m_MediaSession->SetTopology(0, topology));

    SAFE_CALL(SetState(OpenPending));

    return S_OK;
}

HRESULT MediaPlayer::OnSessionEvent(IMFMediaEvent*, MediaEventType)
{
    return S_OK;
}

HRESULT MediaPlayer::OnStateChange(State state, State previousState)
{
    HRESULT hr = S_OK;

    if (state == Started && previousState != Paused)
    {
        m_SimpleAudioVolume = nullptr;
        m_PresentationClock = nullptr;

        com_ptr<IMFClock> clock;
        SAFE_CALL(m_MediaSession->GetClock(&clock));
        SAFE_CALL(clock->QueryInterface(&m_PresentationClock));

        SAFE_CALL(MFGetService(m_MediaSession, MR_POLICY_VOLUME_SERVICE, IID_IMFSimpleAudioVolume, (void**)&m_SimpleAudioVolume));

        // Set master volume to previously set value
        if (m_SetMasterVolume)
        {
            SAFE_CALL(ApplyMasterVolume(*m_SetMasterVolume));
            m_SetMasterVolume = nullopt;
        }

        MFGetService(m_MediaSession, MR_STREAM_VOLUME_SERVICE, IID_IMFAudioStreamVolume, (void**)&m_StreamAudioVolume);
        ApplyReplayGain(m_ReplayGain);
    }

    if (state == Stopped)
    {
        m_StreamAudioVolume = nullptr;
        m_SimpleAudioVolume = nullptr;
        m_PresentationClock = nullptr;
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE MediaPlayer::GetParameters(DWORD* flags, DWORD* queue)
{
    return E_NOTIMPL; // optional, not implemented
}

HRESULT STDMETHODCALLTYPE MediaPlayer::Invoke(IMFAsyncResult* asyncResult)
{
    HRESULT hr = S_OK;

    if (m_SourceResolver && m_State == OpenPending)
    {
        auto doFinishOpen = [this](IMFAsyncResult* asyncResult)
        {
            HRESULT hr = S_OK;

            // We are in async OpenURL
            MF_OBJECT_TYPE objectType = MF_OBJECT_INVALID;
            com_ptr<IUnknown> source;

            SAFE_CALL(m_SourceResolver->EndCreateObjectFromURL(asyncResult, &objectType, &source));
            SAFE_CALL(source->QueryInterface(&m_MediaSource));

            return hr;
        };

        hr = doFinishOpen(asyncResult);

        m_CancelCookie   = nullptr;
        m_SourceResolver = nullptr;

        if (SUCCEEDED(hr))
            hr = ApplyTopology();

        if (FAILED(hr))
            CloseSession();

        return hr;
    }

    MediaEventType         eventType = MEUnknown;
    com_ptr<IMFMediaEvent> mediaEvent;
    SAFE_CALL(m_MediaSession->EndGetEvent(asyncResult, &mediaEvent));
    SAFE_CALL(mediaEvent->GetType(&eventType));

    if (eventType == MESessionClosed)
        // The session was closed. 
        // The application is waiting on the m_CloseEvent mediaEvent handle. 
        SetEvent(m_CloseEvent);
    else
        // For all other events, get the next mediaEvent in the queue.
        SAFE_CALL(m_MediaSession->BeginGetEvent(this, nullptr));

    // Check the application state. 

    // If a call to IMFMediaSession::Close is pending, it means the 
    // application is waiting on the m_CloseEvent mediaEvent and
    // the application's message loop is blocked. 

    // Otherwise, post a private window message to the application. 
    if (m_State != Closing)
    {
        // Leave a reference count on the mediaEvent.
        m_EventCallback(this, std::move(mediaEvent), eventType);
    }

    return S_OK;
}

//  Create a media source from a URL.
//static HRESULT CreateMediaSource(const wchar_t* url, IMFMediaSource** mediaSource)
//{
//    HRESULT hr = S_OK;
//
//    MF_OBJECT_TYPE objectType = MF_OBJECT_INVALID;
//
//    com_ptr<IMFSourceResolver> sourceResolver;
//    com_ptr<IUnknown>          source;
//
//    // Create the source resolver.
//    SAFE_CALL(MFCreateSourceResolver(&sourceResolver));
//
//    // Use the source resolver to create the media source.
//
//    // Note: For simplicity this sample uses the synchronous method to create 
//    // the media source. However, creating a media source can take a noticeable
//    // amount of time, especially for a network source. For a more responsive 
//    // UI, use the asynchronous BeginCreateObjectFromURL method.
//    SAFE_CALL(sourceResolver->CreateObjectFromURL(
//        url,                        // URL of the source.
//        MF_RESOLUTION_MEDIASOURCE,  // Create a source object.
//        NULL,                       // Optional property store.
//        &objectType,                // Receives the created object type. 
//        &source));                  // Receives a pointer to the media source.
//
//    // Get the IMFMediaSource interface from the media source.
//    SAFE_CALL(source->QueryInterface(IID_PPV_ARGS(mediaSource)));
//
//    return hr;
//}

static HRESULT CreateMediaSinkActivate(IMFStreamDescriptor* streamDescriptor, IMFActivate** outActivate)
{
    HRESULT hr = S_OK;

    com_ptr<IMFMediaTypeHandler> mediaTypeHandler;
    com_ptr<IMFActivate> activate;

    // Get the media type handler for the stream.
    SAFE_CALL(streamDescriptor->GetMediaTypeHandler(&mediaTypeHandler));

    // Get the major media type.
    GUID majorType = GUID_NULL;
    SAFE_CALL(mediaTypeHandler->GetMajorType(&majorType));

    // Create an IMFActivate object for the renderer, based on the media type.
    if (MFMediaType_Audio == majorType)
    {
        // Create the audio renderer.
        SAFE_CALL(MFCreateAudioRendererActivate(&activate));
    }
    else // Unknown stream type. 
    {
        SAFE_CALL(E_FAIL);
        // Optionally, you could deselect this stream instead of failing.
    }

    // Return IMFActivate pointer to caller.
    *outActivate = activate.detach();

    return hr;
}

// Add a source node to a topology.
static HRESULT AddSourceNode(IMFTopology* topology, IMFMediaSource* mediaSource, IMFPresentationDescriptor* presentationDescriptor, IMFStreamDescriptor* streamDescriptor, IMFTopologyNode* *outTopologyNode)
{
    HRESULT hr = S_OK;

    com_ptr<IMFTopologyNode> node;

    // Create the node.
    SAFE_CALL(MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &node));
    SAFE_CALL(node->SetUnknown(MF_TOPONODE_SOURCE, mediaSource));
    SAFE_CALL(node->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, presentationDescriptor));
    SAFE_CALL(node->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, streamDescriptor));
    SAFE_CALL(topology->AddNode(node));

    *outTopologyNode = node.detach();

    return hr;
}

// Add an output node to a topology.
static HRESULT AddOutputNode(IMFTopology* topology, IMFActivate* activate, DWORD streamSinkId, IMFTopologyNode** outTopologyNode)
{
    HRESULT hr = S_OK;

    com_ptr<IMFTopologyNode> node;

    SAFE_CALL(MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &node));
    SAFE_CALL(node->SetObject(activate));
    SAFE_CALL(node->SetUINT32(MF_TOPONODE_STREAMID, streamSinkId));
    SAFE_CALL(node->SetUINT32(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, false));
    SAFE_CALL(topology->AddNode(node));

    *outTopologyNode = node.detach();

    return hr;
}

// Add a topology branch for one stream.
//
// For each stream, this function does the following:
//
//   1. Creates a source node associated with the stream. 
//   2. Creates an output node for the renderer. 
//   3. Connects the two nodes.
//
// The media session will add any decoders that are needed.
static HRESULT AddBranchToPartialTopology(IMFTopology* topology, IMFMediaSource* mediaSource, IMFPresentationDescriptor* presentationDescriptor, DWORD streamIndex)
{
    HRESULT hr = S_OK;

    com_ptr<IMFStreamDescriptor> streamDescriptor;
    com_ptr<IMFActivate>         activate;
    com_ptr<IMFTopologyNode>     sourceNode;
    com_ptr<IMFTopologyNode>     outputNode;

    BOOL isSelected = false;
    SAFE_CALL(presentationDescriptor->GetStreamDescriptorByIndex(streamIndex, &isSelected, &streamDescriptor));
    if (!isSelected)
        return hr;

    // Create the media sink activation object.
    SAFE_CALL(CreateMediaSinkActivate(streamDescriptor, &activate));
    SAFE_CALL(AddSourceNode(topology, mediaSource, presentationDescriptor, streamDescriptor, &sourceNode));
    SAFE_CALL(AddOutputNode(topology, activate, 0, &outputNode));
    SAFE_CALL(sourceNode->ConnectOutput(0, outputNode, 0));

    return hr;
}

// Create a playback topology from a media source.
static HRESULT CreatePlaybackTopology(IMFMediaSource* mediaSourcee, IMFPresentationDescriptor* presentationDescriptor, IMFTopology** outTopology)
{
    HRESULT hr = S_OK;

    com_ptr<IMFTopology> topology;

    // Create a new topology.
    SAFE_CALL(MFCreateTopology(&topology));

    // Get the number of streams in the media source.
    DWORD streamDescriptorCount = 0;
    SAFE_CALL(presentationDescriptor->GetStreamDescriptorCount(&streamDescriptorCount));

    // For each stream, create the topology nodes and add them to the topology.
    for (DWORD i = 0; i < streamDescriptorCount; ++i)
        SAFE_CALL(AddBranchToPartialTopology(topology, mediaSourcee, presentationDescriptor, i));

    *outTopology = topology.detach();

    return hr;
}

# undef SAFE_CALL

struct _player_t
{
    com_ptr<MediaPlayer>    player;
};

extern "C" player2_t WMFPlayerCreate()
{
    com_ptr<MediaPlayer> player;
    auto hr = MediaPlayer::Create(nullptr, &player);
    if (FAILED(hr))
        return nullptr;

    auto out = new _player_t();
    out->player = player;
    return out;
}

extern "C" void WMFPlayerDestroy(player2_t player)
{
    if (player)
        delete player;
}

extern "C" void WMFPlayerSetVolume(player2_t player, float volume)
{
    player->player->SetMasterVolume(volume);
}

extern "C" float WMFPlayerGetVolume(player2_t player)
{
    return player->player->GetMasterVolume();
}

extern "C" void WMFPlayerSetGain(player2_t player, float gainDb)
{
    player->player->SetReplayGain(gainDb);
}

extern "C" float WMFPlayerGetGain(player2_t player)
{
    return player->player->GetReplayGain();
}

extern "C" double WMFPlayerGetDuration(player2_t player)
{
    if (auto duration = player->player->GetDuration())
        return *duration;
    else
        return 0.0f;
}

extern "C" double WMFPlayerGetTime(player2_t player)
{
    if (auto duration = player->player->GetPresentationTime())
        return *duration;
    else
        return 0.0f;
}

extern "C" bool WMFPlayerOpen(player2_t player, const char* url)
{
    auto urlLength = strlen(url);
    if (urlLength == 0)
        return false;

    int bufferLength = ::MultiByteToWideChar(CP_UTF8, 0, url, urlLength, nullptr, 0);
    if (bufferLength == 0)
        return false;

    wchar_t* buffer = new wchar_t[bufferLength + 1];
    int result = ::MultiByteToWideChar(CP_UTF8, 0, url, urlLength, buffer, bufferLength);
    if (result == 0)
    {
        delete [] buffer;
        return false;
    }

    buffer[bufferLength] = 0;

    auto hr = player->player->OpenURL(buffer);

    delete[] buffer;

    while (player->player->GetState() == MediaPlayer::OpenPending)
        Sleep(1);

    return SUCCEEDED(hr);
}

extern "C" bool WMFPlayerPlay(player2_t player)
{
    return SUCCEEDED(player->player->Play());
}

extern "C" bool WMFPlayerPause(player2_t player)
{
    return SUCCEEDED(player->player->Pause());
}

extern "C" bool WMFPlayerStop(player2_t player)
{
    return SUCCEEDED(player->player->Stop());
}

extern "C" bool WMFPlayerFinish(player2_t player)
{
    return SUCCEEDED(player->player->Stop());
}

extern "C" bool WMFPlayerIsPlaying(player2_t player)
{
    auto state = player->player->GetState();
    return state == MediaPlayer::OpenPending || state == MediaPlayer::Started;
}

extern "C" bool WMFPlayerIsPaused(player2_t player)
{
    auto state = player->player->GetState();
    return state == MediaPlayer::Paused;
}

extern "C" bool WMFPlayerIsStopped(player2_t player)
{
    auto state = player->player->GetState();
    return state == MediaPlayer::Stopped || state == MediaPlayer::Closing || state == MediaPlayer::Closed;
}

extern "C" bool WMFPlayerIsFinished(player2_t player)
{
    return WMFPlayerIsStopped(player);
    //auto state = player->player->GetState();
    //return state == MediaPlayer::Closing || state == MediaPlayer::Closed;
}

extern "C" player2_iface player2_windows_media_foundation =
{
    /*.Name           =*/ "Windows Media Foundation",
    /*.Create         =*/ WMFPlayerCreate,
    /*.Destroy        =*/ WMFPlayerDestroy,
    /*.SetVolume      =*/ WMFPlayerSetVolume,
    /*.GetVolume      =*/ WMFPlayerGetVolume,
    /*.SetGain        =*/ WMFPlayerSetGain,
    /*.GetGain        =*/ WMFPlayerGetGain,
    /*.GetDuration    =*/ WMFPlayerGetDuration,
    /*.GetTime        =*/ WMFPlayerGetTime,
    /*.Open           =*/ WMFPlayerOpen,
    /*.Play           =*/ WMFPlayerPlay,
    /*.Pause          =*/ WMFPlayerPause,
    /*.Stop           =*/ WMFPlayerStop,
    /*.Finish         =*/ WMFPlayerFinish,
    /*.IsPlaying      =*/ WMFPlayerIsPlaying,
    /*.IsPaused       =*/ WMFPlayerIsPaused,
    /*.IsStopped      =*/ WMFPlayerIsStopped,
    /*.IsFinished     =*/ WMFPlayerIsFinished
};
