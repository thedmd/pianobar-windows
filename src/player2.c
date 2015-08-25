/*
Copyright (c) 2008-2014
	Lars-Dominik Braun <lars@6xq.net>

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

#include "config.h"
#include "player2.h"
#define COBJMACROS
#define INITGUID
#include <objbase.h>
#include <dshow.h>
#pragma comment(lib, "strmiids.lib")

# define WM_GRAPH_EVENT		(WM_APP + 1)

enum { NO_GRAPH, RUNNING, PAUSED, STOPPED };

struct _player_t {
	int				state;
	IGraphBuilder*	graph;
	IMediaControl*	control;
	IMediaEventEx*	event;
	IBasicAudio*	audio;
	IMediaSeeking*  media;
	float			volume; // dB
	float			gain;   // dB
};

static void BarPlayer2ApplyVolume(player2_t player) {
	long v = (long)((player->volume + player->gain) * 100.0f);

	if (!player->audio)
		return;

	if (v < -10000)
		v = -10000;
	if (v > 0)
		v = 0;

	IBasicAudio_put_Volume(player->audio, v);
}

static void BarPlayer2TearDown(player2_t player) {
	/* TODO: send final event */

	if (player->graph) {
		IGraphBuilder_Release(player->graph);
		player->graph = NULL;
	}

	if (player->control) {
		IMediaControl_Release(player->control);
		player->control = NULL;
	}

	if (player->event) {
		IMediaEventEx_Release(player->event);
		player->event = NULL;
	}

	if (player->audio) {
		IBasicAudio_Release(player->audio);
		player->audio = NULL;
	}

	if (player->media) {
		IMediaSeeking_Release(player->media);
		player->media = NULL;
	}

	player->state = NO_GRAPH;
}

static HRESULT BarPlayer2Build(player2_t player) {
	HRESULT hr;

	BarPlayer2TearDown(player);

	hr = CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, &player->graph);
	if (FAILED(hr))
		return hr;

	hr = IGraphBuilder_QueryInterface(player->graph, &IID_IMediaControl, &player->control);
	if (FAILED(hr))
		return hr;

	hr = IGraphBuilder_QueryInterface(player->graph, &IID_IMediaEventEx, &player->event);
	if (FAILED(hr))
		return hr;

	hr = IGraphBuilder_QueryInterface(player->graph, &IID_IBasicAudio, &player->audio);
	if (FAILED(hr))
		return hr;

	hr = IGraphBuilder_QueryInterface(player->graph, &IID_IMediaSeeking, &player->media);
	if (FAILED(hr))
		return hr;

	hr = IMediaEventEx_SetNotifyWindow(player->event, (OAHWND)NULL, WM_GRAPH_EVENT, (LONG_PTR)player);
	if (FAILED(hr))
		return hr;

	player->state = STOPPED;

	return S_OK;
}

static HRESULT BarPlayer2AddFilterByCLSID(IGraphBuilder *pGraph, REFGUID clsid, IBaseFilter **ppF, LPCWSTR wszName) {
	IBaseFilter *pFilter = NULL;
	HRESULT hr;
	*ppF = 0;

	hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, &pFilter);
	if (FAILED(hr))
		goto done;

	hr = IGraphBuilder_AddFilter(pGraph, pFilter, wszName);
	if (FAILED(hr))
		goto done;

	*ppF = pFilter;

	IBaseFilter_AddRef(*ppF);

done:
	if (pFilter)
		IBaseFilter_Release(pFilter);

	return hr;
}

static HRESULT BarPlayer2Render(player2_t player, IBaseFilter* source) {
	BOOL bRenderedAnyPin = FALSE;

	IPin* pin = NULL;
	IEnumPins *enumPins = NULL;
	IBaseFilter *audioRenderer = NULL;
	IFilterGraph2* filter = NULL;

	HRESULT hr;

	hr = IGraphBuilder_QueryInterface(player->graph, &IID_IFilterGraph2, &filter);
	if (FAILED(hr))
		return hr;

	hr = BarPlayer2AddFilterByCLSID(player->graph, &CLSID_DSoundRender, &audioRenderer, L"Audio Renderer");
	if (FAILED(hr))
		goto done;

	hr = IBaseFilter_EnumPins(source, &enumPins);
	if (FAILED(hr))
		goto done;

	while (S_OK == IEnumPins_Next(enumPins, 1, &pin, NULL))
	{
		HRESULT hr2 = IFilterGraph2_RenderEx(filter, pin, AM_RENDEREX_RENDERTOEXISTINGRENDERERS, NULL);

		IPin_Release(pin);
		if (SUCCEEDED(hr2))
			bRenderedAnyPin = TRUE;
	}

done:
	if (enumPins)
		IEnumPins_Release(enumPins);
	if (enumPins)
		IBaseFilter_Release(audioRenderer);
	if (enumPins)
		IFilterGraph2_Release(filter);

	if (SUCCEEDED(hr) && !bRenderedAnyPin)
		hr = VFW_E_CANNOT_RENDER;
	
	return hr;
}

