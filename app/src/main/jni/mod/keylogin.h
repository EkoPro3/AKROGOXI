#pragma once

#include <json/json.hpp>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <random>

#include "include/obfuscate.h"

// ─── Server Config ────────────────────────────────────────────────────────────
// Server: venomkey.com/connect (HTTPS POST)
// Payload: { "license_key": key, "hwid": androidID, "game_type": "8ball", "version": "1.0" }
// Response: { "status": "success", "data": { "expiry_date": "...", "auth_token": "..." } }
// ─────────────────────────────────────────────────────────────────────────────

bool bValid = false;

std::string xor_encrypt(const std::string& data, const std::string& key) {
    std::string result;
    result.reserve(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        result += data[i] ^ key[i % key.length()];
    }
    return result;
}

std::string xor_decrypt(const std::string& data, const std::string& key) {
    return xor_encrypt(data, key);
}

std::string base64_encode(const std::string& data) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    size_t i = 0;
    unsigned char a3[3];
    unsigned char a4[4];

    while (data.length() - i >= 3) {
        a3[0] = static_cast<unsigned char>(data[i]);
        a3[1] = static_cast<unsigned char>(data[i + 1]);
        a3[2] = static_cast<unsigned char>(data[i + 2]);
        a4[0] = (a3[0] & 0xfc) >> 2;
        a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
        a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
        a4[3] = a3[2] & 0x3f;
        for (int j = 0; j < 4; j++) result += chars[a4[j]];
        i += 3;
    }

    if (data.length() - i > 0) {
        int remaining = data.length() - i;
        for (int j = 0; j < 3; j++)
            a3[j] = (j < remaining) ? static_cast<unsigned char>(data[i + j]) : 0;
        a4[0] = (a3[0] & 0xfc) >> 2;
        a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
        a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
        a4[3] = a3[2] & 0x3f;
        for (int j = 0; j < remaining + 1; j++) result += chars[a4[j]];
        while (result.length() % 4) result += '=';
    }
    return result;
}

std::string base64_decode(const std::string& input) {
    static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[static_cast<unsigned char>(chars[i])] = i;
    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            result.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return result;
}

// ─── HTTP POST helper using libcurl ──────────────────────────────────────────
#include <curl/curl.h>

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* out) {
    size_t totalSize = size * nmemb;
    out->append((char*)contents, totalSize);
    return totalSize;
}

INLINE std::string httpPost(const std::string& url, const std::string& jsonBody) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string responseBuffer;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)jsonBody.size());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOGE("curl_easy_perform() failed: %s", curl_easy_strerror(res));
        return "";
    }
    return responseBuffer;
}

// ─── State variables ──────────────────────────────────────────────────────────
std::string ERROR_MESSAGE = "";
static bool logged_in     = false;
static bool is_logging_in = false;
std::string g_Token, g_Auth;
std::string g_ExpTime = "N/A";

// ─── Main Login Function ──────────────────────────────────────────────────────
INLINE bool Login(std::string androidID, std::string key) {

    if (androidID.empty()) {
        ERROR_MESSAGE = O("Could not get Android ID");
        return false;
    }
    if (key.empty()) {
        ERROR_MESSAGE = O("Key Is Empty");
        return false;
    }

    is_logging_in = true;
    ERROR_MESSAGE = "";

    try {
        // Build JSON payload
        nlohmann::json payload = {
            {"license_key", key},
            {"hwid",        androidID},
            {"game_type",   OO("8ball").str()},
            {"version",     OO("1.0").str()}
        };

        std::string serverUrl = OO("https://venomkey.com/connect").str();
        std::string response  = httpPost(serverUrl, payload.dump());

        if (response.empty()) {
            ERROR_MESSAGE = OO("No response from server").str();
            is_logging_in = false;
            return false;
        }

        nlohmann::json jsonResp = nlohmann::json::parse(response);

        std::string status = jsonResp.value("status", "");
        if (status != "success") {
            ERROR_MESSAGE = jsonResp.value("message", OO("Invalid key or unauthorized").str());
            is_logging_in = false;
            return false;
        }

        // Success
        std::string expiryDate   = "";
        std::string serverVersion = "";
        std::string authToken    = "";

        if (jsonResp.contains("data")) {
            auto data = jsonResp["data"];
            try { expiryDate    = data.value("expiry_date", ""); }  catch(...) {}
            try { serverVersion = data.value("version", "");     }  catch(...) {}
            try { authToken     = data.value("auth_token", "");  }  catch(...) {}
        }

        logged_in     = true;
        is_logging_in = false;
        g_Token       = OO("0wQRlDkgoQlf").str();
        g_Auth        = OO("0wQRlDkgoQlf").str();
        g_ExpTime     = expiryDate.empty() ? "N/A" : expiryDate;
        bValid        = (g_Token == g_Auth);

        persistent_string["key"] = key;
        save_persistence();

        return true;

    } catch (const std::exception& e) {
        ERROR_MESSAGE = OO("Error: ").str() + std::string(e.what());
        is_logging_in = false;
        return false;
    }
}
