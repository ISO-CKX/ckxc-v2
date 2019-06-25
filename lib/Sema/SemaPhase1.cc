#include "Sema/SemaPhase1.h"

#include "AST/Expr.h"
#include "Syntax/Concrete.h"

namespace ckx {
namespace Sema {

SemaPhase1::SemaPhase1(AST::ASTContext &astContext,
    std::vector<sona::ref_ptr<AST::DeclContext>> &declContexts,
    Diag::DiagnosticEngine &diag)
  : SemaCommon(astContext, declContexts, diag) {}

void SemaPhase1::PostTranslateIncompletes(
    std::vector<sona::ref_ptr<IncompleteDecl>> incompletes) {
  for (auto incomplete : incompletes) {
    switch (incomplete->GetType()) {
    case IncompleteDecl::IDT_Var:
      PostTranslateIncompleteVar(incomplete.cast_unsafe<IncompleteVarDecl>());
      break;
    case IncompleteDecl::IDT_Tag:
      PostTranslateIncompleteTag(incomplete.cast_unsafe<IncompleteTagDecl>());
      break;
    case IncompleteDecl::IDT_ValueCtor:
      PostTranslateIncompleteADTConstructor(
            incomplete.cast_unsafe<IncompleteValueCtorDecl>());
      break;
    case IncompleteDecl::IDT_Using:
      PostTranslateIncompleteUsing(
            incomplete.cast_unsafe<IncompleteUsingDecl>());
      break;
    case IncompleteDecl::IDT_Function:
      sona_unreachable1("functions should not be solved here");
      break;
    }
  }
}

sona::ref_ptr<const AST::Type>
SemaPhase1::ResolveType(std::shared_ptr<Scope> scope,
                        sona::ref_ptr<const Syntax::Type> type) {
  switch (type->GetNodeKind()) {
#define CST_TYPE(name) \
  case Syntax::Node::NodeKind::CNK_##name: \
    return Resolve##name(scope, type.cast_unsafe<Syntax::name const>());
#include "Syntax/Nodes.def"
  default:
    sona_unreachable();
  }
  return sona::ref_ptr<AST::Type const>(nullptr);
}

void SemaPhase1::PostTranslateIncompleteVar(
    sona::ref_ptr<IncompleteVarDecl> iVar) {
  sona::ref_ptr<AST::Type const> varType =
      ResolveType(iVar->GetEnclosingScope(), iVar->GetConcrete()->GetType());
  sona_assert(varType != nullptr);
  iVar->GetIncomplete()->SetType(varType);
}

void SemaPhase1::PostTranslateIncompleteTag(
    sona::ref_ptr<IncompleteTagDecl> iTag) {
  (void)iTag;
  // do nothing since the tag will be completed as its fields get completed
}

void SemaPhase1::PostTranslateIncompleteADTConstructor(
    sona::ref_ptr<IncompleteValueCtorDecl> iAdtC) {
  sona::ref_ptr<AST::ValueCtorDecl> halfway =
      iAdtC->GetHalfway().cast_unsafe<AST::ValueCtorDecl>();
  sona::ref_ptr<AST::Type const> adtConstructorType =
      ResolveType(iAdtC->GetEnclosingScope(),
                  iAdtC->GetConcrete()->GetUnderlyingType());
  halfway->SetType(adtConstructorType);
}

void SemaPhase1::PostTranslateIncompleteUsing(
    sona::ref_ptr<IncompleteUsingDecl> iusing) {
  sona::ref_ptr<AST::Type const> aliasee =
      ResolveType(iusing->GetEnclosingScope(),
                  iusing->GetConcrete()->GetAliasee());
  iusing->GetHalfway()->FillAliasee(aliasee);
}

sona::ref_ptr<AST::Type const>
SemaPhase1::ResolveBuiltinType(std::shared_ptr<Scope>,
                             sona::ref_ptr<Syntax::BuiltinType const> bty) {
  return SemaCommon::ResolveBuiltinTypeImpl(bty);
}

sona::ref_ptr<AST::Type const>
SemaPhase1::
ResolveUserDefinedType(std::shared_ptr<Scope> scope,
                       sona::ref_ptr<Syntax::UserDefinedType const> uty) {
  sona::ref_ptr<AST::Type const> lookupResult =
      LookupType(scope, uty->GetName(), false);
  sona_assert(lookupResult != nullptr);
  return lookupResult;
}

sona::ref_ptr<AST::Type const>
SemaPhase1::
ResolveTemplatedType(std::shared_ptr<Scope>,
                     sona::ref_ptr<Syntax::TemplatedType const>) {
  sona_unreachable1("not implemented");
  return sona::ref_ptr<AST::Type const>(nullptr);
}

sona::ref_ptr<AST::Type const>
SemaPhase1::
ResolveComposedType(std::shared_ptr<Scope> scope,
                    sona::ref_ptr<Syntax::ComposedType const> cty) {
  sona::ref_ptr<AST::Type const> ret
      = ResolveType(scope, cty->GetRootType());

  for (Syntax::ComposedType::TypeSpecifier ts : cty->GetTypeSpecifiers()) {
    switch (ts) {
    case Syntax::ComposedType::TypeSpecifier::CTS_Pointer:
      ret = m_ASTContext.CreatePointerType(ret)
                        .cast_unsafe<AST::Type const>();
      break;
    case Syntax::ComposedType::TypeSpecifier::CTS_Ref:
      ret = m_ASTContext.CreateLValueRefType(ret)
                        .cast_unsafe<AST::Type const>();
      break;
    case Syntax::ComposedType::TypeSpecifier::CTS_RvRef:
      ret = m_ASTContext.CreateRValueRefType(ret)
                        .cast_unsafe<AST::Type const>();
      break;
    default:
      sona_unreachable1("not implemented");
    }
  }
  return ret;
}

sona::ref_ptr<AST::Expr const>
SemaPhase1::ActOnLiteralExpr(
    std::shared_ptr<Scope>,
    sona::ref_ptr<Syntax::LiteralExpr const> literalExpr) {
  /// @todo we had to hand write this since we have different process for
  /// integral, unsigned and floating types.
  switch (literalExpr->GetLiteralTypeKind()) {
  case Syntax::BuiltinType::TypeKind::TK_Int8:
  case Syntax::BuiltinType::TypeKind::TK_Int16:
  case Syntax::BuiltinType::TypeKind::TK_Int32:
  case Syntax::BuiltinType::TypeKind::TK_Int64:
    return new AST::IntegralLiteralExpr(literalExpr->GetAsIntUnsafe());
    break;

  case Syntax::BuiltinType::TypeKind::TK_UInt8:
  case Syntax::BuiltinType::TypeKind::TK_UInt16:
  case Syntax::BuiltinType::TypeKind::TK_UInt32:
  case Syntax::BuiltinType::TypeKind::TK_UInt64:
    return new AST::FloatingLiteralExpr(literalExpr->GetAsUIntUnsafe());
    break;

  case Syntax::BuiltinType::TypeKind::TK_Float:
  case Syntax::BuiltinType::TypeKind::TK_Double:
  case Syntax::BuiltinType::TypeKind::TK_Quad:
    return new AST::FloatingLiteralExpr(literalExpr->GetAsFloatUnsafe());
    break;

  default:
    sona_unreachable1("not implemented");
  }
}

} // namespace Sema
} // namespace ckx
