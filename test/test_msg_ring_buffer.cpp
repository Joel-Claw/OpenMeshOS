// OpenMeshOS — MsgRingBuffer host-side unit tests
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Tests the message ring buffer: push, query by channel,
// overflow behaviour, and sequence numbering.
//
// Since the real MsgRingBuffer uses PSRAM (ESP32-specific),
// we test a simplified host version that uses regular malloc.
// The logic is identical to the firmware version.
//
// Compile: g++ -std=c++14 -Wall -Wextra -I../src -o test_msg_ring_buffer test/test_msg_ring_buffer.cpp -lm
// Run:     ./test_msg_ring_buffer

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

// ── Mock types (mirror src/mesh/MessageBus.h) ──────────────────────
static constexpr size_t MSG_MAX_LEN = 251;

enum class MsgKind : uint8_t {
    GroupChannel,
    DirectMessage,
    SystemInfo,
};

struct InboxMessage {
    MsgKind  kind;
    uint8_t  channel_id;
    char     sender[9];
    char     text[MSG_MAX_LEN + 1];
    uint32_t timestamp;
    int      rssi;
};

// ── Host MsgRingBuffer (mirrors src/mesh/MsgRingBuffer.h) ──────────
// Uses calloc instead of heap_caps_malloc for host testing.

static constexpr size_t RING_CAPACITY = 1000;

struct RingEntry {
    InboxMessage msg;
    char    display[MSG_MAX_LEN + 32];
    uint32_t seq;
    bool    valid;
};

class TestMsgRingBuffer {
public:
    static TestMsgRingBuffer& instance() {
        static TestMsgRingBuffer s_ring;
        return s_ring;
    }

    void push(const InboxMessage& msg) {
        uint32_t secs = msg.timestamp / 1000;
        uint32_t mins = (secs / 60) % 60;
        uint32_t hrs  = (secs / 3600) % 24;
        char timeBuf[8];
        snprintf(timeBuf, sizeof(timeBuf), "%02lu:%02lu", (unsigned long)hrs, (unsigned long)mins);

        const char* prefix = (msg.kind == MsgKind::DirectMessage) ? "[DM]" : "[CH]";

        _seq++;
        size_t slot = _seq % RING_CAPACITY;
        _buf[slot].msg = msg;
        _buf[slot].seq = _seq;
        _buf[slot].valid = true;
        snprintf(_buf[slot].display, sizeof(RingEntry::display),
                 "%s %s %s: %s", timeBuf, prefix, msg.sender, msg.text);

        if (_count < RING_CAPACITY) {
            _count++;
        }
    }

    size_t queryByChannel(uint8_t channel_id, size_t offset, size_t limit,
                          const char** outTexts, size_t outMax) const {
        if (_count == 0 || outMax == 0) return 0;

        size_t written = 0;
        size_t skipped = 0;

        for (size_t i = 0; i < _count && written < outMax; i++) {
            size_t slot;
            if (_seq >= RING_CAPACITY) {
                slot = (_seq - i) % RING_CAPACITY;
            } else {
                if (i > _seq) break;
                slot = (_seq - i) % RING_CAPACITY;
            }

            if (!_buf[slot].valid) continue;

            bool match = false;
            if (channel_id == 255) {
                match = (_buf[slot].msg.kind == MsgKind::DirectMessage);
            } else if (channel_id == 0) {
                match = (_buf[slot].msg.kind == MsgKind::GroupChannel &&
                         _buf[slot].msg.channel_id == 0) ||
                        (_buf[slot].msg.kind == MsgKind::SystemInfo);
            } else {
                match = (_buf[slot].msg.kind == MsgKind::GroupChannel &&
                         _buf[slot].msg.channel_id == channel_id);
            }

            if (!match) continue;

            if (skipped < offset) {
                skipped++;
                continue;
            }

            outTexts[written] = _buf[slot].display;
            written++;
            if (written >= limit) break;
        }

        return written;
    }

