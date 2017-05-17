/*
Copyright (c) 2015
	Michał Cichoń <thedmd@interia.pl>

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

#include "config.h"
#include "http.h"
#include <Windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

struct _http_t {
	HINTERNET		session;
	HINTERNET		connection;
	wchar_t*		endpoint;
	wchar_t*		securePort;
	wchar_t*		autoProxy;
	wchar_t*		proxy;
	wchar_t*		proxyUsername;
	wchar_t*		proxyPassword;
	char*			error;
};

static char* HttpToString(const wchar_t* wideString, int size);
static wchar_t* HttpToWideString(const char* string, int size);
static bool HttpCreateConnection (http_t http);
static void HttpCloseConnection (http_t http);
static void HttpSetLastError (http_t http, const char* message);
static void HttpSetLastErrorW (http_t http, const wchar_t* message);
static void HttpSetLastErrorFromWinHttp (http_t http);
static char* HttpFormatWinApiError (DWORD errorCode, HINSTANCE module);
static char* HttpFormatWinHttpError (DWORD errorCode);
static void HttpClearProxy (http_t http);

# define WINHTTP_SAFE(condition) do { if (condition) break; HttpSetLastErrorFromWinHttp (http); return false; } while (false)
# define WINHTTP_SAFE_DONE(condition) do { if (condition) break; HttpSetLastErrorFromWinHttp (http); goto done; } while (false)

static char* HttpToString(const wchar_t* wideString, int size) {
	int utfSize = WideCharToMultiByte(CP_UTF8, 0, wideString, size, NULL, 0, NULL, NULL);
	char* utfMessage = malloc(utfSize + 1);
	if (utfMessage)	{
		utfMessage[utfSize] = 0;
		WideCharToMultiByte(CP_UTF8, 0, wideString, size, utfMessage, utfSize, NULL, NULL);
	}
	return utfMessage;
}

static wchar_t* HttpToWideString(const char* string, int size) {
	int wideSize = MultiByteToWideChar(CP_UTF8, 0, string, size, NULL, 0);
	int wideBytes = (wideSize + 1) * sizeof(wchar_t);
	wchar_t* wideMessage = malloc(wideBytes);
	if (wideMessage) {
		wideMessage[wideSize] = 0;
		MultiByteToWideChar(CP_UTF8, 0, string, size, wideMessage, wideSize);
	}
	return wideMessage;
}


static bool HttpCreateConnection (http_t http) {
	INTERNET_PORT defaultPort = INTERNET_DEFAULT_PORT;

	HttpCloseConnection (http);

	http->session = WinHttpOpen(
		L"WinHTTP/1.0",
		WINHTTP_ACCESS_TYPE_NO_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS,
		0);
	WINHTTP_SAFE(http->session != NULL);

	WinHttpSetTimeouts(http->session,
		60 * 1000,  // DNS time-out
		60 * 1000,  // connect time-out
		30 * 1000,  // send time-out
		30 * 1000); // receive time-out

	http->connection = WinHttpConnect(
		http->session,
		http->endpoint,
		defaultPort,
		0);
	WINHTTP_SAFE(http->connection != NULL);

	return true;
}

static void HttpCloseConnection (http_t http) {
	if (http->connection) {
		WinHttpCloseHandle(http->connection);
		http->connection = NULL;
	}

	if (http->session) {
		WinHttpCloseHandle(http->session);
		http->session = NULL;
	}
}

static void HttpSetLastError (http_t http, const char* message) {
	free(http->error);
	http->error = NULL;

	if (message)
		http->error = strdup(message);
}

static void HttpSetLastErrorW (http_t http, const wchar_t* message) {
	free(http->error);
	http->error = NULL;

	if (message)
		http->error = HttpToString(message, wcslen(message));
}

static void HttpSetLastErrorFromWinHttp (http_t http) {
	free(http->error);
	http->error = NULL;

	DWORD error = GetLastError();
	if (error)
		http->error = HttpFormatWinHttpError(error);
}

static char* HttpFormatWinApiError (DWORD errorCode, HINSTANCE module) {
	const int source_flag = module ? FORMAT_MESSAGE_FROM_HMODULE : FORMAT_MESSAGE_FROM_SYSTEM;

	HLOCAL buffer = NULL;
	int bufferLength = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS | source_flag,
		(void*)module,
		errorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&buffer,
		0,
		NULL);

	if (bufferLength > 0) {
		char* message;

		wchar_t* wideMessage = (wchar_t*)buffer;

		/* Drop new line from the end. */
		wchar_t* wideMessageBack = wideMessage + bufferLength - 1;
		while (wideMessageBack > wideMessage && (*wideMessageBack == '\r' || *wideMessageBack == '\n'))
			--wideMessageBack;

		message = HttpToString (wideMessage, wideMessageBack - wideMessage + 1);

		LocalFree(buffer);

		return message;
	}
	else
		return NULL;
}

