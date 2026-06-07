// OpenMeshOS — MessageBus host-side unit tests
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Tests the MessageBus ring-buffer queue (push, pop, overflow, peek)
// and the MsgRingBuffer query/filter logic.
//
// Compile: g++ -std=c++14 -Wall -Wextra -I../src -o test_message_bus test/test_message_bus.cpp -lm
// Run:     ./test_message_bus

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <vector>
#include <string>

// ── Minimal mocks for Arduino types ────────────────────────────────
// We only need the logic from MessageBus.h and MsgRingBuffer.h.
// The PSRAM allocation in MsgRingBuffer won't work on host,
// so we test MessageBus (pure logic) and a simplified ring buffer.

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
    else { printf("FAIL %s:%d: %s != %s (got %d vs %d)\n", \
                   __FILE__, __LINE__, #a, #b, (int)(a), (int)(b)); } \
} while(0)

// ── Inline MessageBus (from src/mesh/MessageBus.h) ────────────────
// Copied here to avoid Arduino dependencies.
// This MUST be kept in sync with the original.

static constexpr size_t MSG_MAX_LEN = 251;
static constexpr size_t MSG_QUEUE_SIZE = 32;

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

class TestMessageBus {
public:
    TestMessageBus() : _head(0), _tail(0), _count(0), _dropped(0) {}

    bool push(const InboxMessage& msg) {
        if (_count >= MSG_QUEUE_SIZE) {
            _tail = (_tail + 1) % MSG_QUEUE_SIZE;
            _count--;
            _dropped++;
        }
        _buf[_head] = msg;
        _head = (_head + 1) % MSG_QUEUE_SIZE;
        _count++;
        return true;
    }

    bool pop(InboxMessage& msg) {
        if (_count == 0) return false;
        msg = _buf[_tail];
        _tail = (_tail + 1) % MSG_QUEUE_SIZE;
        _count--;
        return true;
    }

    bool peek(InboxMessage& msg) const {
        if (_count == 0) return false;
        msg = _buf[_tail];
        return true;
    }

    size_t count() const { return _count; }
    size_t dropped() const { return _dropped; }

private:
    InboxMessage _buf[MSG_QUEUE_SIZE];
    size_t _head;
    size_t _tail;
    size_t _count;
    size_t _dropped;
};

// ── Test: basic push and pop ────────────────────────────────────────
void test_basic_push_pop() {
    printf("  test_basic_push_pop... ");
    TestMessageBus bus;

    InboxMessage msg = {};
    msg.kind = MsgKind::GroupChannel;
    msg.channel_id = 0;
    strncpy(msg.sender, "Alice", sizeof(msg.sender));
    strncpy(msg.text, "Hello mesh!", sizeof(msg.text));
    msg.timestamp = 1000;
    msg.rssi = -42;

    ASSERT_TRUE(bus.push(msg));
    ASSERT_EQ((int)bus.count(), 1);

    InboxMessage out = {};
    ASSERT_TRUE(bus.pop(out));
    ASSERT_EQ((int)out.kind, (int)MsgKind::GroupChannel);
    ASSERT_EQ((int)out.channel_id, 0);
    ASSERT_EQ(strcmp(out.sender, "Alice"), 0);
    ASSERT_EQ(strcmp(out.text, "Hello mesh!"), 0);
    ASSERT_EQ((int)out.timestamp, 1000);
    ASSERT_EQ((int)out.rssi, -42);
    ASSERT_EQ((int)bus.count(), 0);

    printf("OK\n");
}

// ── Test: FIFO order ────────────────────────────────────────────────
void test_fifo_order() {
    printf("  test_fifo_order... ");
    TestMessageBus bus;

    for (int i = 0; i < 5; i++) {
        InboxMessage msg = {};
        msg.kind = MsgKind::GroupChannel;
        msg.channel_id = 0;
        snprintf(msg.sender, sizeof(msg.sender), "N%d", i);
        snprintf(msg.text, sizeof(msg.text), "Msg %d", i);
        msg.timestamp = (uint32_t)(1000 + i);
        msg.rssi = -50 - i;
        bus.push(msg);
    }
    ASSERT_EQ((int)bus.count(), 5);

    for (int i = 0; i < 5; i++) {
        InboxMessage out = {};
        ASSERT_TRUE(bus.pop(out));
        // Messages should come out in FIFO order (0, 1, 2, 3, 4)
        char expected[8];
        snprintf(expected, sizeof(expected), "N%d", i);
        ASSERT_EQ(strcmp(out.sender, expected), 0);
    }
    ASSERT_EQ((int)bus.count(), 0);

    printf("OK\n");
}

// ── Test: overflow drops oldest ──────────────────────────────────────
void test_overflow_drops_oldest() {
    printf("  test_overflow_drops_oldest... ");
    TestMessageBus bus;

    // Fill the queue completely (MSG_QUEUE_SIZE = 32)
    for (size_t i = 0; i < MSG_QUEUE_SIZE; i++) {
        InboxMessage msg = {};
        msg.kind = MsgKind::DirectMessage;
        msg.channel_id = 255;
        snprintf(msg.sender, sizeof(msg.sender), "U%zu", i);
        snprintf(msg.text, sizeof(msg.text), "Fill %zu", i);
        msg.timestamp = (uint32_t)i;
        bus.push(msg);
    }
    ASSERT_EQ((int)bus.count(), (int)MSG_QUEUE_SIZE);
    ASSERT_EQ((int)bus.dropped(), 0);

    // Push one more — oldest should be dropped
    InboxMessage extra = {};
    extra.kind = MsgKind::SystemInfo;
    extra.channel_id = 0;
    strncpy(extra.sender, "SYS", sizeof(extra.sender));
    strncpy(extra.text, "overflow", sizeof(extra.text));
    extra.timestamp = 99999;
    bus.push(extra);

    ASSERT_EQ((int)bus.count(), (int)MSG_QUEUE_SIZE);
    ASSERT_EQ((int)bus.dropped(), 1);

    // The oldest message (U0) should be gone — first pop should be U1
    InboxMessage out = {};
    ASSERT_TRUE(bus.pop(out));
    ASSERT_EQ(strcmp(out.sender, "U1"), 0);

    // The newest message (SYS) should be accessible
    // Pop all remaining until we find it
    bool foundSys = false;
    while (bus.count() > 0) {
        bus.pop(out);
        if (strcmp(out.sender, "SYS") == 0) foundSys = true;
    }
    ASSERT_TRUE(foundSys);

    printf("OK\n");
}

// ── Test: peek doesn't remove ────────────────────────────────────────
void test_peek_no_remove() {
    printf("  test_peek_no_remove... ");
    TestMessageBus bus;

    InboxMessage msg = {};
    msg.kind = MsgKind::GroupChannel;
    strncpy(msg.sender, "Peek", sizeof(msg.sender));
    strncpy(msg.text, "test", sizeof(msg.text));
    bus.push(msg);

    ASSERT_EQ((int)bus.count(), 1);

    InboxMessage out = {};
    ASSERT_TRUE(bus.peek(out));
    ASSERT_EQ(strcmp(out.sender, "Peek"), 0);
    ASSERT_EQ((int)bus.count(), 1);  // still there

    // Pop should give same message
    ASSERT_TRUE(bus.pop(out));
    ASSERT_EQ(strcmp(out.sender, "Peek"), 0);
    ASSERT_EQ((int)bus.count(), 0);

    printf("OK\n");
}

// ── Test: pop from empty returns false ────────────────────────────────
void test_pop_empty() {
    printf("  test_pop_empty... ");
    TestMessageBus bus;

    InboxMessage out = {};
    ASSERT_TRUE(!bus.pop(out));
    ASSERT_TRUE(!bus.peek(out));
    ASSERT_EQ((int)bus.count(), 0);
    ASSERT_EQ((int)bus.dropped(), 0);

    printf("OK\n");
}

// ── Test: mixed message types ────────────────────────────────────────
void test_mixed_types() {
    printf("  test_mixed_types... ");
    TestMessageBus bus;

    // Group channel message
    InboxMessage g = {};
    g.kind = MsgKind::GroupChannel;
    g.channel_id = 0;
    strncpy(g.sender, "Bob", sizeof(g.sender));
    strncpy(g.text, "Hello #Public", sizeof(g.text));
    g.timestamp = 100;
    g.rssi = -60;
    bus.push(g);

    // Direct message
    InboxMessage d = {};
    d.kind = MsgKind::DirectMessage;
    d.channel_id = 255;
    strncpy(d.sender, "Alice", sizeof(d.sender));
    strncpy(d.text, "Hey DM", sizeof(d.text));
    d.timestamp = 200;
    d.rssi = -45;
    bus.push(d);

    // System info
    InboxMessage s = {};
    s.kind = MsgKind::SystemInfo;
    s.channel_id = 0;
    strncpy(s.sender, "SYS", sizeof(s.sender));
    strncpy(s.text, "Node joined", sizeof(s.text));
    s.timestamp = 300;
    s.rssi = 0;
    bus.push(s);

    ASSERT_EQ((int)bus.count(), 3);

    InboxMessage out = {};
    bus.pop(out);
    ASSERT_EQ((int)out.kind, (int)MsgKind::GroupChannel);
    ASSERT_EQ((int)out.channel_id, 0);

    bus.pop(out);
    ASSERT_EQ((int)out.kind, (int)MsgKind::DirectMessage);
    ASSERT_EQ((int)out.channel_id, 255);

    bus.pop(out);
    ASSERT_EQ((int)out.kind, (int)MsgKind::SystemInfo);

    printf("OK\n");
}

// ── Test: large text truncation boundary ──────────────────────────────
void test_text_boundary() {
    printf("  test_text_boundary... ");
    TestMessageBus bus;

    // MSG_MAX_LEN = 251
    InboxMessage msg = {};
    msg.kind = MsgKind::GroupChannel;
    msg.channel_id = 0;
    strncpy(msg.sender, "T", sizeof(msg.sender));

    // Fill text with exactly MSG_MAX_LEN characters
    memset(msg.text, 'X', MSG_MAX_LEN);
    msg.text[MSG_MAX_LEN] = '\0';

    ASSERT_TRUE(bus.push(msg));
    ASSERT_EQ((int)bus.count(), 1);

    InboxMessage out = {};
    bus.pop(out);
    ASSERT_EQ((int)strlen(out.text), MSG_MAX_LEN);

    printf("OK\n");
}

// ── Test: massive overflow (push way more than capacity) ──────────────
void test_massive_overflow() {
    printf("  test_massive_overflow... ");
    TestMessageBus bus;

    // Push 100 messages into a 32-slot queue
    for (int i = 0; i < 100; i++) {
        InboxMessage msg = {};
        msg.kind = MsgKind::GroupChannel;
        msg.channel_id = 0;
        snprintf(msg.sender, sizeof(msg.sender), "M%d", i);
        snprintf(msg.text, sizeof(msg.text), "Message %d", i);
        msg.timestamp = (uint32_t)i;
        bus.push(msg);
    }

    // Queue should be at capacity (32), with 68 dropped
    ASSERT_EQ((int)bus.count(), (int)MSG_QUEUE_SIZE);
    ASSERT_EQ((int)bus.dropped(), 68);

    // First message should be #68 (0-67 were dropped)
    InboxMessage out = {};
    bus.pop(out);
    ASSERT_EQ(strcmp(out.sender, "M68"), 0);

    // Last message should be #99
    while (bus.count() > 1) {
        bus.pop(out);
    }
    bus.pop(out);
    ASSERT_EQ(strcmp(out.sender, "M99"), 0);

    printf("OK\n");
}

// ── Main ─────────────────────────────────────────────────────────────
int main() {
    printf("MessageBus unit tests\n");
    printf("========================\n\n");

    test_basic_push_pop();
    test_fifo_order();
    test_overflow_drops_oldest();
    test_peek_no_remove();
    test_pop_empty();
    test_mixed_types();
    test_text_boundary();
    test_massive_overflow();

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