    size_t countByChannel(uint8_t channel_id) const {
        if (_count == 0) return 0;
        size_t n = 0;
        for (size_t i = 0; i < _count; i++) {
            size_t slot;
            if (_seq >= RING_CAPACITY) {
                slot = (_seq - i) % RING_CAPACITY;
            } else {
                if (i > _seq) break;
                slot = (_seq - i) % RING_CAPACITY;
            }
            if (!_buf[slot].valid) continue;

            if (channel_id == 255) {
                if (_buf[slot].msg.kind == MsgKind::DirectMessage) n++;
            } else if (channel_id == 0) {
                if ((_buf[slot].msg.kind == MsgKind::GroupChannel &&
                     _buf[slot].msg.channel_id == 0) ||
                    (_buf[slot].msg.kind == MsgKind::SystemInfo)) n++;
            } else {
                if (_buf[slot].msg.kind == MsgKind::GroupChannel &&
                    _buf[slot].msg.channel_id == channel_id) n++;
            }
        }
        return n;
    }

    size_t total() const { return _count; }
    uint32_t seqNum() const { return _seq; }

    // Test helpers
    void reset() {
        memset(_buf, 0, RING_CAPACITY * sizeof(RingEntry));
        _count = 0;
        _seq = 0;
    }

private:
    TestMsgRingBuffer() : _count(0), _seq(0) {
        _buf = (RingEntry*)calloc(RING_CAPACITY, sizeof(RingEntry));
    }
    ~TestMsgRingBuffer() { free(_buf); }

    RingEntry*  _buf;
    size_t      _count;
    uint32_t    _seq;
};

// ── Test framework ──────────────────────────────────────────────────
static size_t tests_run = 0;
static size_t tests_passed = 0;

