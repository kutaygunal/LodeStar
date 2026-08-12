// core/test/s1_phase5_tests.cpp
// ---------------------------------------------------------------------------
// Sprint 1 Phase 5 (Real-time / determinism validation) tests (test-first).
//
// Written by the scrum-master BEFORE the Phase 5 engineer implements the
// feature. The engineer must implement the contract documented below so these
// tests compile and pass. Do NOT weaken the assertions to make them pass;
// implement the feature to satisfy them.
//
// Covers (docs/s1-phase5-test.md): a benchmark harness that times a core
// operation (TraceGraph query) with a monotonic clock, runs it M times,
// records min/avg/max microseconds, and appends a dated section to
// docs/reports/s1-phase5-benchmarks.md. Includes a determinism check (same
// input -> byte-identical output).
//
// Uses the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G.
// Each DB-dependent test opens its own fresh throwaway DB and runs migrations.
// ---------------------------------------------------------------------------

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/common/Result.h"
#include "core/persistence/Database.h"
#include "core/persistence/MigrationRunner.h"
#include "core/persistence/Models.h"
#include "core/tracelink/TraceGraph.h"

#ifndef LODESTAR_MIGRATIONS_DIR
#define LODESTAR_MIGRATIONS_DIR "core/persistence/migrations"
#endif

namespace tl = lodestar::tracelink;
namespace p  = lodestar::persistence;

namespace {

std::string g_migrationsDir = LODESTAR_MIGRATIONS_DIR;

// Report path is relative to the working directory (the repo root).
const char* kReportPath = "docs/reports/s1-phase5-benchmarks.md";

// Opens a fresh throwaway DB for one test, runs migrations, returns true on ok.
bool openFreshDb(p::Database& db, const char* file) {
    std::remove(file);
    std::remove((std::string(file) + "-wal").c_str());
    std::remove((std::string(file) + "-shm").c_str());
    if (db.open(file).failed()) return false;
    p::MigrationRunner runner(db);
    auto mig = runner.run(g_migrationsDir);
    return mig.isOk();
}

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
// Determinism serialization: canonical byte strings for a result set.
// ---------------------------------------------------------------------------
std::string serializeRequirements(const std::vector<p::Requirement>& reqs) {
    std::ostringstream os;
    for (const auto& r : reqs) {
        os << r.id << '|' << r.externalId << '|' << r.name << '|' << r.description
           << '|' << r.status << '|' << r.version << '|' << r.sortOrder << '\n';
    }
    return os.str();
}

std::string serializeLinks(const std::vector<p::TraceLink>& links) {
    std::ostringstream os;
    for (const auto& l : links) {
        os << l.id << '|' << l.sourceType << '|' << l.sourceId << '|'
           << l.targetType << '|' << l.targetId << '|' << l.relation << '|'
           << l.status << '|' << l.version << '\n';
    }
    return os.str();
}

// ---------------------------------------------------------------------------
// Benchmark helpers (monotonic clock, microseconds).
// ---------------------------------------------------------------------------
struct BenchResult {
    long long minUs = 0;
    long long avgUs = 0;
    long long maxUs = 0;
};

// Runs `fn` M times, returns min/avg/max microseconds.
template <typename Fn>
BenchResult timeIt(int m, Fn&& fn) {
    std::vector<long long> samples;
    samples.reserve(static_cast<size_t>(m));
    for (int i = 0; i < m; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        fn();
        auto t1 = std::chrono::steady_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        samples.push_back(us);
    }
    long long minUs = samples[0], maxUs = samples[0], sum = 0;
    for (auto s : samples) {
        if (s < minUs) minUs = s;
        if (s > maxUs) maxUs = s;
        sum += s;
    }
    return BenchResult{minUs, sum / m, maxUs};
}

std::string nowStamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buf);
}

// Appends a dated section to the benchmark report. Creates the file/dir if
// needed. Returns true on success.
bool appendReport(const std::string& op, int n, int m, const BenchResult& r) {
    try {
        std::filesystem::create_directories("docs/reports");
    } catch (...) {
        return false;
    }
    std::ofstream out(kReportPath, std::ios::app);
    if (!out) return false;
    out << "## S1 Phase 5 Benchmark — " << nowStamp() << "\n";
    out << "- operation: " << op << "\n";
    out << "- N: " << n << ", M: " << m << "\n";
    out << "- min_us: " << r.minUs << ", avg_us: " << r.avgUs
        << ", max_us: " << r.maxUs << "\n";
    out << "\n";
    out.close();
    return out.good();
}

