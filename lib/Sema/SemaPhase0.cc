#include "Sema/SemaPhase0.h"

#include "Syntax/Concrete.h"
#include "AST/Expr.h"
#include "AST/Stmt.h"
#include "AST/Decl.h"
#include "AST/Type.h"

#include <functional>
#include <numeric>

#include <sona/linq.h>

namespace ckx {
namespace Sema {

SemaPhase0::SemaPhase0(
    AST::ASTContext &astContext,
    std::vector<sona::ref_ptr<AST::DeclContext>> &declContexts,
    Diag::DiagnosticEngine& diag)
  : SemaCommon(astContext, declContexts, diag) {}

sona::owner<AST::TransUnitDecl>
SemaPhase0::ActOnTransUnit(sona::ref_ptr<Syntax::TransUnit> transUnit) {
  sona::owner<AST::TransUnitDecl> transUnitDecl = new AST::TransUnitDecl();
  PushDeclContext(transUnitDecl.borrow().cast_unsafe<AST::DeclContext>());
  PushScope();
  for (sona::ref_ptr<Syntax::Decl const> decl : transUnit->GetDecls()) {
    transUnitDecl.borrow()->AddDecl(ActOnDecl(decl).first);
  }
  return transUnitDecl;
}

void SemaPhase0::PostSubstituteDepends() {
  auto r1 = sona::linq::from_container(m_IncompleteVars).
            transform([](auto& p) -> sona::ref_ptr<IncompleteDecl>
                      { return static_cast<IncompleteDecl*>(&(p.second)); });
  auto r2 = sona::linq::from_container(m_IncompleteTags).
            transform([](auto& p) -> sona::ref_ptr<IncompleteDecl>
                      { return static_cast<IncompleteDecl*>(&(p.second)); });
  auto r3 = sona::linq::from_container(m_IncompleteUsings).
            transform([](auto& p) -> sona::ref_ptr<IncompleteDecl>
                      { return static_cast<IncompleteDecl*>(&(p.second)); });
  auto r4 = sona::linq::from_container(m_IncompleteValueCtors).
            transform([](auto& p) -> sona::ref_ptr<IncompleteDecl>
                      { return static_cast<IncompleteDecl*>(&(p.second)); });

  for (sona::ref_ptr<IncompleteDecl>
       incomplete : r1.concat_with(r2).concat_with(r3).concat_with(r4)) {
    std::shared_ptr<Scope> inScope = incomplete->GetEnclosingScope();
    for (auto &dep : incomplete->GetDependencies()) {
      if (dep.IsDependByname()) {
        AST::QualType type = LookupType(inScope, dep.GetIdUnsafe(), true);
        if (type.GetUnqualTy() == nullptr) {
          m_Diag.Diag(Diag::DIR_Error,
                      Diag::Format(Diag::DMT_ErrNotDeclared,
                      {dep.GetIdUnsafe().GetIdentifier()}),
                      dep.GetIdUnsafe().GetIdSourceRange());
          continue;
        }
        sona::ref_ptr<AST::Decl const> decl =
            AST::GetDeclOfUserDefinedType(type.GetUnqualTy());
         dep.ReplaceNameWithDecl(decl);
      }
    }
  }
}

/// This algorithm directly comes from wikipedia:
///   https://en.wikipedia.org/wiki/Topological_sorting
std::vector<sona::ref_ptr<IncompleteDecl>>
SemaPhase0::FindTranslationOrder() {
  /// @todo add diagnostics info
  std::vector<sona::ref_ptr<IncompleteDecl>> transOrder;
  std::unordered_set<sona::ref_ptr<IncompleteDecl>> permanents;
  std::unordered_set<sona::ref_ptr<IncompleteDecl>> temporaries;

  std::function<bool(sona::ref_ptr<IncompleteDecl>)> VisitIncompleteDecl =
  [&, this](sona::ref_ptr<IncompleteDecl> decl) -> bool {
    if (permanents.find(decl) != permanents.cend()) {
      return true;
    }
    if (temporaries.find(decl) != temporaries.cend()) {
      m_Diag.Diag(Diag::DIR_Error,
                  Diag::Format(Diag::DMT_ErrCircularDepend,
                               {decl->GetName()}),
                  decl->GetRepresentingRange());
      return false;
    }

    temporaries.insert(decl);
    for (sona::ref_ptr<IncompleteDecl> dependingIncompleteDecl :
         sona::linq::from_container(decl->GetDependencies())
           .filter([](Sema::Dependency const& dep)
                   { return dep.IsStrong(); })
           .transform([](Sema::Dependency const& dep)
                      { return dep.GetDeclUnsafe(); })
           .transform([this](sona::ref_ptr<AST::Decl const> depDecl)
                      { return SearchInUnfinished(depDecl); })) {
      if (dependingIncompleteDecl == nullptr) {
        continue;
      }
      if (!VisitIncompleteDecl(dependingIncompleteDecl)) {
        return false;
      }
    }
    temporaries.erase(temporaries.find(decl));
    permanents.insert(decl);
    transOrder.push_back(decl);
    return true;
  };

  auto r1 = sona::linq::from_container(m_IncompleteVars).
            transform([](auto& p) -> sona::ref_ptr<IncompleteDecl>
                      { return static_cast<IncompleteDecl*>(&(p.second)); });
  auto r2 = sona::linq::from_container(m_IncompleteTags).
            transform([](auto& p) -> sona::ref_ptr<IncompleteDecl>
                      { return static_cast<IncompleteDecl*>(&(p.second)); });
  auto r3 = sona::linq::from_container(m_IncompleteUsings).
            transform([](auto& p) -> sona::ref_ptr<IncompleteDecl>
                      { return static_cast<IncompleteDecl*>(&(p.second)); });
  auto r4 = sona::linq::from_container(m_IncompleteValueCtors).
            transform([](auto& p) -> sona::ref_ptr<IncompleteDecl>
                      { return static_cast<IncompleteDecl*>(&(p.second)); });
  for (sona::ref_ptr<IncompleteDecl> incomplete :
       r1.concat_with(r2).concat_with(r3).concat_with(r4)) {
    if (!VisitIncompleteDecl(incomplete)) {
      return transOrder;
    }
  }
  return transOrder;
}

std::vector<IncompleteFuncDecl> &SemaPhase0::GetIncompleteFuncs() {
  return m_IncompleteFuncs;
}

sona::either<AST::QualType, std::vector<Dependency>>
SemaPhase0::ResolveType(sona::ref_ptr<Syntax::Type const> type) {
  switch (type->GetNodeKind()) {
#define CST_TYPE(name) \
  case Syntax::Node::CNK_##name: \
    return Resolve##name(type.cast_unsafe<Syntax::name const>());
#include "Syntax/Nodes.def"
  default:
    sona_unreachable();
  }
  return AST::QualType(nullptr);
}

std::pair<sona::owner<AST::Decl>, bool>
SemaPhase0::ActOnDecl(sona::ref_ptr<const Syntax::Decl> decl) {
  switch (decl->GetNodeKind()) {
#define CST_DECL(name) \
  case Syntax::Node::CNK_##name: \
    return ActOn##name(decl.cast_unsafe<Syntax::name const>());
#include "Syntax/Nodes.def"
  default:
    sona_unreachable();
  }
  return std::make_pair(nullptr, false);
}

sona::either<AST::QualType, std::vector<Dependency>>
SemaPhase0::ResolveBuiltinType(sona::ref_ptr<Syntax::BuiltinType const> bty) {
  return AST::QualType(SemaCommon::ResolveBuiltinTypeImpl(bty));
}

sona::either<AST::QualType, std::vector<Dependency>>
SemaPhase0::
ResolveUserDefinedType(
sona::ref_ptr<Syntax::UserDefinedType const> uty) {
  AST::QualType lookupResult =
      LookupType(GetCurrentScope(), uty->GetName(), false);
  if (lookupResult.GetUnqualTy() != nullptr) {
    if (!CheckTypeComplete(lookupResult.GetUnqualTy())) {
      std::vector<Dependency> dependencies;
      dependencies.emplace_back(
        AST::GetDeclOfUserDefinedType(lookupResult.GetUnqualTy()), true);
      return std::move(dependencies);
    }

    return AST::QualType(lookupResult);
  }

  std::vector<Dependency> dependencies;
  dependencies.emplace_back(uty->GetName().ExplicitlyClone(), true);

  return sona::either<AST::QualType,
                      std::vector<Dependency>>(std::move(dependencies));
}

sona::either<AST::QualType, std::vector<Dependency>>
SemaPhase0::
ResolveTemplatedType(sona::ref_ptr<Syntax::TemplatedType const>) {
  sona_unreachable1("not implemented");
  return AST::QualType(nullptr);
}

static bool IsPtrOrRefType(sona::ref_ptr<Syntax::ComposedType const> cty) {
  return std::any_of(
        cty->GetTypeSpecifiers().begin(),
        cty->GetTypeSpecifiers().end(),
        [](Syntax::ComposedType::TypeSpecifier ts) {
          return ts == Syntax::ComposedType::CTS_Pointer
                 || ts == Syntax::ComposedType::CTS_Ref
                 || ts == Syntax::ComposedType::CTS_RvRef;
  });
}

sona::either<AST::QualType, std::vector<Dependency>>
SemaPhase0::
ResolveComposedType(sona::ref_ptr<Syntax::ComposedType const> cty) {
  auto rootTypeResult = ResolveType(cty->GetRootType());
  if (rootTypeResult.contains_t1()) {
    auto ret = rootTypeResult.as_t1();
    auto r = sona::linq::from_container(cty->GetTypeSpecifiers())
              .zip_with(
               sona::linq::from_container(cty->GetTypeSpecRanges()));
    for (const auto &p : r) {
      switch (p.first) {
      case Syntax::ComposedType::CTS_Pointer:
        ret = m_ASTContext.CreatePointerType(ret);
        break;
      case Syntax::ComposedType::CTS_Ref:
        ret = m_ASTContext.CreateLValueRefType(ret);
        break;
      case Syntax::ComposedType::CTS_RvRef:
        ret = m_ASTContext.CreateRValueRefType(ret);
        break;
      case Syntax::ComposedType::CTS_Const:
        if (ret.IsConst()) {
          m_Diag.Diag(Diag::DIR_Error,
                      Diag::Format(Diag::DMT_ErrDuplicateQual, {"const"}),
                      p.second);
        }
        else {
          ret.AddConst();
        }
        break;
      case Syntax::ComposedType::CTS_Volatile:
        if (ret.IsVolatile()) {
          m_Diag.Diag(Diag::DIR_Error,
                      Diag::Format(Diag::DMT_ErrDuplicateQual, {"volatile"}),
                      p.second);
        }
        else {
          ret.AddVolatile();
        }
        break;
      case Syntax::ComposedType::CTS_Restrict:
        if (ret.IsRestrict()) {
          m_Diag.Diag(Diag::DIR_Error,
                      Diag::Format(Diag::DMT_ErrDuplicateQual, {"restrict"}),
                      p.second);
        }
        else {
          ret.AddRestrict();
        }
        break;
      default:
        sona_unreachable1("not implemented");
      }
    }
    return ret;
  }

  auto dependencies = std::move(rootTypeResult.as_t2());
  if (IsPtrOrRefType(cty)) {
    for (Dependency &dependency : dependencies) {
      dependency.SetStrong(false);
    }
  }

  return sona::either<AST::QualType,
                      std::vector<Dependency>>(std::move(dependencies));
}

std::pair<sona::owner<AST::Decl>, bool>
SemaPhase0::ActOnVarDecl(sona::ref_ptr<Syntax::VarDecl const> decl) {
  {
    auto prevType = GetCurrentScope()->LookupTypeLocally(decl->GetName());
    if (prevType.GetUnqualTy() != nullptr) {
      m_Diag.Diag(Diag::DIR_Error,
                  Diag::Format(Diag::DMT_ErrRedefinition, { decl->GetName() }),
                  decl->GetNameRange());
      return std::make_pair(nullptr, false);
    }
  }

  auto typeResult = ResolveType(decl->GetType());
  if (typeResult.contains_t1()) {
    sona::owner<AST::Decl> varDecl =
        new AST::VarDecl(GetCurrentDeclContext(), typeResult.as_t1(),
                         AST::Decl::DS_None /** @todo  */,
                         decl->GetName());
    GetCurrentScope()->AddVarDecl(varDecl.borrow()
                                  .cast_unsafe<AST::VarDecl>());

    return std::make_pair(std::move(varDecl), true);
  }

  sona::owner<AST::Decl> incomplete =
      new AST::VarDecl(GetCurrentDeclContext(), AST::QualType(nullptr),
                       AST::Decl::DS_None /*TODO*/, decl->GetName());
  GetCurrentScope()->AddVarDecl(incomplete.borrow()
                                          .cast_unsafe<AST::VarDecl>());
  m_IncompleteVars.emplace(
        incomplete.borrow().cast_unsafe<AST::VarDecl>(),
        Sema::IncompleteVarDecl(incomplete.borrow().cast_unsafe<AST::VarDecl>(),
                                decl, GetCurrentDeclContext(),
                                std::move(typeResult.as_t2()),
                                GetCurrentScope()));
  return std::make_pair(std::move(incomplete), false);
}

std::pair<sona::owner<AST::Decl>, bool>
SemaPhase0::ActOnClassDecl(sona::ref_ptr<Syntax::ClassDecl const> decl) {
  {
    auto prevType = GetCurrentScope()->LookupTypeLocally(decl->GetName());
    if (prevType.GetUnqualTy() != nullptr) {
      m_Diag.Diag(Diag::DIR_Error,
                  Diag::Format(Diag::DMT_ErrRedefinition,
                               { decl->GetName() }),
                  decl->GetNameRange());
      return std::make_pair(nullptr, false);
    }
  }

  std::vector<Dependency> collectedDependencies;
  sona::owner<AST::ClassDecl> classDecl =
      new AST::ClassDecl(GetCurrentDeclContext(), decl->GetName());
  PushDeclContext(classDecl.borrow().cast_unsafe<AST::DeclContext>());
  PushScope(Scope::SF_Class);

  for (sona::ref_ptr<Syntax::Decl const> subDecl : decl->GetSubDecls()) {
    auto p = ActOnDecl(subDecl);
    if (!p.second) {
      collectedDependencies.emplace_back(p.first.borrow(), true);
    }
    GetCurrentDeclContext()->AddDecl(std::move(p.first));
  }

  PopScope();
  PopDeclContext();

  GetCurrentScope()->AddType(
      decl->GetName(),
      m_ASTContext.AddUserDefinedType(
          new AST::ClassType(classDecl.borrow())));

  if (!collectedDependencies.empty()) {
    m_IncompleteTags.emplace(
          classDecl.borrow().cast_unsafe<AST::Decl>(),
          IncompleteTagDecl(classDecl.borrow().cast_unsafe<AST::TypeDecl>(),
                            decl.cast_unsafe<Syntax::TagDecl const>(),
                            std::move(collectedDependencies),
                            GetCurrentScope()));
  }

  return std::make_pair(std::move(classDecl).cast_unsafe<AST::Decl>(),
                        !collectedDependencies.empty());
}

std::pair<sona::owner<AST::Decl>, bool>
SemaPhase0::ActOnADTDecl(sona::ref_ptr<Syntax::ADTDecl const> decl) {
  {
    auto prevType = GetCurrentScope()->LookupTypeLocally(decl->GetName());
    if (prevType.GetUnqualTy() != nullptr) {
      m_Diag.Diag(Diag::DIR_Error,
                  Diag::Format(Diag::DMT_ErrRedefinition,
                               { decl->GetName() }),
                  decl->GetNameRange());
      return std::make_pair(nullptr, false);
    }
  }

  std::vector<Dependency> collectedDependencies;
  sona::owner<AST::ADTDecl> adtDecl =
    new AST::ADTDecl(GetCurrentDeclContext(), decl->GetName());
  PushDeclContext(adtDecl.borrow().cast_unsafe<AST::DeclContext>());
  PushScope(Scope::SF_ADT);

  for (sona::ref_ptr<Syntax::ADTDecl::ValueConstructor const> constructor
       : decl->GetConstructors()) {
    auto result = ActOnValueConstructor(constructor);
    if (!result.second) {
      collectedDependencies.emplace_back(result.first.borrow(), true);
    }
    GetCurrentDeclContext()->AddDecl(std::move(result.first));
  }

  PopScope();
  PopDeclContext();

  GetCurrentScope()->AddType(
      adtDecl.borrow()->GetName(),
      m_ASTContext.AddUserDefinedType(
          new AST::ADTType(adtDecl.borrow())));

  if (!collectedDependencies.empty()) {
    m_IncompleteTags.emplace(
          adtDecl.borrow().cast_unsafe<AST::Decl>(),
          IncompleteTagDecl(
            adtDecl.borrow().cast_unsafe<AST::TypeDecl>(),
            decl.cast_unsafe<Syntax::TagDecl const>(),
            std::move(collectedDependencies),
            GetCurrentScope()));
  }

  return std::make_pair(std::move(adtDecl).cast_unsafe<AST::Decl>(),
                        !collectedDependencies.empty());
}

std::pair<sona::owner<AST::Decl>, bool>
SemaPhase0::ActOnValueConstructor(
    sona::ref_ptr<const Syntax::ADTDecl::ValueConstructor> dc) {
  auto typeResult = ResolveType(dc->GetUnderlyingType());
  sona::owner<AST::Decl> ret0 =
      typeResult.contains_t1() ?
        new AST::ValueCtorDecl(GetCurrentDeclContext(),
                                     dc->GetName(), typeResult.as_t1())
      : new AST::ValueCtorDecl(GetCurrentDeclContext(),
                                     dc->GetName(), AST::QualType(nullptr));
  if (typeResult.contains_t2()) {
    m_IncompleteValueCtors.emplace(
          ret0.borrow().cast_unsafe<AST::ValueCtorDecl>(),
          Sema::IncompleteValueCtorDecl(
            ret0.borrow(), dc, std::move(typeResult.as_t2()),
            GetCurrentScope()));
  }

  return std::make_pair(std::move(ret0), typeResult.contains_t1());
}

sona::ref_ptr<IncompleteDecl>
SemaPhase0::SearchInUnfinished(sona::ref_ptr<const AST::Decl> decl) {
  switch (decl->GetDeclKind()) {
  case AST::Decl::DK_Var: {
    auto it = m_IncompleteVars.find(decl.cast_unsafe<AST::VarDecl const>());
    if (it != m_IncompleteVars.cend()) {
      return it->second;
    }
    return nullptr;
  }
  case AST::Decl::DK_Class:
  case AST::Decl::DK_Enum: {
    auto it = m_IncompleteTags.find(decl);
    if (it !=  m_IncompleteTags.cend()) {
      return it->second;
    }
    return nullptr;
  }
  case AST::Decl::DK_ValueCtor: {
    auto it = m_IncompleteValueCtors.find(
                decl.cast_unsafe<AST::ValueCtorDecl const>());
    if (it != m_IncompleteValueCtors.cend()) {
      return it->second;
    }
    return nullptr;
  }
  default: {
    sona_unreachable();
  }
  }
  return nullptr;
}

bool SemaPhase0::CheckTypeComplete(sona::ref_ptr<const AST::Type> type) {
  switch (type->GetTypeId()) {
  case AST::Type::TypeId::TI_Builtin:
  case AST::Type::TypeId::TI_Pointer:
  case AST::Type::TypeId::TI_Ref:
    return true;

  case AST::Type::TypeId::TI_Array:
    return CheckTypeComplete(type.cast_unsafe<AST::ArrayType const>()
                                 ->GetBase().GetUnqualTy());
  case AST::Type::TypeId::TI_Tuple: {
    sona::ref_ptr<AST::TupleType const> tupleType =
        type.cast_unsafe<AST::TupleType const>();
    return std::accumulate(tupleType->GetTupleElemTypes().begin(),
                           tupleType->GetTupleElemTypes().end(), true,
                           [this](bool cur, AST::QualType qty) {
                             return cur && CheckTypeComplete(qty.GetUnqualTy());
                           });
  }

  case AST::Type::TypeId::TI_UserDefined: {
    return CheckUserDefinedTypeComplete(
             type.cast_unsafe<AST::UserDefinedType const>());
  }

  default:
    sona_unreachable1("not implemented");
  }
  return false;
}

bool SemaPhase0::CheckUserDefinedTypeComplete(
    sona::ref_ptr<const AST::UserDefinedType> type) {
  sona::ref_ptr<AST::TypeDecl const> correspondingDecl = type->GetTypeDecl();
  if (type->GetUserDefinedTypeId()
      == AST::UserDefinedType::UDTypeId::UTI_Using) {
    auto it = m_IncompleteUsings.find(
                correspondingDecl.cast_unsafe<AST::UsingDecl const>());
    return it == m_IncompleteUsings.end();
  }
  else {
    auto it = m_IncompleteTags.find(
                correspondingDecl.cast_unsafe<AST::Decl const>());
    return it == m_IncompleteTags.end();
  }
}

std::pair<sona::owner<AST::Decl>, bool>
SemaPhase0::ActOnUsingDecl(sona::ref_ptr<Syntax::UsingDecl const> decl) {
  {
    auto prevType = GetCurrentScope()->LookupTypeLocally(decl->GetName());
    if (prevType.GetUnqualTy() != nullptr) {
      m_Diag.Diag(Diag::DIR_Error,
                  Diag::Format(Diag::DMT_ErrRedefinition,
                               { decl->GetName() }),
                  decl->GetNameRange());
      return std::make_pair(nullptr, false);
    }
  }

  auto typeResult = ResolveType(decl->GetAliasee());
  sona::owner<AST::Decl> ret0 =
      typeResult.contains_t1() ?
        new AST::UsingDecl(GetCurrentDeclContext(),
                           decl->GetName(), typeResult.as_t1())
      : new AST::UsingDecl(GetCurrentDeclContext(),
                           decl->GetName(), AST::QualType(nullptr));
  sona::ref_ptr<AST::UsingDecl> usingDecl =
      ret0.borrow().cast_unsafe<AST::UsingDecl>();
  GetCurrentScope()->AddType(
        usingDecl->GetName(),
        m_ASTContext.AddUserDefinedType(new AST::UsingType(usingDecl)));
  if (typeResult.contains_t2()) {
    m_IncompleteUsings.emplace(
          ret0.borrow().cast_unsafe<AST::UsingDecl>(),
          Sema::IncompleteUsingDecl(
            ret0.borrow().cast_unsafe<AST::UsingDecl>(),
            decl, std::move(typeResult.as_t2()),
            GetCurrentScope()));
  }

  return std::make_pair(std::move(ret0), typeResult.contains_t1());
}

std::pair<sona::owner<AST::Decl>, bool>
SemaPhase0::ActOnFuncDecl(sona::ref_ptr<Syntax::FuncDecl const> decl) {
  m_IncompleteFuncs.emplace_back(decl, GetCurrentScope(),
                                 GetCurrentDeclContext());
  return std::make_pair(nullptr, false);
}

std::pair<sona::owner<AST::Decl>, bool>
SemaPhase0::ActOnEnumDecl(sona::ref_ptr<Syntax::EnumDecl const> decl) {
  {
    auto prevType = GetCurrentScope()->LookupTypeLocally(decl->GetName());
    if (prevType.GetUnqualTy() != nullptr) {
      m_Diag.Diag(Diag::DIR_Error,
                  Diag::Format(Diag::DMT_ErrRedefinition,
                               { decl->GetName() }),
                  decl->GetNameRange());
      return std::make_pair(nullptr, false);
    }
  }

  sona::owner<AST::EnumDecl> enumDecl =
      new AST::EnumDecl(GetCurrentDeclContext(), decl->GetName());
  PushDeclContext(enumDecl.borrow().cast_unsafe<AST::DeclContext>());
  PushScope(Scope::SF_Enum);

  std::int64_t value = 0;
  std::unordered_set<sona::strhdl_t> collectedNames;
  for (Syntax::EnumDecl::Enumerator const& e : decl->GetEnumerators()) {
    if (collectedNames.find(e.GetName()) != collectedNames.cend()) {
      m_Diag.Diag(Diag::DIR_Error,
                  Diag::Format(Diag::DMT_ErrRedeclaration, { e.GetName() }),
                  e.GetNameRange());
      continue;
    }
    value = e.HasValue() ? e.GetValueUnsafe() : value;
    collectedNames.insert(e.GetName());
    GetCurrentDeclContext()->AddDecl(
          new AST::EnumeratorDecl(GetCurrentDeclContext(), e.GetName(), value));
    value++;
  }

  PopScope();
  PopDeclContext();

  GetCurrentScope()->AddType(
        decl->GetName(),
        m_ASTContext.AddUserDefinedType(
          new AST::EnumType(enumDecl.borrow())));

  return std::make_pair(std::move(enumDecl).cast_unsafe<AST::Decl>(), true);
}

std::pair<sona::owner<AST::Decl>, bool>
SemaPhase0::ActOnForwardDecl(sona::ref_ptr<Syntax::ForwardDecl const> decl) {
  (void)decl;
  sona_unreachable1("not implemented");
  return std::make_pair(nullptr, false);
}

std::pair<sona::owner<AST::Decl>, bool>
SemaPhase0::ActOnTemplatedDecl(
    sona::ref_ptr<Syntax::TemplatedDecl const> decl) {
  (void)decl;
  sona_unreachable1("not implemented");
  return std::make_pair(nullptr, false);
}

} // namespace Sema
} // namespace ckx