static char* HttpFormatWinHttpError (DWORD errorCode) {
	if (errorCode >= WINHTTP_ERROR_BASE && errorCode <= WINHTTP_ERROR_LAST) {
		HMODULE module = GetModuleHandleW(L"WinHTTP.dll");
		if (module) {
			char* message = HttpFormatWinApiError(errorCode, module);
			if (message)
				return message;
		}
	}

	return HttpFormatWinApiError(errorCode, NULL);
}

bool HttpInit(http_t* http, const char* endpoint, const char* securePort) {
	http_t out = malloc(sizeof(struct _http_t));
	if (!out)
		return false;
	memset(out, 0, sizeof(struct _http_t));

	out->endpoint   = HttpToWideString(endpoint, -1);
	out->securePort = HttpToWideString(securePort, -1);

	if (!HttpCreateConnection (out)) {
		HttpDestroy (out);
		return false;
	}

	*http = out;
	return true;
}

void HttpDestroy(http_t http) {
	if (http) {
		free(http->endpoint);
		free(http->securePort);
		http->endpoint = NULL;
		http->securePort = NULL;
		HttpCloseConnection (http);
		HttpClearProxy (http);
	}
	free(http);
}

static void HttpClearProxy (http_t http) {
	if (http->autoProxy) {
		free(http->autoProxy);
		http->autoProxy = NULL;
	}

	if (http->proxy) {
		free(http->proxy);
		http->proxy = NULL;
	}

	if (http->proxyUsername) {
		free(http->proxyUsername);
		http->proxyUsername = NULL;
	}

	if (http->proxyPassword) {
		free(http->proxyPassword);
		http->proxyPassword = NULL;
	}
}

bool HttpSetAutoProxy (http_t http, const char* url) {
	HttpClearProxy (http);
	if (HttpSetProxy (http, url)) {
		http->autoProxy = http->proxy;
		http->proxy     = NULL;
		return true;
	}
	else
		return false;
}

bool HttpSetProxy (http_t http, const char* url) {
	URL_COMPONENTS urlComponents;
	wchar_t* wideUrl = NULL;
	wchar_t* wideUrl2 = NULL;
	wchar_t* wideUsername = NULL;
	wchar_t* widePassword = NULL;

	ZeroMemory(&urlComponents, sizeof(urlComponents));
	urlComponents.dwStructSize      = sizeof(urlComponents);
	urlComponents.dwUserNameLength = -1;
	urlComponents.dwPasswordLength = -1;

	wideUrl = HttpToWideString(url, -1);
	if (WinHttpCrackUrl(wideUrl, wcslen(wideUrl), 0, &urlComponents)) {
		if (urlComponents.lpszUserName && urlComponents.dwUserNameLength > 0) {
			wideUsername = wcsdup(urlComponents.lpszUserName);
			wideUsername[urlComponents.dwUserNameLength] = 0;
		}
		if (urlComponents.lpszPassword && urlComponents.dwPasswordLength > 0) {
			widePassword = wcsdup(urlComponents.lpszPassword);
			widePassword[urlComponents.dwPasswordLength] = 0;
		}
	}

	ZeroMemory(&urlComponents, sizeof(urlComponents));
	urlComponents.dwStructSize = sizeof(urlComponents);
	urlComponents.dwHostNameLength  = -1;
	urlComponents.dwUrlPathLength   = -1;

	if (!WinHttpCrackUrl(wideUrl, wcslen(wideUrl), 0, &urlComponents)) {
		free(wideUsername);
		free(widePassword);
		return false;
	}

	if (urlComponents.lpszHostName && urlComponents.dwHostNameLength > 0) {
		wideUrl2 = wcsdup(urlComponents.lpszHostName);
		wideUrl2[urlComponents.lpszUrlPath - urlComponents.lpszHostName] = 0;
	}

	free(wideUrl);

	HttpClearProxy(http);
	http->proxy         = wideUrl2;
	http->proxyUsername = wideUsername;
	http->proxyPassword = widePassword;
	return true;
}

