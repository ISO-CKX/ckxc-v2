#ifndef AST_EXPR_H
#define AST_EXPR_H

#include "DeclBase.h"
#include "ExprBase.h"
#include "StmtBase.h"
#include "TypeBase.h"

#include "Syntax/Operator.h"

#include "sona/either.h"
#include "sona/optional.h"
#include "sona/pointer_plus.h"
#include <type_traits>
#include <vector>

namespace ckx {
namespace AST {

class CastStep {
public:
  enum CastStepKind {
    // Implicits
    ICSK_IntPromote,
    ICSK_UIntPromote,
    ICSK_FloatPromote,
    ICSK_LValue2RValue,
    ICSK_AdjustQual,
    ICSK_Nil2Ptr,

    // Explicits
    ECSK_IntDowngrade,
    ECSK_UIntDowngrade,
    ECSK_FloatDowngrade,
    ECSK_Signed2Unsigned,
    ECSK_Unsigned2Signed,
    ECSK_Int2Float,
    ECSK_UInt2Float,
    ECSK_Float2Int,
    ECSK_FLoat2UInt,

    // Either explicit or implicit
    CSK_AdjustPtrQual,
    CSK_AdjustRefQual
  };

  CastStep(CastStepKind CSK, QualType destTy, Expr::ValueCat destValueCat) :
    m_CSK(CSK), m_DestTy(destTy), m_DestValueCat(destValueCat) {}

  CastStepKind GetCSK() const noexcept {
    return m_CSK;
  }

  QualType GetDestTy() const noexcept {
    return m_DestTy;
  }

  Expr::ValueCat GetDestValueCat() const noexcept {
    return m_DestValueCat;
  }

private:
  CastStepKind m_CSK;
  QualType m_DestTy;
  Expr::ValueCat m_DestValueCat;
};

class ImplicitCast : public Expr {
public:
  ImplicitCast(sona::owner<Expr> &&castedExpr,
               std::vector<CastStep> &&castSteps)
    : Expr(ExprId::EI_ImplicitCast,
           castSteps.back().GetDestTy(), castSteps.back().GetDestValueCat()),
      m_CastedExpr(std::move(castedExpr)),
      m_CastSteps(std::move(castSteps)) {}

  sona::ref_ptr<Expr const> GetCastedExpr() const noexcept {
    return m_CastedExpr.borrow();
  }

  std::vector<CastStep> const& GetCastSteps() const noexcept {
    return m_CastSteps;
  }

  sona::owner<ImplicitCast> AddCastStep(CastStep const& step) && noexcept {
    m_CastSteps.push_back(step);
    return new ImplicitCast(std::move(m_CastedExpr), std::move(m_CastSteps));
  }

  sona::owner<Backend::ActionResult>
  Accept(sona::ref_ptr<Backend::ExprVisitor> visitor) const override;

private:
  sona::owner<Expr> m_CastedExpr;
  std::vector<CastStep> m_CastSteps;
};

class ExplicitCastExpr : public Expr {
public:
  enum ExplicitCastOperator {
    ECOP_Static,
    ECOP_Const,
    ECOP_Bit
  };

  ExplicitCastExpr(ExplicitCastOperator castOp,
                   sona::owner<Expr> &&castedExpr, QualType destTy,
                   ValueCat destValueCat)
    : Expr(ExprId::EI_ExplicitCast, destTy, destValueCat),
      m_CastOp(castOp), m_CastedExpr(std::move(castedExpr)),
      m_MaybeCastSteps(sona::empty_optional()) {
    sona_assert1(castOp != ExplicitCastOperator::ECOP_Static,
                 "static_cast requires cast step chain");
  }

  ExplicitCastExpr(ExplicitCastOperator castOp,
                   sona::owner<Expr> &&castedExpr,
                   std::vector<CastStep> &&castSteps)
    : Expr(ExprId::EI_ExplicitCast,
           castSteps.back().GetDestTy(),
           castSteps.back().GetDestValueCat()),
      m_CastOp(castOp), m_CastedExpr(std::move(castedExpr)),
      m_MaybeCastSteps(std::move(castSteps)) {
    sona_assert1(castOp == ExplicitCastOperator::ECOP_Static,
                 "only static_cast can have cast step chain");
  }

  ExplicitCastOperator GetCastOp() const noexcept { return m_CastOp; }

  sona::ref_ptr<Expr const> GetCastedExpr() const noexcept {
    return m_CastedExpr.borrow();
  }

  std::vector<CastStep> const& GetCastStepsUnsafe() const noexcept {
    sona_assert(m_MaybeCastSteps.has_value());
    return m_MaybeCastSteps.value();
  }