// ---------------------------------------------------------------------------
// Seed helpers.
// ---------------------------------------------------------------------------
// Inserts `n` requirements and `n` test cases, then `n` verifies links
// (test_case i verifies requirement i). Returns false on any failure.
bool seedGraph(tl::TraceGraph& g, int n) {
    std::vector<std::string> reqIds;
    reqIds.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        p::Requirement r;
        r.externalId = "REQ-" + std::to_string(i);
        r.name = "Requirement " + std::to_string(i);
        r.description = "Body of requirement " + std::to_string(i);
        r.status = "Approved";
        auto res = g.addRequirement(r);
        if (res.failed()) return false;
        reqIds.push_back(r.id);
    }
    std::vector<std::string> tcIds;
    tcIds.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        p::TestCase t;
        t.externalId = "TC-" + std::to_string(i);
        t.name = "Test " + std::to_string(i);
        t.description = "Test body " + std::to_string(i);
        auto res = g.addTestCase(t);
        if (res.failed()) return false;
        tcIds.push_back(t.id);
    }
    for (int i = 0; i < n; ++i) {
        p::TraceLink link;
        link.sourceType = "test_case";
        link.sourceId = tcIds[static_cast<size_t>(i)];
        link.targetType = "requirement";
        link.targetId = reqIds[static_cast<size_t>(i)];
        link.relation = "verifies";
        auto res = g.addLink(link);
        if (res.failed()) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// T1. Determinism: same input -> same output
// ---------------------------------------------------------------------------
void testDeterminismSameInput(Harness& h) {
    h.section("T1. Determinism: same input -> same output");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s1p5_t1.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    tl::TraceGraph g(db);
    h.check(seedGraph(g, 5), "seed 5 requirements + test cases + verifies links");

    // Run requirements() twice.
    auto r1 = g.requirements();
    auto r2 = g.requirements();
    h.check(r1.isOk() && r2.isOk(), "requirements() ok on both runs");
    if (r1.isOk() && r2.isOk()) {
        std::string s1 = serializeRequirements(r1.value());
        std::string s2 = serializeRequirements(r2.value());
        h.check(s1 == s2, "requirements() byte-identical across two runs");
    }

    // Run linksTo("requirement", id) twice for the first requirement.
    auto reqs = g.requirements();
    if (reqs.isOk() && !reqs.value().empty()) {
        const std::string id = reqs.value()[0].id;
        auto l1 = g.linksTo("requirement", id);
        auto l2 = g.linksTo("requirement", id);
        h.check(l1.isOk() && l2.isOk(), "linksTo() ok on both runs");
        if (l1.isOk() && l2.isOk()) {
            std::string s1 = serializeLinks(l1.value());
            std::string s2 = serializeLinks(l2.value());
            h.check(s1 == s2, "linksTo() byte-identical across two runs");
        }
    } else {
        h.check(false, "requirements() returned rows for linksTo determinism");
    }

    db.close();
    std::remove("lodestar_s1p5_t1.db");
    std::remove("lodestar_s1p5_t1.db-wal");
    std::remove("lodestar_s1p5_t1.db-shm");
}

// ---------------------------------------------------------------------------
// T2. Determinism: repeated query is stable (5 runs)
// ---------------------------------------------------------------------------
void testDeterminismRepeated(Harness& h) {
    h.section("T2. Determinism: repeated query is stable (5 runs)");

    p::Database db;
    if (!openFreshDb(db, "lodestar_s1p5_t2.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    tl::TraceGraph g(db);
    h.check(seedGraph(g, 5), "seed 5 requirements + test cases + verifies links");

    std::string baseline;
    bool first = true;
    bool stable = true;
    for (int i = 0; i < 5; ++i) {
        auto r = g.requirements();
        if (!r.isOk()) {
            stable = false;
            break;
        }
        std::string s = serializeRequirements(r.value());
        if (first) {
            baseline = s;
            first = false;
        } else if (s != baseline) {
            stable = false;
        }
    }
    h.check(stable, "requirements() identical across 5 repeated runs");

    // Also check linksTo stability across 5 runs.
    auto reqs = g.requirements();
    if (reqs.isOk() && !reqs.value().empty()) {
        const std::string id = reqs.value()[0].id;
        std::string linkBaseline;
        bool lfirst = true;
        bool lstable = true;
        for (int i = 0; i < 5; ++i) {
            auto l = g.linksTo("requirement", id);
            if (!l.isOk()) {
                lstable = false;
                break;
            }
            std::string s = serializeLinks(l.value());
            if (lfirst) {
                linkBaseline = s;
                lfirst = false;
            } else if (s != linkBaseline) {
                lstable = false;
            }
        }
        h.check(lstable, "linksTo() identical across 5 repeated runs");
    } else {
        h.check(false, "requirements() returned rows for linksTo stability");
    }

    db.close();
    std::remove("lodestar_s1p5_t2.db");
    std::remove("lodestar_s1p5_t2.db-wal");
    std::remove("lodestar_s1p5_t2.db-shm");
}

// ---------------------------------------------------------------------------
// T3. Benchmark: graph query timing is recorded
// ---------------------------------------------------------------------------
void testBenchmarkQuery(Harness& h) {
    h.section("T3. Benchmark: graph query timing is recorded");

    const int N = 100;
    const int M = 100;

    p::Database db;
    if (!openFreshDb(db, "lodestar_s1p5_t3.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }
    tl::TraceGraph g(db);
    h.check(seedGraph(g, N), "seed N=100 requirements + test cases + verifies links");

    auto reqs = g.requirements();
    if (!reqs.isOk() || reqs.value().empty()) {
        h.check(false, "requirements() returned rows for query benchmark");
        db.close();
        return;
    }
    const std::string id = reqs.value()[0].id;

    // Time a combined graph query (requirements() + linksTo) over M iterations.
    auto res = timeIt(M, [&]() {
        (void)g.requirements();
        (void)g.linksTo("requirement", id);
    });

    std::printf("  graph_query: N=%d M=%d min=%lldus avg=%lldus max=%lldus\n",
                N, M, res.minUs, res.avgUs, res.maxUs);
    h.check(res.minUs >= 0 && res.avgUs >= 0 && res.maxUs >= 0,
            "query benchmark produced non-negative timings");

    bool recorded = appendReport("graph_query", N, M, res);
    h.check(recorded, "appended graph_query section to benchmark report");

    db.close();
    std::remove("lodestar_s1p5_t3.db");
    std::remove("lodestar_s1p5_t3.db-wal");
    std::remove("lodestar_s1p5_t3.db-shm");
}

// ---------------------------------------------------------------------------
// T4. Benchmark: insert throughput is recorded
// ---------------------------------------------------------------------------
void testBenchmarkInsert(Harness& h) {
    h.section("T4. Benchmark: insert throughput is recorded");

    const int N = 100;
    const int M = 1;

    p::Database db;
    if (!openFreshDb(db, "lodestar_s1p5_t4.db")) {
        h.check(false, "open fresh db + run migrations");
        return;
    }

    // Time inserting N requirements + N test cases + N verifies links.
    auto res = timeIt(M, [&]() {
        tl::TraceGraph g(db);
        (void)seedGraph(g, N);
    });

    std::printf("  insert_throughput: N=%d M=%d min=%lldus avg=%lldus max=%lldus\n",
                N, M, res.minUs, res.avgUs, res.maxUs);
    h.check(res.minUs >= 0 && res.avgUs >= 0 && res.maxUs >= 0,
            "insert benchmark produced non-negative timings");

    bool recorded = appendReport("insert_throughput", N, M, res);
    h.check(recorded, "appended insert_throughput section to benchmark report");

    db.close();
    std::remove("lodestar_s1p5_t4.db");
    std::remove("lodestar_s1p5_t4.db-wal");
    std::remove("lodestar_s1p5_t4.db-shm");
}

// ---------------------------------------------------------------------------
// T5. Report file is well-formed
// ---------------------------------------------------------------------------
void testReportWellFormed(Harness& h) {
    h.section("T5. Report file is well-formed");

    std::ifstream in(kReportPath);
    h.check(in.good(), "benchmark report file exists and is readable");
    if (!in.good()) return;

    std::stringstream ss;
    ss << in.rdbuf();
    std::string content = ss.str();

    h.check(content.find("## S1 Phase 5 Benchmark") != std::string::npos,
            "report contains a '##' benchmark section");
    h.check(content.find("graph_query") != std::string::npos,
            "report contains the graph_query operation name");
    h.check(content.find("insert_throughput") != std::string::npos,
            "report contains the insert_throughput operation name");
    h.check(content.find("min_us:") != std::string::npos &&
                content.find("avg_us:") != std::string::npos &&
                content.find("max_us:") != std::string::npos,
            "report contains min_us/avg_us/max_us numeric fields");
}

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc > 1) {
        g_migrationsDir = argv[1];
    }

    Harness h("S1 Phase 5 real-time / determinism validation");
    std::printf("S1 PHASE 5 REAL-TIME / DETERMINISM TESTS (migrations: %s)\n",
                g_migrationsDir.c_str());

    testDeterminismSameInput(h);
    testDeterminismRepeated(h);
    testBenchmarkQuery(h);
    testBenchmarkInsert(h);
    testReportWellFormed(h);

    std::printf("\n%s: %d failure(s)\n", h.name(), h.failures());
    return h.failures() == 0 ? 0 : 1;
}
