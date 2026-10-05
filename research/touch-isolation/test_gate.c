#include <assert.h>
#include <stdio.h>
#include "touch_gate.h"
#include "touch_tap.h"
int main(void) {
    struct touch_gate g = {0};
    assert(touch_gate_next(&g, 0, 1) == TOUCH_WAIT);
    g.wanted = 1;
    assert(touch_gate_next(&g, 1, 1) == TOUCH_WAIT); /* game drag */
    assert(touch_gate_next(&g, 2, 1) == TOUCH_WAIT); /* multitouch */
    assert(touch_gate_next(&g, 0, 0) == TOUCH_WAIT); /* dropped events */
    assert(touch_gate_next(&g, -1, 1) == TOUCH_WAIT); /* ioctl failure */
    assert(touch_gate_next(&g, 0, 1) == TOUCH_GRAB);
    touch_gate_ack(&g, TOUCH_GRAB, 0);
    assert(!g.owned); /* EBUSY must not enable interactive UI */
    touch_gate_ack(&g, TOUCH_GRAB, 1);
    assert(g.owned && touch_gate_next(&g, 0, 1) == TOUCH_WAIT);
    g.wanted = 0;
    assert(touch_gate_next(&g, 1, 1) == TOUCH_WAIT); /* panel drag */
    assert(touch_gate_next(&g, 0, 0) == TOUCH_WAIT);
    assert(touch_gate_next(&g, 0, 1) == TOUCH_RELEASE);
    touch_gate_ack(&g, TOUCH_RELEASE, 0);
    assert(g.owned);
    touch_gate_ack(&g, TOUCH_RELEASE, 1);
    assert(!g.owned);
    g.wanted = 1;
    g.wanted = 0; /* show cancelled while game finger still down */
    assert(touch_gate_next(&g, 0, 1) == TOUCH_WAIT);
    puts("PASS: idle, held/multitouch, loss/unknown, failed ioctls, show/hide/cancel");
    struct touch_tap t = {0};
    assert(!touch_tap_frame(&t,1,1,1,20,20,0));
    assert(touch_tap_frame(&t,1,1,0,20,20,100));
    assert(!touch_tap_frame(&t,1,1,0,20,20,101)); /* no duplicate */
    assert(!touch_tap_frame(&t,1,1,1,20,20,200));
    assert(!touch_tap_frame(&t,1,1,1,40,20,250)); /* drag */
    assert(!touch_tap_frame(&t,1,1,0,20,20,300));
    assert(!touch_tap_frame(&t,1,1,1,288,20,400)); /* half-open edge */
    assert(!touch_tap_frame(&t,1,1,1,20,20,410)); /* outside then inside */
    assert(!touch_tap_frame(&t,1,1,0,20,20,500));
    assert(!touch_tap_frame(&t,1,1,1,20,20,600));
    assert(!touch_tap_frame(&t,1,1,2,20,20,610));
    assert(!touch_tap_frame(&t,1,1,1,20,20,620));
    assert(!touch_tap_frame(&t,1,1,0,20,20,650));
    assert(!touch_tap_frame(&t,1,1,1,20,20,700));
    assert(!touch_tap_frame(&t,1,0,0,20,20,710)); /* overflow */
    assert(!touch_tap_frame(&t,1,1,0,20,20,750));
    assert(!touch_tap_frame(&t,1,1,1,20,20,800));
    assert(!touch_tap_frame(&t,1,1,0,20,20,1500)); /* long press */
    assert(!touch_tap_frame(&t,0,1,1,20,20,1600));
    assert(!touch_tap_frame(&t,1,1,1,20,20,1610)); /* inherited contact */
    assert(!touch_tap_frame(&t,1,1,0,20,20,1650));
    assert(!touch_tap_frame(&t,1,1,1,20,20,1700));
    assert(touch_tap_frame(&t,1,1,0,20,20,1800));
    assert(!touch_tap_frame_rect(&t,1,1,1,500,380,1900,408,368,616,404));
    assert(touch_tap_frame_rect(&t,1,1,0,500,380,2000,408,368,616,404));
    assert(!touch_tap_frame_rect(&t,1,1,1,616,380,2100,408,368,616,404));
    assert(!touch_tap_frame_rect(&t,1,1,0,616,380,2200,408,368,616,404));
    puts("PASS: tap, release-only, drag, edges, multitouch, overflow, hold, inherited contact");
}
