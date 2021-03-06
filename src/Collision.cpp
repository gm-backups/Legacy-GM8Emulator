#include "AssetManager.hpp"
#include "Collision.hpp"
#include "Instance.hpp"
#include "InstanceList.hpp"
#include <cmath>

#define PI 3.141592653589793

int dRound(double d) {
    // This mimics the x86_32 "FISTP" operator which is commonly used in the GM8 runner.
    // We can't actually use that operator, because we're targeting other platforms than x86 Windows.
    int down = ( int )d;
    if ((d - down) < 0.5) return down;
    if ((d - down) > 0.5) return (down + 1);
    return down + (down & 1);
}

// cX and cY is the center to rotate around. pX and pY is the point to rotate. s and c are the sin and cos, respectively, of the angle to rotate at.
void rotateAround(double* pX, double* pY, double cX, double cY, double s, double c) {
    (*pX) -= cX;
    (*pY) -= cY;
    double xnew = (*pX) * c - (*pY) * s;
    double ynew = (*pX) * s + (*pY) * c;
    (*pX) = xnew + cX;
    (*pY) = ynew + cY;
}

void RefreshInstanceBbox(Instance* i) {
    if (i->bboxIsStale) {
        int spriteIndex = i->mask_index;
        if (spriteIndex == -1) spriteIndex = i->sprite_index;
        if (spriteIndex == -1) {
            i->bbox_bottom = -100000;
            i->bbox_top = -100000;
            i->bbox_left = -100000;
            i->bbox_right = -100000;
        }
        else {
            Sprite* s = AssetManager::GetSprite(spriteIndex);
            CollisionMap* map = (s->separateCollision ? (s->collisionMaps + (( int )(i->image_index) % s->frameCount)) : s->collisionMaps);

            double tlX = (i->x - (s->originX * i->image_xscale)) + (static_cast<int>(map->left) * i->image_xscale);
            double tlY = (i->y - (s->originY * i->image_yscale)) + (static_cast<int>(map->top) * i->image_yscale);
            double brX = tlX + ((static_cast<int>(map->right) + 1 - static_cast<int>(map->left)) * i->image_xscale) - 1;
            double brY = tlY + ((static_cast<int>(map->bottom) + 1 - static_cast<int>(map->top)) * i->image_yscale) - 1;

            double tmp;
            if (i->image_xscale <= 0) {
                // swap left x and right x
                tmp = tlX;
                tlX = brX;
                brX = tmp;
            }
            if (i->image_yscale <= 0) {
                // swap top y and bottom y
                tmp = tlY;
                tlY = brY;
                brY = tmp;
            }

            if (i->image_angle) {
                double trX = brX;
                double trY = tlY;
                double blX = tlX;
                double blY = brY;
                double angle = (-i->image_angle) * PI / 180.0;
                double s = sin(angle);
                double c = cos(angle);

                rotateAround(&tlX, &tlY, i->x, i->y, s, c);
                rotateAround(&trX, &trY, i->x, i->y, s, c);
                rotateAround(&blX, &blY, i->x, i->y, s, c);
                rotateAround(&brX, &brY, i->x, i->y, s, c);

                i->bbox_left = dRound(fmin(tlX, fmin(trX, fmin(blX, brX))));
                i->bbox_right = dRound(fmax(tlX, fmax(trX, fmax(blX, brX))));
                i->bbox_top = dRound(fmin(tlY, fmin(trY, fmin(blY, brY))));
                i->bbox_bottom = dRound(fmax(tlY, fmax(trY, fmax(blY, brY))));
            }
            else {
                i->bbox_left = dRound(tlX);
                i->bbox_right = dRound(brX);
                i->bbox_bottom = dRound(brY);
                i->bbox_top = dRound(tlY);
            }
        }

        i->bboxIsStale = false;
    }
}

