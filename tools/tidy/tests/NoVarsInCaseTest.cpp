// SPDX-FileCopyrightText: Didyk Ivan
// SPDX-License-Identifier: WTFPL

#include "TidyTest.h"

static TidyConfig getCaseVarsConfig(bool ignoreBitselect) {
    TidyConfig cfg;
    cfg.getCheckConfigs().ignoreVectorBitSelect = ignoreBitselect;
    return cfg;
}

TEST_CASE("NoVarsInCase: Must allow constant case labels") {
    CHECK(runCheckTest("NoVarsInCase", R"(
module m #(parameter P = 1'b1) (input i, input j);
    always_comb begin;
        case (i)
            1'b0:       $display("one");     // Simple constants are compliant
            P:          $display("param");   // Params also
            default:    $display("default"); // Default case is always good
        endcase
    end
endmodule
)",
                       getCaseVarsConfig(false))); // we just cannot have nice formatting
}

TEST_CASE("NoVarsInCase: Must disallow non-constant case labels") {
    CHECK_FALSE(runCheckTest("NoVarsInCase", R"(
module m(input i, input j);
    always_comb begin;
        case (i)
            j:          $display("j");
            default:    $display("default");
        endcase
    end
endmodule
)",
                             getCaseVarsConfig(false)));
}

TEST_CASE("NoVarsInCase: Will ignore correct bitselect when asked") {
    CHECK(runCheckTest("NoVarsInCase", R"(
module bitselect(input reg [1:0] x);
    parameter first = 0;
    always_comb begin;
        case(1'b1)
            x[first]:       $display("first");
            x[1 - first]:   $display("second");
        endcase
    end
endmodule
)",
                       getCaseVarsConfig(true)));
}

TEST_CASE("NoVarsInCase: Will flag a valid bitselect when not asked to ignore it") {
    CHECK_FALSE(runCheckTest("NoVarsInCase", R"(
module bitselect(input reg [1:0] x);
    parameter first = 0;
    always_comb begin;
        case(1'b1)
            x[first]:       $display("first");
            x[1 - first]:   $display("second");
        endcase
    end
endmodule
)",
                             getCaseVarsConfig(false)));
}

TEST_CASE("NoVarsInCase: Will give bitselect errors") {
    CHECK_FALSE(runCheckTest("NoVarsInCase", R"(
module bitselect(input reg [1:0] x);
    logic dummy;
    always_comb begin;
        case(1'b1)
            dummy: $display("bad select");
        endcase
    end
endmodule
)",
                             getCaseVarsConfig(false)));
}
