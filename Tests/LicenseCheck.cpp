// Developer tool: checks a licence key offline with the plug-in's own code. Usage: NFVocalSunshineLicenseCheck "<long key>"
#include <JuceHeader.h>
#include "../Source/License/NFVSLicense.h"
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf("usage: LicenseCheck \"<key>\"\n"); return 2; }
    const juce::String key(argv[1]);
    std::printf("short code: %s\n", NFVSLicense::looksShort(key) ? "yes" : "no");
    const auto r = NFVSLicense::check(key);
    std::printf("valid: %s  customer: %s  expires: %s  %s\n", r.valid ? "YES" : "no", r.customer.toRawUTF8(), r.expires.toRawUTF8(), r.message.toRawUTF8());
    return r.valid ? 0 : 1;
}
