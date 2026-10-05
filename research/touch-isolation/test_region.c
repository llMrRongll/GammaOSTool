#include <assert.h>
#include "region_policy.h"
#include "touch_tap.h"
int main(void) {
    assert(region_owner(0,1,1)==1);
    assert(region_owner(0,1,0)==2);
    assert(region_owner(2,1,1)==3);
    assert(region_owner(3,1,0)==3);
    assert(region_owner(1,1,0)==1);
    assert(region_owner(1,2,1)==3);
    assert(region_owner(2,2,0)==3);
    assert(region_owner(3,0,0)==0);
    struct touch_tap tap={0};
    /* A game gesture must not block the next panel tap. */
    assert(!touch_tap_frame_rect(&tap,0,1,1,100,100,0,408,368,616,404));
    assert(!touch_tap_frame_rect(&tap,1,1,0,100,100,100,408,368,616,404));
    assert(!touch_tap_frame_rect(&tap,1,1,1,500,382,200,408,368,616,404));
    assert(touch_tap_frame_rect(&tap,1,1,0,500,382,300,408,368,616,404));
    return 0;
}
