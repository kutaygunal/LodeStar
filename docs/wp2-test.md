# WP-2 Test Contract — Review / comment / approval

> Written by the scrum-master BEFORE the WP-2 engineer implements the feature.
> The engineer must implement the contract below so the test file compiles and
> passes. Do NOT weaken the assertions to make them pass; implement the feature
> to satisfy them. This is a TEST CONTRACT, not a testing task.

## Test file
- **File:** `core/test/wp2_review_tests.cpp`
- **CMake target:** `lodestar_wp2_review_tests`
- **Links:** `lodestar_common`, `lodestar_persistence`, `lodestar_tracelink`
- **Defines:** `LODESTAR_MIGRATIONS_DIR`
- **Binary:** `./build/core/Release/lodestar_wp2_review_tests.exe`
- **Harness:** the same lightweight self-contained harness as WP-1..WP-8 / WP-A..WP-G
  (each DB-dependent test opens its own fresh throwaway DB, runs migrations, prints
  `[PASS]`/`[FAIL]`, returns exit 0 iff zero failures).

## Contract the WP-2 engineer must provide

### (A) Migration 014
`core/persistence/migrations/014_*.sql` creates `comments` and `reviews` tables so
any artifact can carry general review comments and an approval verdict (beyond the
change-request workflow). Append-only and idempotent (`IF NOT EXISTS`). Suggested:

```sql
CREATE TABLE IF NOT EXISTS comments (
    id          TEXT PRIMARY KEY,             -- UUID
    entity_type TEXT NOT NULL,
    entity_id   TEXT NOT NULL,
    author      TEXT NOT NULL DEFAULT '',
    body        TEXT NOT NULL DEFAULT '',
    created_at  TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_comments_entity ON comments(entity_type, entity_id);

CREATE TABLE IF NOT EXISTS reviews (
    id          TEXT PRIMARY KEY,             -- UUID
    entity_type TEXT NOT NULL,
    entity_id   TEXT NOT NULL,
    reviewer    TEXT NOT NULL DEFAULT '',
    verdict     TEXT NOT NULL DEFAULT '',     -- Approve|Reject|RequestChanges
    comment     TEXT NOT NULL DEFAULT '',
    created_at  TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_reviews_entity ON reviews(entity_type, entity_id);
```

### (B) `ReviewService` (new, `core/tracelink/ReviewService.h`)
```cpp
struct Comment {
    std::string id;
    std::string entityType;
    std::string entityId;
    std::string author;
    std::string body;
    std::string createdAt;
};

struct Review {
    std::string id;
    std::string entityType;
    std::string entityId;
    std::string reviewer;
    std::string verdict;   // Approve | Reject | RequestChanges
    std::string comment;
    std::string createdAt;
};

class ReviewService {
public:
    explicit ReviewService(persistence::Database& db);

    // Adds a comment to an artifact. Assigns a UUID if id is empty.
    common::Result<Comment> addComment(const std::string& entityType,
                                       const std::string& entityId,
                                       const std::string& author,
                                       const std::string& body);

    // All comments for an artifact, oldest first.
    common::Result<std::vector<Comment>> commentsFor(
        const std::string& entityType, const std::string& entityId);

    // Submits a review verdict for an artifact.
    common::Result<Review> submitReview(const std::string& entityType,
                                        const std::string& entityId,
                                        const std::string& reviewer,
                                        const std::string& verdict,
                                        const std::string& comment);

    // All reviews for an artifact, newest first.
    common::Result<std::vector<Review>> reviewsFor(
        const std::string& entityType, const std::string& entityId);

    // Current approval status: "Approved" if the latest review is Approve,
    // "Rejected" if Reject, "RequestChanges" if RequestChanges, else "None".
    common::Result<std::string> approvalStatus(
        const std::string& entityType, const std::string& entityId);
};
```

## Test cases & expected behavior

### T1. Migration 014 applies
- Open a fresh DB and run migrations.
- **Expect:** migration succeeds; `comments` and `reviews` tables exist.

### T2. addComment + commentsFor roundtrip
- Add two comments to requirement R.
- **Expect:** `commentsFor` returns both, oldest first, with author/body preserved.

### T3. submitReview Approve -> approvalStatus Approved
- `submitReview("requirement", rId, "alice", "Approve", "ok")`.
- **Expect:** `approvalStatus` returns `"Approved"`; `reviewsFor` returns the review.

### T4. submitReview Reject -> approvalStatus Rejected
- On a fresh artifact, submit `"Reject"`.
- **Expect:** `approvalStatus` returns `"Rejected"`.

### T5. Latest verdict wins
- Submit `"RequestChanges"` then `"Approve"` on the same artifact.
- **Expect:** `approvalStatus` returns `"Approved"` (latest review governs).

### T6. Per-entity isolation
- Comment/review artifact A only.
- **Expect:** `commentsFor`/`reviewsFor` for artifact B are empty; `approvalStatus`
  for B is `"None"`.

## CMake registration
Add inside the `if(LODESTAR_BUILD_TESTS)` block of `core/CMakeLists.txt`:

```cmake
add_executable(lodestar_wp2_review_tests
    test/wp2_review_tests.cpp)
target_link_libraries(lodestar_wp2_review_tests PRIVATE
    lodestar_common
    lodestar_persistence
    lodestar_tracelink)
target_compile_definitions(lodestar_wp2_review_tests PRIVATE
    LODESTAR_MIGRATIONS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/persistence/migrations")
```

> Note: the target is named `lodestar_wp2_review_tests` (not `lodestar_wp2_tests`)
> to avoid clobbering the existing Phase-1 `lodestar_wp2_tests` regression target.
