#ifndef PICKLESCORE_OTA_CREDENTIALS_H
#define PICKLESCORE_OTA_CREDENTIALS_H
#include <cstdint>
#include <cstring>

namespace ota {
#pragma pack(push, 1)
struct Credentials {
    uint8_t schema;
    char ssid[33];
    char apPassword[33];
    uint8_t adminHash[32];
    uint8_t publicKey[65];
};
#pragma pack(pop)
static_assert(sizeof(Credentials) == 164, "Provisioning record must match offline tool");
inline bool credentialShapeValid(const Credentials& c)
{
    if (c.schema != 1 || !c.ssid[0] || c.ssid[32] || c.apPassword[32] || c.publicKey[0] != 4) return false;
    bool ended = false;
    for (size_t i = 0; i < 32; ++i) {
        char a = c.ssid[i], b = c.apPassword[i];
        if (!a) ended = true;
        else if (ended || !((a >= 'A' && a <= 'Z') || (a >= 'a' && a <= 'z') ||
                 (a >= '0' && a <= '9') || a == '-' || a == '_')) return false;
        if (!((b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z') || (b >= '0' && b <= '9') || b == '-' || b == '_')) return false;
    }
    uint8_t any = 0;
    for (uint8_t v : c.adminHash) any |= v;
    return any != 0;
}
}
#endif
