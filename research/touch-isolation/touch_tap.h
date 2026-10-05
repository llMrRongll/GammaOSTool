/* Single-finger tap recognition in normalized HUD pixels. The evdev adapter
 * must deliver complete SYN_REPORT frames with all contacts counted. */
#ifndef TOUCH_TAP_H
#define TOUCH_TAP_H
struct touch_tap { int armed, blocked, x, y; long long started; };
static inline void touch_tap_cancel(struct touch_tap *t) {
    t->armed = 0; t->blocked = 1;
}
static inline int touch_tap_frame_rect(struct touch_tap *t, int owner, int synced,
                                 int contacts, int x, int y, long long now,
                                 int left,int top,int right,int bottom) {
    if (!owner || !synced || contacts < 0) { touch_tap_cancel(t); return 0; }
    if (!contacts) {
        int hit = t->armed && !t->blocked && now >= t->started &&
                  now-t->started <= 600;
        t->armed = t->blocked = 0;
        return hit;
    }
    if (contacts != 1 || x < left || x >= right || y < top || y >= bottom) {
        touch_tap_cancel(t); return 0;
    }
    if (t->blocked) return 0;
    if (!t->armed) {
        t->armed = 1; t->x = x; t->y = y; t->started = now;
    } else if (x-t->x > 12 || t->x-x > 12 ||
               y-t->y > 12 || t->y-y > 12) touch_tap_cancel(t);
    return 0; /* trigger only on release */
}
static inline int touch_tap_frame(struct touch_tap *t,int owner,int synced,
                                 int contacts,int x,int y,long long now) {
    return touch_tap_frame_rect(t,owner,synced,contacts,x,y,now,8,8,288,96);
}
#endif
