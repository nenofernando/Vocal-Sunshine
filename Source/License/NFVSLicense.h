#pragma once
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
#include <cstring>
#include "NFVSLicensePublicKey.h"

// NF Vocal Sunshine's own licence: a key made by the licence panel (mixa-licence.nenovoice.workers.dev/admin) ("base64(payload).base64(signature)"), checked offline with the public key
// above (RSA-2048 PKCS#1 v1.5 / SHA-256 over the payload). The app stores the accepted key in
// <app data>/NF Vocal Sunshine/licence.key. Payload: NFVS2|customer name|expiry (YYYY-MM-DD or "never")|order id.
namespace NFVSLicense
{
inline juce::File keyFile() { return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("NF Audio Tools/NF Vocal Sunshine/licence.key"); }

struct Info { bool valid = false; juce::String customer, expires, message; };

inline Info check(const juce::String& rawKey)
{
    Info r;
    const auto key = rawKey.removeCharacters(" \t\r\n");
    const int dot = key.indexOfChar('.');
    if (dot <= 0) { r.message = "This is not a NF Vocal Sunshine licence key."; return r; }

    juce::MemoryOutputStream payloadStream, sigStream;
    if (!juce::Base64::convertFromBase64(payloadStream, key.substring(0, dot)) || !juce::Base64::convertFromBase64(sigStream, key.substring(dot + 1)))
    { r.message = "The key is damaged (check that it was pasted whole)."; return r; }
    const auto payload = payloadStream.toUTF8();

    // RSA PKCS#1 v1.5 with SHA-256 (the standard scheme, so a Cloudflare Worker / OpenSSL can sign it natively):
    // signature^e mod n must be  00 01 FF..FF 00 <DigestInfo of SHA-256> <hash>  (256 bytes)
    juce::RSAKey pub(juce::String(NFVSLicenseKey::exponentHex) + "," + juce::String(NFVSLicenseKey::modulusHex));
    if (!pub.isValid() || sigStream.getDataSize() != 256) { r.message = "This licence key is not valid."; return r; }
    juce::SHA256 sha(payload.toRawUTF8(), (size_t) payload.getNumBytesAsUTF8());
    auto hash = sha.getRawData();
    static const juce::uint8 prefix[] = { 0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20 };
    juce::uint8 expected[256];
    const int tLen = (int) sizeof(prefix) + 32;
    expected[0] = 0x00; expected[1] = 0x01;
    for (int i = 2; i < 256 - tLen - 1; ++i) expected[i] = 0xff;
    expected[256 - tLen - 1] = 0x00;
    std::memcpy(expected + 256 - tLen, prefix, sizeof(prefix));
    std::memcpy(expected + 256 - 32, hash.getData(), 32);

    juce::MemoryBlock sigLE(256);                       // the signature arrives big-endian; JUCE loads little-endian
    for (int i = 0; i < 256; ++i) static_cast<juce::uint8*>(sigLE.getData())[i] = static_cast<const juce::uint8*>(sigStream.getData())[255 - i];
    juce::BigInteger sig;
    sig.loadFromMemoryBlock(sigLE);
    if (!pub.applyToValue(sig)) { r.message = "This licence key is not valid."; return r; }
    const auto le = sig.toMemoryBlock();                // little-endian, without leading zeros
    juce::uint8 got[256] = {};
    for (size_t i = 0; i < le.getSize() && i < 256; ++i) got[255 - i] = static_cast<const juce::uint8*>(le.getData())[i];
    if (std::memcmp(got, expected, 256) != 0) { r.message = "This licence key is not valid."; return r; }

    const auto parts = juce::StringArray::fromTokens(payload, "|", "");
    if (parts.size() < 4 || parts[0] != "NFVS2") { r.message = "This licence key is not valid."; return r; }
    r.customer = parts[1];
    r.expires = parts[2];
    if (r.expires != "never")
    {
        const auto d = juce::StringArray::fromTokens(r.expires, "-", "");
        if (d.size() != 3) { r.message = "This licence key is not valid."; return r; }
        const auto end = juce::Time(d[0].getIntValue(), d[1].getIntValue() - 1, d[2].getIntValue(), 23, 59, 59, 0, true);
        if (juce::Time::getCurrentTime() > end) { r.message = "This licence key expired on " + r.expires + "."; return r; }
    }
    r.valid = true;
    return r;
}

// ---- Online activation: at most 2 machines per licence (the Worker in site/license-worker decides and the panel lists the machines) ----
#ifndef NFVS_LICENCE_SERVER
 #define NFVS_LICENCE_SERVER "https://mixa-licence.nenovoice.workers.dev"
#endif
enum class Online { Ok, Refused, NoNetwork };
struct OnlineResult { Online state = Online::NoNetwork; juce::String message, longKey; };   // longKey: the signed key the server returns for a short code

// A short code ("NFVS-XXXX-XXXX-XXXX-XXXX") is only a code: the server trades it for the signed long key (first activation needs internet).
inline bool looksShort(const juce::String& rawKey)
{
    const auto k = rawKey.removeCharacters(" \t\r\n");
    return k.isNotEmpty() && !k.contains(".") && k.length() >= 8 && k.length() <= 40 && k.containsOnly("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-");
}

inline juce::File stampFile() { return keyFile().getSiblingFile("licence.verified"); }
inline juce::String machineId() { return juce::SHA256((juce::SystemStats::getUniqueDeviceID() + "|nf-vocal-sunshine").toUTF8()).toHexString(); }

// Asks the Worker to register this machine for the key. Refused = the server said no (revoked / 2 machines already / unknown key);
// NoNetwork = could not reach the server (the caller decides about the offline grace).
inline OnlineResult activateOnline(const juce::String& rawKey)
{
    OnlineResult r;
    auto* body = new juce::DynamicObject();
    body->setProperty("key", rawKey.removeCharacters(" \t\r\n"));
    body->setProperty("machine_id", machineId());
    body->setProperty("machine_name", juce::SystemStats::getComputerName());
   #if JUCE_WINDOWS
    body->setProperty("os", "windows");
   #elif JUCE_MAC
    body->setProperty("os", "macos");
   #else
    body->setProperty("os", "other");
   #endif
    int status = 0;
    auto stream = juce::URL(juce::String(NFVS_LICENCE_SERVER) + "/activate")
                      .withPOSTData(juce::JSON::toString(juce::var(body), true))
                      .createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                             .withExtraHeaders("Content-Type: application/json")
                                             .withConnectionTimeoutMs(8000)
                                             .withStatusCode(&status));
    if (stream == nullptr || status == 0 || status >= 500)
    {
        r.message = "Could not reach the licence server. Check your internet connection and try again.";
        return r;
    }
    const auto reply = juce::JSON::parse(stream->readEntireStreamAsString());
    if (status == 200 && (bool) reply["ok"]) { r.state = Online::Ok; r.longKey = reply["license_key"].toString(); return r; }
    r.state = Online::Refused;
    const auto err = reply["error"].toString();
    if (err == "limit")        r.message = "This licence is already active on " + juce::String((int) reply["max"]) + " computers. Contact NF Audio Tools to move it to another one.";
    else if (err == "revoked") r.message = "This licence is no longer active. Contact NF Audio Tools.";
    else if (err == "unknown") r.message = "This licence key was not found. Check that it was pasted whole.";
    else                       r.message = "The licence server did not accept this key.";
    return r;
}

inline void writeStamp() { stampFile().replaceWithText(juce::String(juce::Time::currentTimeMillis())); }
inline void clearLicence() { keyFile().deleteFile(); stampFile().deleteFile(); }
inline juce::int64 daysSinceVerified()
{
    const auto f = stampFile();
    if (!f.existsAsFile()) return 1000000;
    return (juce::Time::currentTimeMillis() - f.loadFileAsString().getLargeIntValue()) / (24 * 60 * 60 * 1000);
}
inline bool needsOnlineCheck() { return daysSinceVerified() >= 7; }       // the machine reports itself to the server about once a week
inline bool withinOfflineGrace() { return daysSinceVerified() < 30; }     // no internet for up to 30 days is fine

inline bool isLicensed()
{
    const auto f = keyFile();
    return f.existsAsFile() && check(f.loadFileAsString()).valid;
}
}