bool HttpRequest(http_t http, PianoRequest_t * const request) {
	HINTERNET handle = NULL;
	wchar_t* wideQuery = NULL;
	bool requestSent = false;
	bool complete = false;
	int retryLimit = 3;
	size_t responseDataSize;

	wideQuery = HttpToWideString(request->urlPath, -1);
	WINHTTP_SAFE_DONE(wideQuery != NULL);

	handle = WinHttpOpenRequest(
		http->connection,
		L"POST",
		wideQuery,
		L"HTTP/1.1",
		WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES,
		request->secure ? WINHTTP_FLAG_SECURE : 0);
	WINHTTP_SAFE_DONE(handle != NULL);

	if (http->proxy || http->autoProxy) {
		wchar_t* fullUrl;
		DWORD fullUrlSize = 0;
		WINHTTP_PROXY_INFO proxyInfo;
		bool success;

		if (http->autoProxy) {
			WINHTTP_AUTOPROXY_OPTIONS proxyOptions = { 0 };

			success = WinHttpQueryOption(request, WINHTTP_OPTION_URL, NULL, &fullUrlSize);
			WINHTTP_SAFE(!success && GetLastError() == ERROR_INSUFFICIENT_BUFFER);
			fullUrl = calloc(1, fullUrlSize + 1);
			success = WinHttpQueryOption(request, WINHTTP_OPTION_URL, fullUrl, &fullUrlSize);
			if (!success) {
				free(fullUrl);
				WINHTTP_SAFE(success);
			}

			proxyOptions.lpszAutoConfigUrl = http->autoProxy;
			proxyOptions.dwFlags = WINHTTP_AUTOPROXY_CONFIG_URL;

			if (!(success = WinHttpGetProxyForUrl(http->session, fullUrl, &proxyOptions, &proxyInfo))) {
				proxyOptions.fAutoLogonIfChallenged = true;
				success = WinHttpGetProxyForUrl(http->session, fullUrl, &proxyOptions, &proxyInfo);
			}

			if (!success) {
				free(fullUrl);
				WINHTTP_SAFE(success);
			}
		}
		else {
			proxyInfo.dwAccessType    = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
			proxyInfo.lpszProxy       = http->proxy;
			proxyInfo.lpszProxyBypass = NULL;
		}

		WINHTTP_SAFE(WinHttpSetOption(handle,
			WINHTTP_OPTION_PROXY,
			&proxyInfo, sizeof(proxyInfo)));

		if (http->proxyUsername && http->proxyPassword) {
			WINHTTP_SAFE(WinHttpSetCredentials(handle,
				WINHTTP_AUTH_TARGET_PROXY,
				WINHTTP_AUTH_SCHEME_BASIC,
				http->proxyUsername,
				http->proxyPassword,
				NULL));
		}
	}

	while (retryLimit > 0) {
		DWORD errorCode, statusCode, statusCodeSize;
		bool succeeded = false;
		bool retry = false;

		if (!requestSent) {
			size_t postDataSize = strlen(request->postData);
			succeeded = WinHttpSendRequest(handle,
				WINHTTP_NO_ADDITIONAL_HEADERS,
				0,
				request->postData,
				postDataSize,
				postDataSize,
				0);

			if (succeeded)
				requestSent = true;
		}

		if (requestSent)
			succeeded = WinHttpReceiveResponse(handle, NULL);

		errorCode = GetLastError();

		statusCode = 0;
		statusCodeSize = sizeof(statusCode);
		if (!WinHttpQueryHeaders(handle,
				WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
				WINHTTP_HEADER_NAME_BY_INDEX,
				&statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX)) {
			statusCode = 0;
		}

		if (succeeded && statusCode == 407) {
			wchar_t statusText[256] = { 0 };
			DWORD statusTextSize = sizeof(statusText) - 1;
			WinHttpQueryHeaders(handle,
				WINHTTP_QUERY_STATUS_TEXT,
				WINHTTP_HEADER_NAME_BY_INDEX,
				statusText, &statusTextSize, WINHTTP_NO_HEADER_INDEX);
			HttpSetLastErrorW (http, statusText);
			requestSent = false;
			retry       = true;
		}
		else {
			if (errorCode == ERROR_SUCCESS)
				break;

			switch (errorCode) {
				case ERROR_WINHTTP_RESEND_REQUEST:
					requestSent = false;
					/* pass trough */

				case ERROR_WINHTTP_NAME_NOT_RESOLVED:
				case ERROR_WINHTTP_CANNOT_CONNECT:
				case ERROR_WINHTTP_TIMEOUT:
					retry = true;
					break;

				default:
					HttpSetLastErrorFromWinHttp (http);
					goto done;
			}
		}

		if (retry)
			--retryLimit;
	}

	responseDataSize = 0;
	while (retryLimit > 0)
	{
		DWORD bytesLeft;
		char* writePtr;

		DWORD bytesAvailable = 0;
		if (!WinHttpQueryDataAvailable(handle, &bytesAvailable)) {
			WINHTTP_SAFE(GetLastError() == ERROR_WINHTTP_TIMEOUT);
			--retryLimit;
			continue;
		}

		if (0 == bytesAvailable)
			break;

		responseDataSize += bytesAvailable;
		request->responseData = realloc(request->responseData, responseDataSize + 1);

		writePtr = request->responseData + responseDataSize - bytesAvailable;
		writePtr[bytesAvailable] = 0;

		bytesLeft = bytesAvailable;
		while (bytesLeft > 0)
		{
			DWORD bytesRead = 0;
			if (!WinHttpReadData(handle, writePtr, bytesLeft, &bytesRead))
			{
				WINHTTP_SAFE(GetLastError() == ERROR_WINHTTP_TIMEOUT);
				if (--retryLimit == 0)
					break;

				continue;
			}

			bytesLeft -= bytesRead;
			writePtr  += bytesRead;
		}

		if (bytesLeft > 0)
			HttpSetLastError (http, "Maximum retries count exceeded");
	}

	if (retryLimit == 0)
		goto done;

	complete = true;

	HttpSetLastError (http, NULL);

done:
	free(wideQuery);
	return complete;
}

const char* HttpGetError(http_t http) {
	return http->error;
}
