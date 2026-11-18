// SONiX keyboards on their 2.4 GHz dongle: AJAZZ AK820 MAX ("AJAZZ AK820 MAX 2.4G", 05AC:024F, tested),
// AULA F108 Pro (same 05AC:024F dongle) and AJAZZ AK980 V2 (0C45:FEFE). The dongle reuses Apple's
// vendor id, so it is also matched by its vendor collection: interface 3, usage page FF60 / usage 61,
// 33-byte input and output reports (report id 0 + 32 bytes).
//   request [20 01 00 ... sum]          sum = byte sum of the 31 bytes before it
//   reply   [20 01 00 level ... sum]    level in percent; same checksum
// The keyboard only reports its level while it is switched to 2.4G mode. The protocol is read-only.
// From github.com/donglover69/sonix-keyboard-studio (README, "2.4G dongles").
#include "drivers.h"

namespace {

const std::pair<USHORT, USHORT> kDongles[] = { { 0x05AC, 0x024F }, { 0x0C45, 0xFEFE } };

const size_t kLen = 32;  // payload bytes after the report id; the last one is the checksum

BYTE Sum(const std::vector<BYTE>& b, size_t from, size_t n) {
    unsigned s = 0;
    for (size_t i = from; i < from + n; i++) s += b[i];
    return (BYTE)(s & 0xFF);
}

// Returns the level byte, or -1 when the dongle did not answer.
int QueryLevel(HidDev& d, size_t payloadLen = 32) {
    std::vector<BYTE> req(1 + payloadLen, 0);  // [0] = report id 0
    req[1] = 0x20;
    req[2] = 0x01;
    req[payloadLen] = Sum(req, 1, payloadLen - 1);
    d.Flush();
    if (!d.Write(req)) return -1;
    ULONGLONG end = GetTickCount64() + 500;
    for (;;) {
        int left = (int)((LONGLONG)end - (LONGLONG)GetTickCount64());
        if (left <= 0) return -1;
        std::vector<BYTE> r;
        int n = d.Read(r, left);  // report id 0 is stripped: r[0] is the first payload byte
        if (n < 0) return -1;
        if (n < (int)payloadLen || r[0] != 0x20 || r[1] != 0x01) continue;
        bool sumOk = (r[payloadLen - 1] == Sum(r, 0, payloadLen - 1)) || (r[payloadLen - 1] == 0);
        if (!sumOk) continue;
        return r[3];
    }
}

std::wstring KeyboardName(const HidColl& c) {
    std::wstring name = HidProduct(c);
    const std::wstring suffix = L" 2.4G";
    if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
        name.resize(name.size() - suffix.size());
    if (name.find(L"Atom") != std::wstring::npos || name.find(L"HE68") != std::wstring::npos)
        return L"Fantech Atom HE68 PRO";
    return name.empty() ? L"Wireless keyboard" : name;
}

}  // namespace

void PollSonix(PollContext& ctx) {
    for (auto [vid, pid] : kDongles) {
        if (ctx.IsClaimed(vid, pid)) continue;
        for (const HidColl* c : ctx.Find(vid, pid, [](const HidColl& c) {
                 return (c.page() == 0xFF60 || c.page() == 0xFF59) && c.usage() == 0x0061 &&
                        (c.outLen() == 33 || c.outLen() == 65) && c.outLen() == c.inLen();
             })) {
            HidDev d;
            if (!d.Open(*c)) continue;
            size_t payloadLen = c->outLen() - 1;
            // The first request can go unanswered while the 2.4 GHz link wakes up; retry once.
            int level = QueryLevel(d, payloadLen);
            if (level < 0) level = QueryLevel(d, payloadLen);
            if (level < 1 || level > 100) continue;
            DeviceStatus s;
            s.key = L"sonix:" + Hex4(vid) + L":" + Hex4(pid);
            s.name = KeyboardName(*c);
            s.kind = DeviceKind::Keyboard;
            s.via = L"2.4 GHz receiver";
            s.connected = true;
            s.percent = level;
            s.graceSec = 1800;  // keep the last value if a sleeping keyboard stops answering
            ctx.Add(s, vid, pid);
            break;
        }
    }
}
