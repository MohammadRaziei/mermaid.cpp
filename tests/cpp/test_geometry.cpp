#include <mermaid/geometry.h>
#include "utest.h"

using mermaid::Point;
using mermaid::Rect;
using mermaid::Size;

UTEST(geometry, point_equality_and_arithmetic) {
    ASSERT_TRUE((Point{1, 2} == Point{1, 2}));
    ASSERT_FALSE((Point{1, 2} == Point{1, 3}));
    Point p = Point{1, 2} + Point{3, 4};
    ASSERT_EQ(4.0, p.x);
    ASSERT_EQ(6.0, p.y);
    Point q = Point{5, 5} - Point{2, 1};
    ASSERT_EQ(3.0, q.x);
    ASSERT_EQ(4.0, q.y);
}

UTEST(geometry, distance) {
    ASSERT_NEAR(5.0, mermaid::distance(Point{0, 0}, Point{3, 4}), 1e-12);
}

UTEST(geometry, rect_edges_and_center) {
    Rect r{10, 20, 100, 40};
    ASSERT_EQ(110.0, r.right());
    ASSERT_EQ(60.0, r.bottom());
    ASSERT_EQ(60.0, r.center().x);
    ASSERT_EQ(40.0, r.center().y);
    ASSERT_EQ(100.0, r.size().width);
    ASSERT_EQ(40.0, r.size().height);
}

UTEST(geometry, rect_from_center) {
    Rect r = Rect::from_center(Point{50, 50}, Size{20, 10});
    ASSERT_EQ(40.0, r.x);
    ASSERT_EQ(45.0, r.y);
    ASSERT_EQ(60.0, r.right());
    ASSERT_EQ(55.0, r.bottom());
}

UTEST(geometry, rect_contains) {
    Rect r{0, 0, 10, 10};
    ASSERT_TRUE(r.contains(Point{5, 5}));
    ASSERT_TRUE(r.contains(Point{0, 10}));  // borders are inside
    ASSERT_FALSE(r.contains(Point{10.1, 5}));
}

UTEST(geometry, rect_united_and_expanded) {
    Rect a{0, 0, 10, 10}, b{20, 5, 10, 10};
    Rect u = a.united(b);
    ASSERT_EQ(0.0, u.x);
    ASSERT_EQ(0.0, u.y);
    ASSERT_EQ(30.0, u.right());
    ASSERT_EQ(15.0, u.bottom());
    Rect e = a.expanded(2);
    ASSERT_EQ(-2.0, e.x);
    ASSERT_EQ(14.0, e.width);
}
