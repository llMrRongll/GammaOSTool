#ifndef REGION_POLICY_H
#define REGION_POLICY_H
/* 0 idle, 1 panel, 2 game, 3 cancelled until all fingers lift. */
static inline int region_owner(int owner,int contacts,int inside) {
    if(!contacts)return 0;
    if(!owner)owner=inside?1:2;
    if(contacts!=1||(owner==2&&inside))return 3;
    return owner;
}
#endif