  sona::owner<Backend::ActionResult>
  Accept(sona::ref_ptr<Backend::ExprVisitor> visitor) const override;

private:
  ExplicitCastOperator m_CastOp;
  sona::owner<Expr> m_CastedExpr;
  sona::optional<std::vector<CastStep>> m_MaybeCastSteps;
};

class AssignExpr : public Expr {
public:
  enum class AssignmentOperator {
#define ASSIGN_OP_DEF(name, rep, text) AOP_##name,
#include "Syntax/Operators.def"
  AOP_Invalid
  };

  AssignExpr(AssignmentOperator op, sona::owner<Expr> &&assigned,
             sona::owner<Expr> &&assignee, QualType type, ValueCat valueCat)
      : Expr(ExprId::EI_Assign, type, valueCat), m_Operator(op),
        m_Assigned(std::move(assigned)), m_Assignee(std::move(assignee)) {}

  AssignmentOperator GetOperator() const noexcept { return m_Operator; }

  sona::ref_ptr<Expr const> GetAssigned() const noexcept {
    return m_Assigned.borrow();
  }

  sona::ref_ptr<Expr const> GetAssignee() const noexcept {
    return m_Assignee.borrow();
  }

  sona::owner<Backend::ActionResult>
  Accept(sona::ref_ptr<Backend::ExprVisitor> visitor) const override;

private:
  AssignmentOperator m_Operator;
  sona::owner<Expr> m_Assigned, m_Assignee;
};

class UnaryExpr : public Expr {
public:
  enum UnaryOperator {
  #define UNARY_OP_DEF(name, rep, text) UOP_##name,
  #include "Syntax/Operators.def"
    UOP_Invalid
  };

  UnaryExpr(UnaryOperator op, sona::owner<Expr> &&operand,
            QualType exprType, ValueCat valueCat)
    : Expr(ExprId::EI_Unary, exprType, valueCat), m_Operator(op),
      m_Operand(std::move(operand)) {}

  UnaryOperator GetOperator() const noexcept { return m_Operator; }

  sona::ref_ptr<Expr const> GetOperand() const noexcept {
    return m_Operand.borrow();
  }

  sona::owner<Backend::ActionResult>
  Accept(sona::ref_ptr<Backend::ExprVisitor> visitor) const override;

private:
  UnaryOperator m_Operator;
  sona::owner<Expr> m_Operand;
};

class BinaryExpr : public Expr {
public:
  enum BinaryOperator {
  #define BINARY_OP_DEF(name, rep, text) BOP_##name,
  #include "Syntax/Operators.def"
    BOP_Invalid
  };

  BinaryExpr(BinaryOperator op, sona::owner<Expr> &&leftOperand,
             sona::owner<Expr> &&rightOperand, QualType exprType,
             ValueCat valueCat)
    : Expr(ExprId::EI_Binary, exprType, valueCat), m_Operator(op),
      m_LeftOperand(std::move(leftOperand)),
      m_RightOperand(std::move(rightOperand)){}

  BinaryOperator GetOperator() const noexcept { return m_Operator; }

  sona::ref_ptr<Expr const> GetLeftOperand() const noexcept {
    return m_LeftOperand.borrow();
  }

  sona::ref_ptr<Expr const> GetRightOperand() const noexcept {
    return m_RightOperand.borrow();
  }

  sona::owner<Backend::ActionResult>
  Accept(sona::ref_ptr<Backend::ExprVisitor> visitor) const override;

private:
  BinaryOperator m_Operator;
  sona::owner<Expr> m_LeftOperand, m_RightOperand;
};

class CondExpr : public Expr {
public:
  CondExpr(sona::owner<Expr> &&condExpr, sona::owner<Expr> &&thenExpr,
           sona::owner<Expr> &&elseExpr, QualType type, ValueCat valueCat)
    : Expr(ExprId::EI_Cond, type, valueCat),
      m_CondExpr(std::move(condExpr)),
      m_ThenExpr(std::move(thenExpr)),
      m_ElseExpr(std::move(elseExpr)) {
    sona_assert(m_ThenExpr.borrow()->GetExprType()
                == m_ElseExpr.borrow()->GetExprType());
  }

  sona::owner<Backend::ActionResult>
  Accept(sona::ref_ptr<Backend::ExprVisitor> visitor) const override;

private:
  sona::owner<Expr> m_CondExpr, m_ThenExpr, m_ElseExpr;
};

class IdRefExpr : public Expr {
public:
  IdRefExpr(sona::ref_ptr<AST::VarDecl const> varDecl,
            QualType type, ValueCat valueCat)
    : Expr(ExprId::EI_ID, type, valueCat), m_VarDecl(varDecl) {}