bool BarPlayer2Init(player2_t* player) {

	if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED)))
		return false;

	player2_t out = malloc(sizeof(struct _player_t));
	if (!out)
		return false;

	memset(out, 0, sizeof(struct _player_t));

	*player = out;

	return true;
}

void BarPlayer2Destroy(player2_t player) {
	BarPlayer2TearDown(player);
	free(player);
}

void BarPlayer2SetVolume(player2_t player, float volume) {
	player->volume = volume;
	BarPlayer2ApplyVolume(player);
}

float BarPlayer2GetVolume(player2_t player) {
	return player->volume;
}

void BarPlayer2SetGain(player2_t player, float gain) {
	player->gain = gain;
	BarPlayer2ApplyVolume(player);
}

float BarPlayer2GetGain(player2_t player) {
	return player->gain;
}

double BarPlayer2GetDuration(player2_t player) {
	LONGLONG time;
	if (SUCCEEDED(IMediaSeeking_GetDuration(player->media, &time)))
		return time / 10000000.0;
	else
		return 0;
}

double BarPlayer2GetTime(player2_t player) {
	LONGLONG time;
	if (SUCCEEDED(IMediaSeeking_GetCurrentPosition(player->media, &time)))
		return time / 10000000.0;
	else
		return 0;
}

bool BarPlayer2Open(player2_t player, const char* url) {
	IBaseFilter* source = NULL;
	HRESULT hr;
	wchar_t* wideUrl = NULL;
	size_t urlSize;
	int result;

	hr = BarPlayer2Build(player);
	if (FAILED(hr))
		goto done;

	urlSize = strlen(url);
	result = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, url, urlSize, NULL, 0);
	wideUrl = malloc((result + 1) * sizeof(wchar_t));
	if (!wideUrl) {
		hr = E_OUTOFMEMORY;
		goto done;
	}
	memset(wideUrl, 0, (result + 1) * sizeof(wchar_t));

	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, url, urlSize, wideUrl, result);

	hr = IGraphBuilder_AddSourceFilter(player->graph, wideUrl, NULL, &source);
	if (FAILED(hr))
		goto done;

	hr = BarPlayer2Render(player, source);

	BarPlayer2ApplyVolume(player);

done:
	if (wideUrl)
		free(wideUrl);
	if (FAILED(hr))
		BarPlayer2TearDown(player);
	if (source)
		IBaseFilter_Release(source);

	return SUCCEEDED(hr);
}

bool BarPlayer2Play(player2_t player) {
	HRESULT hr;

	if (player->state != PAUSED && player->state != STOPPED)
		return false; /* wrong state */

	hr = IMediaControl_Run(player->control);
	if (SUCCEEDED(hr))
		player->state = RUNNING;

	return SUCCEEDED(hr);
}

bool BarPlayer2Pause(player2_t player) {
	HRESULT hr;

	if (player->state != RUNNING)
		return false; /* wrong state */

	hr = IMediaControl_Pause(player->control);
	if (SUCCEEDED(hr))
		player->state = PAUSED;

	return SUCCEEDED(hr);
}

bool BarPlayer2Stop(player2_t player) {
	HRESULT hr;

	if (player->state != RUNNING && player->state != PAUSED)
		return false; /* wrong state */

	hr = IMediaControl_Stop(player->control);
	if (SUCCEEDED(hr))
		player->state = STOPPED;

	return SUCCEEDED(hr);
}

bool BarPlayer2Finish(player2_t player) {
	if (!player->control)
		return false;

	BarPlayer2TearDown(player);
	return true;
}

bool BarPlayer2IsPlaying(player2_t player) {
	return player->state == RUNNING;
}

bool BarPlayer2IsPaused(player2_t player) {
	return player->state == PAUSED;
}

bool BarPlayer2IsStopped(player2_t player) {
	return player->state == STOPPED;
}

bool BarPlayer2IsFinished(player2_t player) {
	LONGLONG time;
	LONGLONG duration;

	if (!player->media)
		return true;

	if (FAILED(IMediaSeeking_GetDuration(player->media, &duration)) ||
		FAILED(IMediaSeeking_GetCurrentPosition(player->media, &time)))
		return true;

	return time >= duration;
}
