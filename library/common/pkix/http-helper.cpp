/*
 * Copyright (c) 2021, The UAPKI Project Authors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#else
#define CURL_STATICLIB
#include "curl/curl.h"
#endif
#include "ba-utils.h"
#include "http-helper.h"
#include "uapkic.h"
#include "uapki-errors.h"
#include "uapki-ns.h"
#include <string.h>
#include <stdlib.h>
#include <map>
#include <mutex>
#include <vector>
#include <limits>


#define DEBUG_OUTCON(expression)
#ifndef DEBUG_OUTCON
#define DEBUG_OUTCON(expression) expression
#endif


using namespace std;


const char* HttpHelper::CONTENT_TYPE_APP_JSON       = "Content-Type:application/json";
const char* HttpHelper::CONTENT_TYPE_OCSP_REQUEST   = "Content-Type:application/ocsp-request";
const char* HttpHelper::CONTENT_TYPE_TSP_REQUEST    = "Content-Type:application/timestamp-query";


struct HTTP_HELPER {
    bool    isInitialized;
    bool    offlineMode;
    string  proxyUrl;
    string  proxyCredentials;
#if defined(_WIN32)
    HINTERNET   session;
    wstring     proxyUsername;
    wstring     proxyPassword;
#endif
    mutex   mtx;
    map<string, mutex>
            mtxByUrl;

    HTTP_HELPER (void)
        : isInitialized(false)
        , offlineMode(false)
#if defined(_WIN32)
        , session(nullptr)
#endif
    {}

    void reset (void)
    {
#if defined(_WIN32)
        if (session) {
            WinHttpCloseHandle(session);
            session = nullptr;
        }
        proxyUsername.clear();
        proxyPassword.clear();
#endif
        isInitialized = false;
        offlineMode = false;
        proxyUrl.clear();
        proxyCredentials.clear();
    }
};  //  end struct HTTP_HELPER

static HTTP_HELPER http_helper;


#if defined(_WIN32)

static wstring utf8_to_wstring (const string& value)
{
    if (value.empty()) return wstring();
    const int required = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), (int)value.size(), nullptr, 0);
    if (required <= 0) return wstring();
    wstring result((size_t)required, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), (int)value.size(), &result[0], required);
    return result;
}

static wstring normalize_proxy (const string& proxy)
{
    if (proxy.empty()) return wstring();
    string hostport = proxy;
    const size_t pos = hostport.find("//");
    if (pos != string::npos) {
        hostport = hostport.substr(pos + 2);
    }
    return utf8_to_wstring(hostport);
}

static void split_credentials (const string& creds, wstring& username, wstring& password)
{
    const size_t sep = creds.find(':');
    if (sep == string::npos) {
        username = utf8_to_wstring(creds);
        password.clear();
    }
    else {
        username = utf8_to_wstring(creds.substr(0, sep));
        password = utf8_to_wstring(creds.substr(sep + 1));
    }
}

static int winhttp_perform_request (
        const string& uri,
        const wchar_t* method,
        const vector<wstring>& headers,
        const uint8_t* body,
        size_t body_len,
        ByteArray** baResponse)
{
    if (http_helper.offlineMode) {
        if (baResponse) *baResponse = nullptr;
        return RET_UAPKI_OFFLINE_MODE;
    }
    if (!http_helper.isInitialized || !http_helper.session) {
        if (baResponse) *baResponse = nullptr;
        return RET_UAPKI_GENERAL_ERROR;
    }

    if (baResponse) {
        *baResponse = nullptr;
    }

    const wstring wideUri = utf8_to_wstring(uri);
    if (wideUri.empty()) {
        return RET_UAPKI_GENERAL_ERROR;
    }

    URL_COMPONENTS components;
    memset(&components, 0, sizeof(components));
    components.dwStructSize = sizeof(components);
    components.dwHostNameLength = (DWORD)-1;
    components.dwUrlPathLength = (DWORD)-1;
    components.dwExtraInfoLength = (DWORD)-1;

    if (!WinHttpCrackUrl(wideUri.c_str(), 0, 0, &components)) {
        return RET_UAPKI_CONNECTION_ERROR;
    }

    wstring host(components.lpszHostName, components.dwHostNameLength);
    wstring path;
    if (components.dwUrlPathLength > 0) {
        path.assign(components.lpszUrlPath, components.dwUrlPathLength);
    }
    if (components.dwExtraInfoLength > 0) {
        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }
    if (path.empty()) {
        path = L"/";
    }

    INTERNET_PORT port = components.nPort;
    if (port == 0) {
        port = (components.nScheme == INTERNET_SCHEME_HTTPS) ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
    }
    DWORD flags = (components.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;

    HINTERNET hConnect = WinHttpConnect(http_helper.session, host.c_str(), port, 0);
    if (!hConnect) {
        return RET_UAPKI_CONNECTION_ERROR;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, method, path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        return RET_UAPKI_CONNECTION_ERROR;
    }

    if (!http_helper.proxyCredentials.empty() && !http_helper.proxyUsername.empty()) {
        WinHttpSetOption(
            hRequest,
            WINHTTP_OPTION_PROXY_USERNAME,
            (LPVOID)http_helper.proxyUsername.c_str(),
            (DWORD)((http_helper.proxyUsername.size() + 1) * sizeof(wchar_t))
        );
        WinHttpSetOption(
            hRequest,
            WINHTTP_OPTION_PROXY_PASSWORD,
            (LPVOID)http_helper.proxyPassword.c_str(),
            (DWORD)((http_helper.proxyPassword.size() + 1) * sizeof(wchar_t))
        );
    }

    for (const auto& header : headers) {
        if (!header.empty()) {
            if (!WinHttpAddRequestHeaders(hRequest, header.c_str(), (DWORD)header.size(), WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE)) {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                return RET_UAPKI_CONNECTION_ERROR;
            }
        }
    }

    if (body_len > (size_t)numeric_limits<DWORD>::max()) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return RET_UAPKI_GENERAL_ERROR;
    }

    DWORD dwLength = (body && body_len > 0) ? (DWORD)body_len : 0;
    LPVOID bodyPtr = (body && body_len > 0) ? (LPVOID)body : WINHTTP_NO_REQUEST_DATA;

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, bodyPtr, dwLength, dwLength, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return RET_UAPKI_CONNECTION_ERROR;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return RET_UAPKI_CONNECTION_ERROR;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return RET_UAPKI_CONNECTION_ERROR;
    }

    int ret = (status == 200) ? RET_OK : RET_UAPKI_HTTP_STATUS_NOT_OK;

    if (baResponse) {
        ByteArray* response = ba_alloc();
        if (!response) {
            ret = RET_UAPKI_GENERAL_ERROR;
        }
        else {
            size_t total = 0;
            while (true) {
                DWORD available = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &available)) {
                    ret = RET_UAPKI_CONNECTION_ERROR;
                    break;
                }
                if (available == 0) {
                    break;
                }
                if (ba_change_len(response, total + available) != RET_OK) {
                    ret = RET_UAPKI_GENERAL_ERROR;
                    break;
                }
                uint8_t* buf = ba_get_buf(response);
                if (!buf) {
                    ret = RET_UAPKI_GENERAL_ERROR;
                    break;
                }
                DWORD read = 0;
                if (!WinHttpReadData(hRequest, buf + total, available, &read)) {
                    ret = RET_UAPKI_CONNECTION_ERROR;
                    break;
                }
                total += read;
                if (read < available) {
                    (void)ba_change_len(response, total);
                }
            }
            if (ret == RET_OK) {
                (void)ba_change_len(response, total);
                *baResponse = response;
            }
            else {
                ba_free(response);
                *baResponse = nullptr;
            }
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    return ret;
}

static string build_basic_auth_header (const char* userPwd)
{
    if (!userPwd || (userPwd[0] == '\0')) {
        return string();
    }
    ByteArray* baCreds = ba_alloc_from_uint8((const uint8_t*)userPwd, strlen(userPwd));
    if (!baCreds) {
        return string();
    }
    char* encoded = nullptr;
    string header;
    if (ba_to_base64_with_alloc(baCreds, &encoded) == RET_OK && encoded) {
        header = string("Authorization: Basic ") + encoded;
    }
    ba_free(baCreds);
    if (encoded) {
        free(encoded);
    }
    return header;
}

#else

static size_t cb_curlwrite (
        void* dataIn,
        size_t size,
        size_t nmemb,
        void* userp
)
{
    size_t realsize = size * nmemb;
    ByteArray* data = (ByteArray*)userp;
    size_t old_len = ba_get_len(data);
    uint8_t* buf;

    ba_change_len(data, old_len + realsize);
    buf = (uint8_t*)ba_get_buf(data);
    memcpy(&(buf[old_len]), dataIn, realsize);

    return realsize;
}   //  cb_curlwrite

static bool curl_set_url_and_proxy (
        CURL* curl,
        const string& uri
)
{
    CURLcode rv_ccode = curl_easy_setopt(curl, CURLOPT_URL, uri.c_str());
    if (rv_ccode != CURLE_OK) return false;

    if (!http_helper.proxyUrl.empty()) {
        rv_ccode = curl_easy_setopt(curl, CURLOPT_PROXY, http_helper.proxyUrl.c_str());
        if (rv_ccode != CURLE_OK) return false;

        curl_easy_setopt(curl, CURLOPT_PROXYAUTH, CURLAUTH_ANY);

        if (!http_helper.proxyCredentials.empty()) {
            rv_ccode = curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, http_helper.proxyCredentials.c_str());
            if (rv_ccode != CURLE_OK) return false;
        }
    }

    return true;
}   //  curl_set_url_and_proxy
#endif


int HttpHelper::init (
        const bool offlineMode,
        const char* proxyUrl,
        const char* proxyCredentials
)
{
    http_helper.offlineMode = offlineMode;
#if defined(_WIN32)
    if (!http_helper.isInitialized) {
        wstring proxyWide;
        DWORD accessType = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
        LPCWSTR proxyName = WINHTTP_NO_PROXY_NAME;
        if (proxyUrl && proxyUrl[0]) {
            http_helper.proxyUrl = string(proxyUrl);
            proxyWide = normalize_proxy(http_helper.proxyUrl);
            if (!proxyWide.empty()) {
                accessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
                proxyName = proxyWide.c_str();
            }
        }
        http_helper.session = WinHttpOpen(L"UAPKI Native", accessType, proxyName, WINHTTP_NO_PROXY_BYPASS, 0);
        http_helper.isInitialized = (http_helper.session != nullptr);
        if (!http_helper.isInitialized) {
            return RET_UAPKI_GENERAL_ERROR;
        }
    }
    if (proxyUrl && proxyUrl[0]) {
        http_helper.proxyUrl = string(proxyUrl);
    }
    else {
        http_helper.proxyUrl.clear();
    }
    if (proxyCredentials && proxyCredentials[0]) {
        http_helper.proxyCredentials = string(proxyCredentials);
        split_credentials(http_helper.proxyCredentials, http_helper.proxyUsername, http_helper.proxyPassword);
    }
    else {
        http_helper.proxyCredentials.clear();
        http_helper.proxyUsername.clear();
        http_helper.proxyPassword.clear();
    }
    return RET_OK;
#else
    int ret = RET_OK;
    if (!http_helper.isInitialized) {
        const CURLcode curl_code = curl_global_init(CURL_GLOBAL_ALL);
        http_helper.isInitialized = (curl_code == CURLE_OK);
        if (proxyUrl && http_helper.isInitialized) {
            http_helper.proxyUrl = string(proxyUrl);
            if (proxyCredentials && !http_helper.proxyUrl.empty()) {
                http_helper.proxyCredentials = string(proxyCredentials);
            }
        }
        ret = (http_helper.isInitialized) ? RET_OK : RET_UAPKI_GENERAL_ERROR;
    }
    return ret;
#endif
}

void HttpHelper::deinit (void)
{
#if defined(_WIN32)
    if (http_helper.isInitialized) {
        http_helper.reset();
    }
#else
    if (http_helper.isInitialized) {
        http_helper.reset();
        curl_global_cleanup();
    }
#endif
}

bool HttpHelper::isOfflineMode (void)
{
    return http_helper.offlineMode;
}

const string& HttpHelper::getProxyUrl (void)
{
    return http_helper.proxyUrl;
}

int HttpHelper::get (
        const string& uri,
        ByteArray** baResponse
)
{
#if defined(_WIN32)
    vector<wstring> headers;
    return winhttp_perform_request(uri, L"GET", headers, nullptr, 0, baResponse);
#else
    DEBUG_OUTCON(printf("HttpHelper::get(uri='%s')\n", uri.c_str()));
    CURL* curl;
    CURLcode curl_code;
    int ret;

    if (http_helper.offlineMode) {
        return RET_UAPKI_OFFLINE_MODE;
    }

    // get a curl handle 
    if ((curl = curl_easy_init()) == NULL) {
        return RET_UAPKI_CONNECTION_ERROR;
    }

    // First set the URL that is about to receive our POST. This URL can
    // just as well be a https:// URL if that is what should receive the data.
    if (!curl_set_url_and_proxy(curl, uri)) {
        return RET_UAPKI_CONNECTION_ERROR;
    }

    // send all data to this function
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb_curlwrite);

    *baResponse = ba_alloc();
    // we pass our 'chunk' struct to the callback function
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, *baResponse);

    // Perform the request, res will get the return code
    curl_code = curl_easy_perform(curl);
    if (curl_code == CURLE_OK) {
        long http_code = 0;
        curl_code = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        ret = (http_code == 200) ? RET_OK : RET_UAPKI_HTTP_STATUS_NOT_OK;
    }
    else {
        ret = RET_UAPKI_CONNECTION_ERROR;
    }

    // always cleanup
    curl_easy_cleanup(curl);

    return ret;
#endif
}

int HttpHelper::post (
        const string& uri,
        const char* contentType,
        const ByteArray* baRequest,
        ByteArray** baResponse
)
{
#if defined(_WIN32)
    vector<wstring> headers;
    if (contentType && contentType[0]) {
        headers.push_back(utf8_to_wstring(contentType));
    }
    const uint8_t* body = baRequest ? ba_get_buf_const(baRequest) : nullptr;
    const size_t bodyLen = baRequest ? ba_get_len(baRequest) : 0;
    return winhttp_perform_request(uri, L"POST", headers, body, bodyLen, baResponse);
#else
    DEBUG_OUTCON(
        printf("HttpHelper::post(uri='%s', contentType='%s'), Request:\n", uri.c_str(), contentType);
        ba_print(stdout, baRequest);
    )
    CURL* curl;
    CURLcode curl_code;
    int ret;

    if (http_helper.offlineMode) {
        return RET_UAPKI_OFFLINE_MODE;
    }

    // get a curl handle 
    if ((curl = curl_easy_init()) == NULL) {
        return RET_UAPKI_GENERAL_ERROR;
    }

    struct curl_slist* chunk = NULL;

    // Add a custom header 
    chunk = curl_slist_append(chunk, contentType);

    // set our custom set of headers
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

    // First set the URL that is about to receive our POST. This URL can
    // just as well be a https:// URL if that is what should receive the data.
    if (!curl_set_url_and_proxy(curl, uri)) {
        return RET_UAPKI_CONNECTION_ERROR;
    }
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    // Now specify the POST data
    // binary data
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, ba_get_buf_const(baRequest));
    // size of the POST data
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)ba_get_len(baRequest));

    // send all data to this function
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb_curlwrite);

    *baResponse = ba_alloc();
    // we pass our 'chunk' struct to the callback function
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, *baResponse);

    // Perform the request, res will get the return code
    curl_code = curl_easy_perform(curl);
    if (curl_code == CURLE_OK) {
        long http_code = 0;
        curl_code = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        ret = (http_code == 200) ? RET_OK : RET_UAPKI_HTTP_STATUS_NOT_OK;
    }
    else {
        ret = RET_UAPKI_CONNECTION_ERROR;
    }

    // always cleanup
    curl_easy_cleanup(curl);
    curl_slist_free_all(chunk);

    return ret;
#endif
}

int HttpHelper::post (
        const string& uri,
        const char* contentType,
        const char* userPwd,
        const string& authorizationBearer,
        const string& request,
        ByteArray** baResponse
)
{
#if defined(_WIN32)
    vector<wstring> headers;
    if (contentType && contentType[0]) {
        headers.push_back(utf8_to_wstring(contentType));
    }
    const string basicHeader = build_basic_auth_header(userPwd);
    if (!basicHeader.empty()) {
        headers.push_back(utf8_to_wstring(basicHeader));
    }
    if (!authorizationBearer.empty()) {
        headers.push_back(utf8_to_wstring(authorizationBearer));
    }
    const uint8_t* body = request.empty() ? nullptr : (const uint8_t*)request.data();
    return winhttp_perform_request(uri, L"POST", headers, body, request.size(), baResponse);
#else
    DEBUG_OUTCON(
        printf("HttpHelper::post(uri='%s', contentType='%s', userPwd='%s', authorizationBearer='%s', request='%s')\n",
                uri.c_str(), contentType, userPwd, authorizationBearer.c_str(), request.c_str());
    )
    CURL* curl;
    CURLcode curl_code;
    int ret;

    if (http_helper.offlineMode) {
        return RET_UAPKI_OFFLINE_MODE;
    }

    // get a curl handle 
    if ((curl = curl_easy_init()) == NULL) {
        return RET_UAPKI_GENERAL_ERROR;
    }

    if (userPwd) {
        curl_easy_setopt(curl, CURLOPT_USERPWD, userPwd);
    }

#ifdef HTTP_HELPER_DISABLE_VERIFYSSL
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
#endif

    struct curl_slist* chunk = NULL;

    // Add a custom header 
    chunk = curl_slist_append(chunk, contentType);
    if (!authorizationBearer.empty()) {
        chunk = curl_slist_append(chunk, authorizationBearer.c_str());
    }

    // set our custom set of headers
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

    // First set the URL that is about to receive our POST. This URL can
    // just as well be a https:// URL if that is what should receive the
    // data.
    if (!curl_set_url_and_proxy(curl, uri)) {
        return RET_UAPKI_CONNECTION_ERROR;
    }
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    // Now specify the POST data
    // binary data
    if (!request.empty()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.c_str());
    }
    // size of the POST data
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)request.length());

    // send all data to this function
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb_curlwrite);

    *baResponse = ba_alloc();
    // we pass our 'chunk' struct to the callback function
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, *baResponse);

    // Perform the request, res will get the return code
    curl_code = curl_easy_perform(curl);
    if (curl_code == CURLE_OK) {
        long http_code = 0;
        curl_code = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        ret = (http_code == 200) ? RET_OK : RET_UAPKI_HTTP_STATUS_NOT_OK;
    }
    else {
        ret = RET_UAPKI_CONNECTION_ERROR;
    }

    // always cleanup
    curl_easy_cleanup(curl);

    return ret;
#endif
}

mutex& HttpHelper::lockUri (
        const string& uri
)
{
    lock_guard<mutex> lock(http_helper.mtx);

    return http_helper.mtxByUrl[uri];
}

vector<string> HttpHelper::randomURIs (
        const vector<string>& uris
)
{
    if (uris.size() < 2) return uris;

    UapkiNS::SmartBA sba_randoms;
    if (!sba_randoms.set(ba_alloc_by_len(uris.size() - 1))) return uris;

    if (drbg_random(sba_randoms.get()) != RET_OK) return uris;

    vector<string> rv_uris, src = uris;
    const uint8_t* buf = sba_randoms.buf();
    for (size_t i = 0; i < uris.size() - 1; i++) {
        const size_t rnd = buf[i] % src.size();
        rv_uris.push_back(src[rnd]);
        src.erase(src.begin() + rnd);
    }
    rv_uris.push_back(src[0]);
    return rv_uris;
}