  sona::ref_ptr<AST::VarDecl const> GetVarDecl() const noexcept {
    return m_VarDecl;
  }

  sona::owner<Backend::ActionResult>
  Accept(sona::ref_ptr<Backend::ExprVisitor> visitor) const override;

private:
  sona::ref_ptr<AST::VarDecl const> m_VarDecl;
};

class TestExpr : public Expr {
public:
  TestExpr(QualType type, ValueCat valueCat)
    : Expr(ExprId::EI_Test, type, valueCat) {}

  sona::owner<Backend::ActionResult>
  Accept(sona::ref_ptr<Backend::ExprVisitor> visitor) const override;
};

class IntLiteralExpr : public Expr {
public:
  IntLiteralExpr(std::int64_t value, QualType type)
    : Expr(ExprId::EI_IntLiteral, type, ValueCat::VC_RValue), m_Value(value) {}

  std::int64_t GetValue() const noexcept { return m_Value; }

  sona::owner<Backend::ActionResult>
  Accept(sona::ref_ptr<Backend::ExprVisitor> visitor) const override;

private:
  std::int64_t m_Value;
};

class UIntLiteralExpr : public Expr {
public:
  UIntLiteralExpr(std::uint64_t value, QualType type)
    : Expr(ExprId::EI_UIntLiteral, type, ValueCat::VC_RValue),
      m_Value(value) {}

  std::uint64_t GetValue() const noexcept { return m_Value; }

  sona::owner<Backend::ActionResult>
  Accept(sona::ref_ptr<Backend::ExprVisitor> visitor) const override;

private:
  std::uint64_t m_Value;
};

class FloatLiteralExpr : public Expr {
public:
  FloatLiteralExpr(double value, QualType type)
    : Expr(ExprId::EI_FloatLiteral, type, ValueCat::VC_RValue),
      m_Value(value) {}

  double GetValue() const noexcept { return m_Value; }

  sona::owner<Backend::ActionResult>
  Accept(sona::ref_ptr<Backend::ExprVisitor> visitor) const override;

private:
  double m_Value;
};

class CharLiteralExpr : public Expr {
public:
  CharLiteralExpr(char value, QualType type)
    : Expr(ExprId::EI_CharLiteral, type, ValueCat::VC_RValue), m_Value(value) {}

  char GetValue() const noexcept { return m_Value; }

  sona::owner<Backend::ActionResult>
  Accept(sona::ref_ptr<Backend::ExprVisitor> visitor) const override;

private:
  /// @todo use char32_t or other wider types to represent one char
  char m_Value;
};

class StringLiteralExpr : public Expr {
public:
  StringLiteralExpr(sona::strhdl_t value, QualType type)
    : Expr(ExprId::EI_CharLiteral, type, ValueCat::VC_RValue), m_Value(value) {}

  sona::strhdl_t GetValue() const noexcept { return m_Value; }

  sona::owner<Backend::ActionResult>
  Accept(sona::ref_ptr<Backend::ExprVisitor> visitor) const override;

private:
  sona::strhdl_t m_Value;
};

class BoolLiteralExpr : public Expr {
public:
  BoolLiteralExpr(bool value, QualType type)
    : Expr(ExprId::EI_BoolLiteral, type, ValueCat::VC_RValue), m_Value(value) {}

  bool GetValue() const noexcept { return m_Value; }

  sona::owner<Backend::ActionResult>
  Accept(sona::ref_ptr<Backend::ExprVisitor> visitor) const override;

private:
  bool m_Value;
};

class NullptrLiteralExpr : public Expr {
public:
  NullptrLiteralExpr(QualType type)
    : Expr(ExprId::EI_NullptrLiteral, type, ValueCat::VC_RValue) {}

  sona::owner<Backend::ActionResult>
  Accept(sona::ref_ptr<Backend::ExprVisitor> visitor) const override;
};

class ParenExpr : public Expr {
public:
  ParenExpr(sona::owner<Expr> &&expr)
    : Expr(ExprId::EI_Paren, expr.borrow()->GetExprType(),
           expr.borrow()->GetValueCat()),
      m_Expr(std::move(expr)) {}

  sona::ref_ptr<Expr const> GetExpr() const noexcept { return m_Expr.borrow(); }

  sona::owner<Backend::ActionResult>
  Accept(sona::ref_ptr<Backend::ExprVisitor> visitor) const override;

private:
  sona::owner<Expr> m_Expr;
};

} // namespace AST
} // namespace ckx

#endif // AST_EXPR_H
