/* Portable transition policy; ioctl success is acknowledged by the caller.
 * All contacts, not just the first finger, must be up before changing owner. */
#ifndef TOUCH_GATE_H
#define TOUCH_GATE_H
enum touch_action { TOUCH_WAIT, TOUCH_GRAB, TOUCH_RELEASE };
struct touch_gate { int wanted, owned; };
static inline enum touch_action touch_gate_next(struct touch_gate *g,
                                                int contacts, int synced) {
    if (!synced || contacts != 0 || g->wanted == g->owned) return TOUCH_WAIT;
    return g->wanted ? TOUCH_GRAB : TOUCH_RELEASE;
}
static inline void touch_gate_ack(struct touch_gate *g, enum touch_action a,
                                 int success) {
    if (success && a != TOUCH_WAIT) g->owned = a == TOUCH_GRAB;
}
#endif
