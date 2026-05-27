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
#include "slang/ast/Expression.h"
#include "slang/ast/expressions/MiscExpressions.h"

using namespace slang;
using namespace slang::ast;

namespace {

struct MainVisitor : public TidyVisitor, ASTVisitor<MainVisitor, VisitFlags::StatementsCanonical> {

    Compilation& m_comp;
    EvalContext m_evalCtx;

    explicit MainVisitor(Diagnostics& diag, Compilation& comp) :
        TidyVisitor(diag), m_comp(comp),
        m_evalCtx(m_comp.createScriptScope(), EvalFlags::IsScript) {}

    const Expression* removeImplicitCast(const Expression* expr) {
        if (auto cvt = expr->as_if<ConversionExpression>(); cvt && cvt->isImplicit())
            expr = &cvt->operand();
        return expr;
    }

    bool isConstant(const Expression* expr) {
        m_evalCtx.reset();
        return !expr->eval(m_evalCtx).bad();
    }

    bool isBitConstant(const Expression* expr) {
        expr = removeImplicitCast(expr);
        auto integer = expr->as_if<IntegerLiteral>();
        return integer != nullptr && integer->getEffectiveWidth() == 1;
    }

    bool isBitAccess(const Expression* expr) {
        expr = removeImplicitCast(expr);
        auto selection = expr->as_if<ElementSelectExpression>();
        return selection != nullptr && isConstant(&selection->selector()) &&
               removeImplicitCast(&selection->value())->as_if<NamedValueExpression>() != nullptr;
    }

    void handle(const CaseStatement& caseStmnt) {
        if (skip(sourceManager->getFileName(caseStmnt.sourceRange.start())))
            return;
        const auto& checkConfig = config.getCheckConfigs();
        bool isBitSelect = checkConfig.ignoreVectorBitSelect && isBitConstant(&caseStmnt.expr);
        for (const auto& item : caseStmnt.items) {
            for (const auto& value : item.expressions) {
                if (isBitSelect) {
                    if (!isBitAccess(value))
                        diags.add(diag::NoVarsInCase, value->sourceRange)
                            << "not a constant bit access used in bit-select case statement"sv;
                }
                else {
                    if (!isConstant(value))
                        diags.add(diag::NoVarsInCase, value->sourceRange)
                            << "non-constant case label"sv;
                }
            }
        }
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
    std::string diagString() const override { return "{}"; }
    std::string name() const override { return "NoVarsInCase"; }
    std::string description() const override { return shortDescription(); }
    std::string shortDescription() const override {
        return "Checks for variables or expressions as case conditions. "
               "Such case statements are essentially complex if constructs, "
               "and could be replaced with them to make reading them easier";
    }
};
REGISTER(NoVarsInCase, NoVarsInCase, TidyKind::Style)
