# ifndef __TD__BASIC_MEDIA_PLAYER_H__
# define __TD__BASIC_MEDIA_PLAYER_H__
# pragma once

# include <mfapi.h>
# include <mfidl.h>
# include "utility/com_ptr.h"
# include "utility/optional.h"

class MediaPlayer;

typedef void (*MediaPlayerEventCallback)(MediaPlayer* player, com_ptr<IMFMediaEvent>, MediaEventType);

class MediaPlayer:
    private IMFAsyncCallback
{
public:
    enum State
    {
        Closed = 0,     // no session
        Ready,          // session was created, ready to open file
        OpenPending,    // session is opening a file
        Started,        // session is playing a file
        Paused,         // session is paused
        Stopped,        // session is stopped (but ready to play)
        Closing         // session was closed, waiting for async callback
    };

    static HRESULT Create(MediaPlayerEventCallback eventCallback, MediaPlayer** outMediaPlayer);

    // IUnknown
    virtual ULONG   STDMETHODCALLTYPE AddRef() override;
    virtual ULONG   STDMETHODCALLTYPE Release() override;
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override;

    HRESULT OpenURL(const wchar_t* url);
    HRESULT Play();
    HRESULT Pause();
    HRESULT Stop();
    HRESULT Shutdown();

    State GetState() const;

    void  SetMasterVolume(float volume);
    float GetMasterVolume() const;

    void  SetReplayGain(float replayGain);
    float GetReplayGain() const;

    optional<float> GetPresentationTime() const;
    optional<float> GetDuration() const;

    void  SetUserData(void* userData);
    void* GetUserData() const;

    HRESULT HandleEvent(IMFMediaEvent* mediaEvent);

protected:
    MediaPlayer(MediaPlayerEventCallback eventCallback);
    virtual ~MediaPlayer();

    HRESULT Initialize();
    HRESULT CreateSession();
    HRESULT CloseSession();
    HRESULT StartPlayback();
    HRESULT ApplyTopology();

    HRESULT ApplyMasterVolume(float volume);
    HRESULT ApplyReplayGain(float replayGain);

    const com_ptr<IMFMediaSession>& GetMediaSession() const { return m_MediaSession; }
    const com_ptr<IMFMediaSource>&  GetMediaSource()  const { return m_MediaSource;  }

    virtual HRESULT OnTopologyStatus(IMFMediaEvent* mediaEvent);
    virtual HRESULT OnPresentationEnded(IMFMediaEvent* mediaEvent);
    virtual HRESULT OnNewPresentation(IMFMediaEvent* mediaEvent);

    // Override to handle additional session events.
    virtual HRESULT OnSessionEvent(IMFMediaEvent* mediaEvent, MediaEventType mediaEventType);

    virtual HRESULT OnStateChange(State state, State previousState);

private:
    // IMFAsyncCallback
    virtual HRESULT STDMETHODCALLTYPE GetParameters(DWORD* flags, DWORD* queue);
    virtual HRESULT STDMETHODCALLTYPE Invoke(IMFAsyncResult* asyncResult);

    HRESULT                         SetState(State state);

    ULONG                           m_RefCount;

    com_ptr<IMFSourceResolver>      m_SourceResolver;
    com_ptr<IUnknown>               m_CancelCookie;

    com_ptr<IMFMediaSession>        m_MediaSession;
    com_ptr<IMFMediaSource>         m_MediaSource;

    com_ptr<IMFSimpleAudioVolume>   m_SimpleAudioVolume;
    com_ptr<IMFPresentationClock>   m_PresentationClock;
    com_ptr<IMFAudioStreamVolume>   m_StreamAudioVolume;
    
    optional<float>                 m_SetMasterVolume;
    mutable float                   m_MasterVolume;
    mutable float                   m_ReplayGain;

    MediaPlayerEventCallback        m_EventCallback;

    void*                           m_UserData;

    State                           m_State;
    HANDLE                          m_CloseEvent;
};


# endif // __TD__BASIC_MEDIA_PLAYER_H__