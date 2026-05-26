//------------------------------------------------------------------------------
//! @file NoVarsInCase.cpp
//! @brief Do not use variables or expressions as case clause conditions
//! (STARC-2.8.5.3)
//
// SPDX-FileCopyrightText: Didyk Ivan
// SPDX-License-Identifier: WTFPL
//------------------------------------------------------------------------------
#include "ASTHelperVisitors.h"
#include "TidyDiags.h"

#include "slang/ast/EvalContext.h"

using namespace slang;
using namespace slang::ast;

namespace {

// TODO: do we want canonical or not?
struct MainVisitor : public TidyVisitor, ASTVisitor<MainVisitor, VisitFlags::StatementsCanonical> {

    Compilation& m_comp;
    EvalContext m_evalCtx;

    explicit MainVisitor(Diagnostics& diag, Compilation& comp) :
        TidyVisitor(diag), m_comp(comp),
        m_evalCtx(m_comp.createScriptScope(), EvalFlags::IsScript) {}

    bool isConstant(const Expression* expr) {
        m_evalCtx.reset();
        expr->eval(m_evalCtx);
        return m_evalCtx.getAllDiagnostics().empty();
    }

    bool isBitConstant(const Expression* expr) {
        if (auto cvt = expr->as_if<ConversionExpression>()) {
            if (!cvt->isImplicit())
                return false;
            expr = &cvt->operand();
        }
        auto integer = expr->as_if<IntegerLiteral>();
        if (!integer)
            return false;
        if (integer->getEffectiveWidth() != 1)
            return false;
        return true;
    }

    bool isBitAccess(const Expression* expr) {
        if (auto cvt = expr->as_if<ConversionExpression>()) {
            if (!cvt->isImplicit())
                return false;
            expr = &cvt->operand();
        }
        auto selection = expr->as_if<ElementSelectExpression>();
        if (!selection)
            return false;
        auto selector = selection->selector().as_if<IntegerLiteral>();
        if (!selector)
            return false;
        return true; // assume we are selecting bit, otherwise we got syntax error
    }

    void handle(const CaseStatement& caseStmnt) {
        const auto& checkConfig = config.getCheckConfigs();

        if (checkConfig.ignoreVectorBitSelect && isBitConstant(&caseStmnt.expr)) {
            for (const auto& item : caseStmnt.items)
                for (const auto& value : item.expressions)
                    if (!isBitAccess(value))
                        diags.add(diag::NoVarsInCase, value->sourceRange);
            return;
        }

        for (const auto& item : caseStmnt.items)
            for (const auto& value : item.expressions)
                if (!isConstant(value))
                    diags.add(diag::NoVarsInCase, value->sourceRange);
    }
};

}; // namespace

class NoVarsInCase : public TidyCheck {
public:
    [[maybe_unused]] explicit NoVarsInCase(TidyKind kind,
                                           std::optional<slang::DiagnosticSeverity> sev) :
        TidyCheck(kind, sev) {}

    bool check(const ast::RootSymbol& root, const analysis::AnalysisManager&) override {
        MainVisitor visitor(diagnostics, root.getCompilation());
        root.visit(visitor);
        return diagnostics.empty();
    }

    DiagCode diagCode() const override { return diag::NoVarsInCase; }
    DiagnosticSeverity diagDefaultSeverity() const override { return DiagnosticSeverity::Warning; }
    std::string diagString() const override {
        return "use of non-constant value in case condition";
    }
    std::string name() const override { return "NoVarsInCase"; }
    std::string description() const override { return shortDescription(); }
    std::string shortDescription() const override {
        return "Checks for variables or expressions as case conditions. "
               "Such case statements are essentially complex if constructs, "
               "and could be replaced with them to make reading them easier";
    }
};
REGISTER(NoVarsInCase, NoVarsInCase, TidyKind::Style)
