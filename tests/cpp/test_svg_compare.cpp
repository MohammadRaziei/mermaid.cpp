#include "svg_compare.h"
#include "utest.h"

using svgtest::SvgComparer;

UTEST(svg_compare, identical_documents_match) {
    const std::string s = "<svg a=\"1\"><g><rect x=\"1\" y=\"2\"/></g></svg>";
    ASSERT_TRUE(SvgComparer().compare(s, s).empty());
}

UTEST(svg_compare, numbers_within_tolerance_match_inside_any_attribute) {
    SvgComparer c(0.1);
    ASSERT_TRUE(c.compare("<svg><path d=\"M10,20L30.05,40\" transform=\"translate(1, 2)\"/></svg>",
                          "<svg><path d=\"M10.04,20L30,40.09\" transform=\"translate(1.0, 2)\"/></svg>")
                    .empty());
}

UTEST(svg_compare, numbers_beyond_tolerance_differ) {
    ASSERT_FALSE(SvgComparer(0.1).compare("<svg><rect x=\"1\"/></svg>", "<svg><rect x=\"1.5\"/></svg>").empty());
}

UTEST(svg_compare, attribute_names_values_and_extra_attributes_differ) {
    SvgComparer c;
    ASSERT_FALSE(c.compare("<svg><g class=\"a\"/></svg>", "<svg><g class=\"b\"/></svg>").empty());
    ASSERT_FALSE(c.compare("<svg><g class=\"a\"/></svg>", "<svg><g/></svg>").empty());
    ASSERT_FALSE(c.compare("<svg><g/></svg>", "<svg><g class=\"a\"/></svg>").empty());
}

UTEST(svg_compare, element_names_child_counts_and_text_differ) {
    SvgComparer c;
    ASSERT_FALSE(c.compare("<svg><g/></svg>", "<svg><rect/></svg>").empty());
    ASSERT_FALSE(c.compare("<svg><g/></svg>", "<svg><g/><g/></svg>").empty());
    ASSERT_FALSE(c.compare("<svg><text>Hi</text></svg>", "<svg><text>Ho</text></svg>").empty());
}

UTEST(svg_compare, whitespace_between_elements_and_comments_are_ignored) {
    ASSERT_TRUE(SvgComparer().compare("<svg>\n  <g/>\n  <!-- c -->\n</svg>", "<svg><g/></svg>").empty());
}
