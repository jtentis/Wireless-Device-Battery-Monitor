// Keychron wireless mice (VID 3434, e.g. M4), from Keychron's own web Launcher (launcher.keychron.com).
// 64-byte output report without report id: [01][00][81][01][0...][checksum], checksum (byte 63)
// = 0xA1 - sum(bytes 0..62). The reply [01][..][..][01][state][rate][vid lo][vid hi][pid lo][pid hi]
// [charging][battery %]... carries the mouse's own vendor id, which is checked so that only a
// genuine reply is accepted.
//
// Keychron keyboards on the Keychron Link receiver (3434:D030): the receiver's raw HID interface
// (usage page FF60, usage 61, 32-byte reports, no report id) answers B2 itself with its state:
// [B2][slots] then per slot [vid lo][vid hi][pid lo][pid hi][connected]. Stock keyboard firmware
// never answers a battery query through the receiver; firmware with the KC_GET_BATTERY patch
// (ldkv/qmk_firmware#1, needs WILRESS_RAW_ENABLE) answers A4 with
// [A4][percent][mV lo][mV hi][0 = on battery / 1 = charging / 2 = full][transport][model].
// Without that patch the query times out and nothing is reported (Bluetooth still works).
#include "drivers.h"

namespace {

// Sends a 32-byte request to the receiver's raw HID interface and waits for a reply starting with `cmd`.
bool LinkQuery(HidDev& d, BYTE cmd, std::vector<BYTE>& r, int timeoutMs) {
    std::vector<BYTE> req(33, 0);  // report id 0 (unnumbered) + 32 bytes
    req[1] = cmd;
    d.Flush();
    if (!d.Write(req)) return false;
    ULONGLONG end = GetTickCount64() + timeoutMs;
    for (;;) {
        int left = (int)((LONGLONG)end - (LONGLONG)GetTickCount64());
        if (left <= 0 || d.Read(r, left) < 8) return false;
        if (r[0] == cmd) return true;
    }
}

// Keychron keyboards with the KC_GET_BATTERY patch, either behind a Keychron Link receiver or
// plugged in directly with the cable (then the keyboard itself owns the FF60 interface).
void PollKeychronLinkKeyboard(PollContext& ctx, USHORT vid, USHORT pid) {
    for (const HidColl* c : ctx.Find(vid, pid, [](const HidColl& c) { return c.page() == 0xFF60 && c.usage() == 0x61 && c.outLen() == 33; })) {
        HidDev d;
        if (!d.Open(*c)) continue;
        std::vector<BYTE> r;
        USHORT kbPid = 0;
        bool viaLink = LinkQuery(d, 0xB2, r, 500);
        if (viaLink) {
            int slots = r[1] < 5 ? r[1] : 5;
            for (int i = 0; i < slots && 2 + 5 * i + 4 < (int)r.size(); i++) {
                const BYTE* s = &r[2 + 5 * i];
                if ((s[0] | (s[1] << 8)) == 0x3434 && s[4]) { kbPid = (USHORT)(s[2] | (s[3] << 8)); break; }
            }
            if (!kbPid) return;  // keyboard off, asleep or on the cable: the app keeps its last value
            // After the link has been idle, the first request is lost while it wakes up (a stray
            // reply arrives instead); a second one is answered within ~60 ms.
            if (!LinkQuery(d, 0xA4, r, 1000) && !LinkQuery(d, 0xA4, r, 1000)) return;  // stock firmware
        } else {
            // Not a receiver (a keyboard answers B2 with VIA's "unhandled" FF): ask it directly.
            kbPid = pid;
            if (!LinkQuery(d, 0xA4, r, 500)) continue;  // stock firmware answers FF, never A4
        }
        if (r[1] > 100) return;
        DeviceStatus s;
        s.key = L"keychron-kb:" + Hex4(kbPid);  // same key over the receiver and the cable
        s.name = (kbPid >= 0x0A50 && kbPid <= 0x0A58) ? L"Keychron K5 Max" : L"Keychron keyboard";
        s.kind = DeviceKind::Keyboard;
        s.via = viaLink ? L"2.4 GHz receiver" : L"USB cable";
        s.percent = r[1];
        s.millivolts = r[2] | (r[3] << 8);
        s.charging = r[4] == 1;  // 0 on battery, 1 charging, 2 full
        ctx.Add(s, vid, pid);
        return;
    }
}

void SetChecksum(std::vector<BYTE>& payload) {  // payload = 64 bytes without the report id
    unsigned sum = 0;
    for (int i = 0; i < 63; i++) sum += payload[i];
    payload[63] = (BYTE)(0xA1 - sum);
}

std::wstring CleanName(std::wstring n) {
    if (n.empty() || n.find(L"Link") != std::wstring::npos || n.find(L"Receiver") != std::wstring::npos) return L"Keychron mouse";
    return n;
}

}  // namespace

void PollKeychron(PollContext& ctx) {
    for (auto [vid, pid] : ctx.Products(0x3434)) {
        for (const HidColl* c : ctx.Find(vid, pid, [](const HidColl& c) { return c.page() >= 0xFF00 && c.outLen() == 65 && c.inLen() == 65; })) {
            HidDev d;
            if (!d.Open(*c)) continue;
            std::vector<BYTE> payload(64, 0);
            payload[0] = 0x01;
            payload[2] = 0x81;
            payload[3] = 0x01;
            SetChecksum(payload);
            std::vector<BYTE> req(1, 0x00);  // report id 0 (unnumbered)
            req.insert(req.end(), payload.begin(), payload.end());
            d.Flush();
            if (!d.Write(req)) continue;
            std::vector<BYTE> r;
            bool ok = false;
            ULONGLONG end = GetTickCount64() + 600;
            while ((LONGLONG)end - (LONGLONG)GetTickCount64() > 0) {
                int left = (int)((LONGLONG)end - (LONGLONG)GetTickCount64());
                if (d.Read(r, left) < 12) break;
                USHORT replyVid = (USHORT)(r[6] | (r[7] << 8));
                if (r[0] == 0x01 && r[3] == 0x01 && replyVid == 0x3434) { ok = true; break; }
            }
            if (!ok || r[11] > 100) continue;
            DeviceStatus s;
            s.key = L"keychron:" + Hex4(pid);
            s.name = CleanName(HidProduct(*c));
            s.kind = DeviceKind::Mouse;
            s.via = L"2.4 GHz receiver";
            s.percent = r[11];
            s.charging = r[10] != 0;
            ctx.Add(s, vid, pid);
            break;
        }
        PollKeychronLinkKeyboard(ctx, vid, pid);
    }
}