bool CollisionCheck(Instance* i1, Instance* i2) {
    RefreshInstanceBbox(i1);
    RefreshInstanceBbox(i2);
    if (i1->bbox_right < i2->bbox_left) return false;
    if (i2->bbox_right < i1->bbox_left) return false;
    if (i1->bbox_bottom < i2->bbox_top) return false;
    if (i2->bbox_bottom < i1->bbox_top) return false;

    int cTop = (i1->bbox_top > i2->bbox_top ? i1->bbox_top : i2->bbox_top);
    int cBottom = (i1->bbox_bottom > i2->bbox_bottom ? i2->bbox_bottom : i1->bbox_bottom);
    int cLeft = (i1->bbox_left > i2->bbox_left ? i1->bbox_left : i2->bbox_left);
    int cRight = (i1->bbox_right > i2->bbox_right ? i2->bbox_right : i1->bbox_right);

    int spriteIndex = i1->mask_index;
    if (spriteIndex == -1) spriteIndex = i1->sprite_index;
    if(spriteIndex < 0) return false;
    Sprite* spr1 = AssetManager::GetSprite(spriteIndex);
    if(!spr1->exists) return false;
    CollisionMap* map1 = (spr1->separateCollision ? (spr1->collisionMaps + (static_cast<int>(i1->image_index) % spr1->frameCount)) : spr1->collisionMaps);
    spriteIndex = i2->mask_index;
    if (spriteIndex == -1) spriteIndex = i2->sprite_index;
    if (spriteIndex < 0) return false;
    Sprite* spr2 = AssetManager::GetSprite(spriteIndex);
    if(!spr2->exists) return false;
    CollisionMap* map2 = (spr2->separateCollision ? (spr2->collisionMaps + (static_cast<int>(i2->image_index) % spr2->frameCount)) : spr2->collisionMaps);

    int x1 = dRound(i1->x);
    int y1 = dRound(i1->y);
    int x2 = dRound(i2->x);
    int y2 = dRound(i2->y);

    double a1 = i1->image_angle * PI / 180.0;
    double a2 = i2->image_angle * PI / 180.0;
    double c1 = cos(a1);
    double s1 = sin(a1);
    double c2 = cos(a2);
    double s2 = sin(a2);
    int w1 = map1->width;
    int w2 = map2->width;

    for (int y = cTop; y <= cBottom; y++) {
        for (int x = cLeft; x <= cRight; x++) {
            double curX = static_cast<double>(x);
            double curY = static_cast<double>(y);
            rotateAround(&curX, &curY, x1, y1, s1, c1);
            curX = spr1->originX + ((curX - x1) / i1->image_xscale);
            curY = spr1->originY + ((curY - y1) / i1->image_yscale);
            int nx = static_cast<int>(curX);
            int ny = static_cast<int>(curY);
            if (nx >= static_cast<int>(map1->left) && nx <= static_cast<int>(map1->right) && ny >= static_cast<int>(map1->top) && ny <= static_cast<int>(map1->bottom)) {
                if (map1->collision[ny * w1 + nx]) {
                    double curX = static_cast<double>(x);
                    double curY = static_cast<double>(y);
                    rotateAround(&curX, &curY, x2, y2, s2, c2);
                    curX = spr2->originX + ((curX - x2) / i2->image_xscale);
                    curY = spr2->originY + ((curY - y2) / i2->image_yscale);
                    nx = static_cast<int>(curX);
                    ny = static_cast<int>(curY);
                    if (nx >= static_cast<int>(map2->left) && nx <= static_cast<int>(map2->right) && ny >= static_cast<int>(map2->top) && ny <= static_cast<int>(map2->bottom)) {
                        if (map2->collision[ny * w2 + nx]) {
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

bool CollisionPointCheck(Instance* i1, int x, int y) {
    RefreshInstanceBbox(i1);
    if (i1->bbox_right < x) return false;
    if (x < i1->bbox_left) return false;
    if (i1->bbox_bottom < y) return false;
    if (y < i1->bbox_top) return false;

    int spriteIndex = i1->mask_index;
    if (spriteIndex == -1) spriteIndex = i1->sprite_index;
    if (spriteIndex < 0) return false;
    Sprite* spr1 = AssetManager::GetSprite(spriteIndex);
    if (!spr1->exists) return false;
    CollisionMap* map1 = (spr1->separateCollision ? (spr1->collisionMaps + (static_cast<int>(i1->image_index) % spr1->frameCount)) : spr1->collisionMaps);
    double a1 = i1->image_angle * PI / 180.0;
    double s1 = sin(a1);
    double c1 = cos(a1);
    int w1 = map1->width;

    double curX = static_cast<double>(x);
    double curY = static_cast<double>(y);
    rotateAround(&curX, &curY, i1->x, i1->y, s1, c1);
    curX = spr1->originX + ((curX - i1->x) / i1->image_xscale);
    curY = spr1->originY + ((curY - i1->y) / i1->image_yscale);
    int nx = dRound(curX);
    int ny = dRound(curY);

    if (nx >= static_cast<int>(map1->left) && nx <= static_cast<int>(map1->right) && ny >= static_cast<int>(map1->top) && ny <= static_cast<int>(map1->bottom)) {
        if (map1->collision[ny * w1 + nx]) {
            return true;
        }
    }
    return false;
}

bool CollisionRectangleCheck(Instance* i1, int x1, int y1, int x2, int y2, bool pixelPerfect) {
    RefreshInstanceBbox(i1);
    if (i1->bbox_right < x1) return false;
    if (x2 < i1->bbox_left) return false;
    if (i1->bbox_bottom < y1) return false;
    if (y2 < i1->bbox_top) return false;

    if(!pixelPerfect) return true;

    int spriteIndex = i1->mask_index;
    if (spriteIndex == -1) spriteIndex = i1->sprite_index;
    if (spriteIndex < 0) return false;
    Sprite* spr1 = AssetManager::GetSprite(spriteIndex);
    if (!spr1->exists) return false;
    CollisionMap* map1 = (spr1->separateCollision ? (spr1->collisionMaps + (static_cast<int>(i1->image_index) % spr1->frameCount)) : spr1->collisionMaps);
    double a1 = i1->image_angle * PI / 180.0;
    double s1 = sin(a1);
    double c1 = cos(a1);
    int w1 = map1->width;

    int cTop = (i1->bbox_top > y1 ? i1->bbox_top : y1);
    int cBottom = (i1->bbox_bottom > y2 ? y2 : i1->bbox_bottom);
    int cLeft = (i1->bbox_left > x1 ? i1->bbox_left : x1);
    int cRight = (i1->bbox_right > x2 ? x2 : i1->bbox_right);

    for (int y = cTop; y <= cBottom; y++) {
        for (int x = cLeft; x <= cRight; x++) {
            double curX = static_cast<double>(x);
            double curY = static_cast<double>(y);
            rotateAround(&curX, &curY, dRound(i1->x), dRound(i1->y), s1, c1);
            curX = spr1->originX + ((curX - dRound(i1->x)) / i1->image_xscale);
            curY = spr1->originY + ((curY - dRound(i1->y)) / i1->image_yscale);
            int nx = static_cast<int>(curX);
            int ny = static_cast<int>(curY);
            if (nx >= static_cast<int>(map1->left) && nx <= static_cast<int>(map1->right) && ny >= static_cast<int>(map1->top) && ny <= static_cast<int>(map1->bottom)) {
                if (map1->collision[ny * w1 + nx]) {
                    return true;
                }
            }
        }
    }

    return false;
}