#define ASSERT_TRUE(expr) do { \
    tests_run++; \
    if (expr) { tests_passed++; } \
    else { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    tests_run++; \
    if ((a) == (b)) { tests_passed++; } \
    else { printf("FAIL %s:%d: %s != %s (got %lld vs %lld)\n", \
                   __FILE__, __LINE__, #a, #b, (long long)(a), (long long)(b)); } \
} while(0)

#define ASSERT_STREQ(a, b) do { \
    tests_run++; \
    if (strcmp((a), (b)) == 0) { tests_passed++; } \
    else { printf("FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); } \
} while(0)

// ── Helper: make a test message ──────────────────────────────────────
static InboxMessage makeMsg(MsgKind kind, uint8_t ch, const char* sender,
                            const char* text, uint32_t ts) {
    InboxMessage msg = {};
    msg.kind = kind;
    msg.channel_id = ch;
    strncpy(msg.sender, sender, sizeof(msg.sender) - 1);
    strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.timestamp = ts;
    msg.rssi = -50;
    return msg;
}

// ── Tests ────────────────────────────────────────────────────────────

void test_push_and_query_public() {
    printf("  test_push_and_query_public... ");
    TestMsgRingBuffer& ring = TestMsgRingBuffer::instance();
    ring.reset();

    ring.push(makeMsg(MsgKind::GroupChannel, 0, "Alice", "Hello", 3600000)); // 01:00
    ring.push(makeMsg(MsgKind::GroupChannel, 0, "Bob", "World", 3660000));    // 01:10

    ASSERT_EQ((long long)ring.total(), 2LL);
    ASSERT_EQ((long long)ring.countByChannel(0), 2LL);

    const char* texts[10] = {};
    size_t n = ring.queryByChannel(0, 0, 10, texts, 10);
    ASSERT_EQ((long long)n, 2LL);
    // Newest first (Bob's message, seq 2)
    ASSERT_TRUE(strstr(texts[0], "Bob") != nullptr);
    ASSERT_TRUE(strstr(texts[1], "Alice") != nullptr);

    printf("OK\n");
}

void test_push_dm_and_public_separate_channels() {
    printf("  test_push_dm_and_public_separate_channels... ");
    TestMsgRingBuffer& ring = TestMsgRingBuffer::instance();
    ring.reset();

    ring.push(makeMsg(MsgKind::GroupChannel, 0, "Alice", "Public msg", 3600000));
    ring.push(makeMsg(MsgKind::DirectMessage, 255, "Bob", "Private msg", 3660000));
    ring.push(makeMsg(MsgKind::GroupChannel, 0, "Charlie", "Another public", 3720000));

    ASSERT_EQ((long long)ring.total(), 3LL);
    ASSERT_EQ((long long)ring.countByChannel(0), 2LL);    // 2 public channel
    ASSERT_EQ((long long)ring.countByChannel(255), 1LL);  // 1 DM

    // Query DMs only
    const char* dmTexts[10] = {};
    size_t n = ring.queryByChannel(255, 0, 10, dmTexts, 10);
    ASSERT_EQ((long long)n, 1LL);
    ASSERT_TRUE(strstr(dmTexts[0], "Bob") != nullptr);

    // Query public only
    const char* pubTexts[10] = {};
    n = ring.queryByChannel(0, 0, 10, pubTexts, 10);
    ASSERT_EQ((long long)n, 2LL);

    printf("OK\n");
}

void test_offset_and_limit() {
    printf("  test_offset_and_limit... ");
    TestMsgRingBuffer& ring = TestMsgRingBuffer::instance();
    ring.reset();

    // Push 5 messages
    for (int i = 0; i < 5; i++) {
        char text[32];
        snprintf(text, sizeof(text), "Msg %d", i);
        ring.push(makeMsg(MsgKind::GroupChannel, 0, "T", text, 3600000 + i * 60000));
    }

    ASSERT_EQ((long long)ring.total(), 5LL);

    // Get latest 2 messages (offset=0, limit=2)
    const char* texts[10] = {};
    size_t n = ring.queryByChannel(0, 0, 2, texts, 10);
    ASSERT_EQ((long long)n, 2LL);
    // Should be newest first: "Msg 4" then "Msg 3"
    ASSERT_TRUE(strstr(texts[0], "Msg 4") != nullptr);
    ASSERT_TRUE(strstr(texts[1], "Msg 3") != nullptr);

    // Skip 2, get next 2 (offset=2, limit=2)
    n = ring.queryByChannel(0, 2, 2, texts, 10);
    ASSERT_EQ((long long)n, 2LL);
    ASSERT_TRUE(strstr(texts[0], "Msg 2") != nullptr);
    ASSERT_TRUE(strstr(texts[1], "Msg 1") != nullptr);

    printf("OK\n");
}

void test_system_info_in_public_channel() {
    printf("  test_system_info_in_public_channel... ");
    TestMsgRingBuffer& ring = TestMsgRingBuffer::instance();
    ring.reset();

    ring.push(makeMsg(MsgKind::SystemInfo, 0, "SYS", "Node joined", 3600000));
    ring.push(makeMsg(MsgKind::GroupChannel, 0, "Alice", "Hello", 3660000));

    // SystemInfo should appear in channel 0 queries
    ASSERT_EQ((long long)ring.countByChannel(0), 2LL);

    const char* texts[10] = {};
    size_t n = ring.queryByChannel(0, 0, 10, texts, 10);
    ASSERT_EQ((long long)n, 2LL);
    // Newest first: Alice's message, then system info
    ASSERT_TRUE(strstr(texts[0], "Alice") != nullptr);
    ASSERT_TRUE(strstr(texts[1], "SYS") != nullptr);

    printf("OK\n");
}

void test_overflow_wraps_correctly() {
    printf("  test_overflow_wraps_correctly... ");
    TestMsgRingBuffer& ring = TestMsgRingBuffer::instance();
    ring.reset();

    // Push more than RING_CAPACITY messages
    for (uint32_t i = 0; i < RING_CAPACITY + 100; i++) {
        char text[32];
        snprintf(text, sizeof(text), "Msg %u", i);
        ring.push(makeMsg(MsgKind::GroupChannel, 0, "T", text, 3600000 + i));
    }

    // Should still only hold RING_CAPACITY messages
    ASSERT_EQ((long long)ring.total(), (long long)RING_CAPACITY);

    // The latest message should be findable
    ASSERT_EQ((long long)ring.seqNum(), (long long)(RING_CAPACITY + 100));

    // Count should reflect only the last RING_CAPACITY messages
    ASSERT_EQ((long long)ring.countByChannel(0), (long long)RING_CAPACITY);

    // The newest message should be accessible
    const char* texts[1] = {};
    size_t n = ring.queryByChannel(0, 0, 1, texts, 1);
    ASSERT_EQ((long long)n, 1LL);
    // Should be the last message: "Msg 1099"
    ASSERT_TRUE(strstr(texts[0], "Msg 1099") != nullptr);

    printf("OK\n");
}

void test_empty_ring_queries() {
    printf("  test_empty_ring_queries... ");
    TestMsgRingBuffer& ring = TestMsgRingBuffer::instance();
    ring.reset();

    ASSERT_EQ((long long)ring.total(), 0LL);
    ASSERT_EQ((long long)ring.countByChannel(0), 0LL);
    ASSERT_EQ((long long)ring.countByChannel(255), 0LL);

    const char* texts[10] = {};
    size_t n = ring.queryByChannel(0, 0, 10, texts, 10);
    ASSERT_EQ((long long)n, 0LL);

    printf("OK\n");
}

void test_display_format() {
    printf("  test_display_format... ");
    TestMsgRingBuffer& ring = TestMsgRingBuffer::instance();
    ring.reset();

    // timestamp 3600000ms = 1:00:00 => "01:00"
    ring.push(makeMsg(MsgKind::GroupChannel, 0, "Alice", "Hello", 3600000));

    const char* texts[1] = {};
    size_t n = ring.queryByChannel(0, 0, 1, texts, 1);
    ASSERT_EQ((long long)n, 1LL);

    // Should contain: "01:00 [CH] Alice: Hello"
    ASSERT_TRUE(strstr(texts[0], "01:00") != nullptr);
    ASSERT_TRUE(strstr(texts[0], "[CH]") != nullptr);
    ASSERT_TRUE(strstr(texts[0], "Alice") != nullptr);
    ASSERT_TRUE(strstr(texts[0], "Hello") != nullptr);

    // DM format: "[DM]"
    ring.reset();
    ring.push(makeMsg(MsgKind::DirectMessage, 255, "Bob", "Private", 3660000));

    n = ring.queryByChannel(255, 0, 1, texts, 1);
    ASSERT_EQ((long long)n, 1LL);
    ASSERT_TRUE(strstr(texts[0], "[DM]") != nullptr);

    printf("OK\n");
}

void test_different_channels() {
    printf("  test_different_channels... ");
    TestMsgRingBuffer& ring = TestMsgRingBuffer::instance();
    ring.reset();

    // Channel 0 (public), channel 1, channel 5
    ring.push(makeMsg(MsgKind::GroupChannel, 0, "A", "Public", 3600000));
    ring.push(makeMsg(MsgKind::GroupChannel, 1, "B", "Channel1", 3660000));
    ring.push(makeMsg(MsgKind::GroupChannel, 5, "C", "Channel5", 3720000));
    ring.push(makeMsg(MsgKind::GroupChannel, 0, "D", "Public2", 3780000));

    ASSERT_EQ((long long)ring.countByChannel(0), 2LL);
    ASSERT_EQ((long long)ring.countByChannel(1), 1LL);
    ASSERT_EQ((long long)ring.countByChannel(5), 1LL);

    const char* texts[10] = {};
    size_t n = ring.queryByChannel(1, 0, 10, texts, 10);
    ASSERT_EQ((long long)n, 1LL);
    ASSERT_TRUE(strstr(texts[0], "B") != nullptr);

    printf("OK\n");
}

void test_sequence_numbers_increase() {
    printf("  test_sequence_numbers_increase... ");
    TestMsgRingBuffer& ring = TestMsgRingBuffer::instance();
    ring.reset();

    ASSERT_EQ((long long)ring.seqNum(), 0LL);

    ring.push(makeMsg(MsgKind::GroupChannel, 0, "A", "1", 3600000));
    ASSERT_EQ((long long)ring.seqNum(), 1LL);

    ring.push(makeMsg(MsgKind::GroupChannel, 0, "B", "2", 3660000));
    ASSERT_EQ((long long)ring.seqNum(), 2LL);

    ring.push(makeMsg(MsgKind::GroupChannel, 0, "C", "3", 3720000));
    ASSERT_EQ((long long)ring.seqNum(), 3LL);

    printf("OK\n");
}

// ── Main ─────────────────────────────────────────────────────────────
int main() {
    printf("MsgRingBuffer unit tests\n");
    printf("==========================\n\n");

    test_push_and_query_public();
    test_push_dm_and_public_separate_channels();
    test_offset_and_limit();
    test_system_info_in_public_channel();
    test_overflow_wraps_correctly();
    test_empty_ring_queries();
    test_display_format();
    test_different_channels();
    test_sequence_numbers_increase();

    printf("\n");
    printf("Results: %zu/%zu passed\n", tests_passed, tests_run);

    if (tests_passed == tests_run) {
        printf("ALL PASSED ✅\n");
        return 0;
    } else {
        printf("SOME FAILED ❌\n");
        return 1;
    }
}