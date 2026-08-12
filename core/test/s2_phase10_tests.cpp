// core/test/s2_phase10_tests.cpp
// ---------------------------------------------------------------------------
// Sprint 2 Phase 10 (Commercial packaging) unit tests.
//
// Written by the scrum-master BEFORE the Phase 10 engineer implements the
// feature. The engineer must implement the contract documented below so these
// tests compile and pass. Do NOT weaken the assertions to make them pass;
// implement the feature to satisfy them.
//
// Covers (docs/s2-phase10-test.md): commercial packaging deliverables —
//   (A) a LICENSE / LICENSE.md describing the commercial license model,
//   (B) an installer script/config that packages the built app + DLLs,
//   (C) docs/user-guide.md covering install, run, and main features,
//   (D) docs/support.md describing support tiers/contact.
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / S1 phases.
// ---------------------------------------------------------------------------
// CONTRACT the Phase 10 engineer must provide (in the repo root):
//   LICENSE.md            -> non-empty, describes the commercial license model
//   packaging/installer.ps1 -> non-empty installer script/config
//   docs/user-guide.md    -> non-empty end-user guide (install/run/features)
//   docs/support.md       -> non-empty support model doc (tiers/contact)
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

// ---------------------------------------------------------------------------
// Lightweight test harness.
// ---------------------------------------------------------------------------
class Harness {
public:
    explicit Harness(const char* name) : name_(name) {}

    void section(const char* s) { std::printf("\n-- %s --\n", s); }

    void check(bool cond, const char* what) {
        if (cond) {
            std::printf("  [PASS] %s\n", what);
        } else {
            std::printf("  [FAIL] %s\n", what);
            ++failures_;
        }
    }

    int failures() const { return failures_; }
    const char* name() const { return name_; }

private:
    const char* name_;
    int failures_ = 0;
};

// ---------------------------------------------------------------------------
// Resolve the repository root from the test binary location.
// The binary lives at <root>/build/core/Release/lodestar_s2_phase10_tests.exe,
// so the root is three directories above the executable's directory.
// ---------------------------------------------------------------------------
std::string repoRoot() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring exe(buf, n);
    // Strip the file name, then go up three directories.
    size_t pos = exe.find_last_of(L"\\/");
    if (pos != std::wstring::npos) exe = exe.substr(0, pos);  // .../Release
    pos = exe.find_last_of(L"\\/");
    if (pos != std::wstring::npos) exe = exe.substr(0, pos);  // .../core
    pos = exe.find_last_of(L"\\/");
    if (pos != std::wstring::npos) exe = exe.substr(0, pos);  // .../build
    pos = exe.find_last_of(L"\\/");
    if (pos != std::wstring::npos) exe = exe.substr(0, pos);  // <root>
    // Convert wide path to UTF-8 (avoids narrowing wchar_t->char warnings).
    int len = WideCharToMultiByte(CP_UTF8, 0, exe.c_str(), (int)exe.size(),
                                  nullptr, 0, nullptr, nullptr);
    std::string root(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, exe.c_str(), (int)exe.size(),
                        &root[0], len, nullptr, nullptr);
    return root;
#else
    return ".";
#endif
}

// Returns true if the file exists and is non-empty.
bool fileExistsAndNonEmpty(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fclose(f);
    return size > 0;
}

// ---------------------------------------------------------------------------
// T1. license file exists
// ---------------------------------------------------------------------------
void testLicense(Harness& h, const std::string& root) {
    h.section("T1. license file exists");

    bool licenseOk = false;
    std::string found;
    const std::string licenseMd = root + "/LICENSE.md";
    const std::string license = root + "/LICENSE";
    if (fileExistsAndNonEmpty(licenseMd)) {
        licenseOk = true;
        found = "LICENSE.md";
    } else if (fileExistsAndNonEmpty(license)) {
        licenseOk = true;
        found = "LICENSE";
    }
    h.check(licenseOk, "LICENSE or LICENSE.md exists and is non-empty");
    if (licenseOk) {
        std::printf("  [INFO] found license file: %s\n", found.c_str());
    }
}

// ---------------------------------------------------------------------------
// T2. installer config exists
// ---------------------------------------------------------------------------
void testInstaller(Harness& h, const std::string& root) {
    h.section("T2. installer config exists");

    bool installerOk = false;
    std::string found;
    const std::string ps1 = root + "/packaging/installer.ps1";
    const std::string cpack = root + "/packaging/CPackConfig.cmake";
    if (fileExistsAndNonEmpty(ps1)) {
        installerOk = true;
        found = "packaging/installer.ps1";
    } else if (fileExistsAndNonEmpty(cpack)) {
        installerOk = true;
        found = "packaging/CPackConfig.cmake";
    }
    h.check(installerOk, "an installer script/config exists and is non-empty");
    if (installerOk) {
        std::printf("  [INFO] found installer config: %s\n", found.c_str());
    }
}

// ---------------------------------------------------------------------------
// T3. user guide exists
// ---------------------------------------------------------------------------
void testUserGuide(Harness& h, const std::string& root) {
    h.section("T3. user guide exists");

    const std::string path = root + "/docs/user-guide.md";
    h.check(fileExistsAndNonEmpty(path),
            "docs/user-guide.md exists and is non-empty");
}

// ---------------------------------------------------------------------------
// T4. support doc exists
// ---------------------------------------------------------------------------
void testSupport(Harness& h, const std::string& root) {
    h.section("T4. support doc exists");

    const std::string path = root + "/docs/support.md";
    h.check(fileExistsAndNonEmpty(path),
            "docs/support.md exists and is non-empty");
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    Harness h("S2 Phase 10 Commercial packaging");
    std::printf("S2 PHASE 10 COMMERCIAL PACKAGING TESTS\n");

    const std::string root = repoRoot();
    std::printf("  [INFO] repo root: %s\n", root.c_str());

    testLicense(h, root);
    testInstaller(h, root);
    testUserGuide(h, root);
    testSupport(h, root);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
