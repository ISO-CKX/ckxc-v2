#include "Sema/Scope.h"

namespace ckx {
namespace Sema {

Scope::Scope(std::shared_ptr<Scope> parentScope, Scope::ScopeFlags scopeFlags)
  : m_ParentScope(parentScope), m_EnclosingFunctionScope(nullptr),
    m_EnclosingLoopScope(nullptr), m_ScopeFlags(scopeFlags) {
  for (sona::ref_ptr<Scope> scope = this;
       scope != nullptr;
       scope = scope->GetParentScope().get()) {
    if (scope->HasFlags(SF_InLoop)) {
      m_EnclosingLoopScope = scope;
      break;
    }
  }

  for (sona::ref_ptr<Scope> scope = this;
       scope != nullptr;
       scope = scope->GetParentScope().get()) {
    if (scope->HasFlags(SF_Function)) {
      m_EnclosingFunctionScope = scope;
      break;
    }
  }
}

void Scope::AddVarDecl(sona::ref_ptr<const AST::VarDecl> varDecl) {
  m_Variables.emplace(varDecl->GetVarName(), varDecl);
}

void Scope::AddType(sona::strhdl_t const& typeName, AST::QualType type) {
  m_Types.emplace(typeName, type);
}

void Scope::AddFunction(sona::ref_ptr<const AST::FuncDecl> funcDecl) {
  m_Functions.emplace(funcDecl->GetName(), funcDecl);
}

sona::ref_ptr<AST::VarDecl const>
Scope::LookupVarDecl(const sona::strhdl_t &name) const noexcept {
  for (sona::ref_ptr<Scope const> s = this; s != nullptr;
       s = s->GetParentScope().get()) {
    sona::ref_ptr<AST::VarDecl const> localResult =
        s->LookupVarDeclLocally(name);
    if (localResult != nullptr) {
      return localResult;
    }
  }
  return nullptr;
}

AST::QualType Scope::LookupType(const sona::strhdl_t &name) const noexcept {
  for (sona::ref_ptr<Scope const> s = this; s != nullptr;
       s = s->GetParentScope().get()) {
    AST::QualType localResult = s->LookupTypeLocally(name);
    if (localResult.GetUnqualTy() != nullptr) {
      return localResult;
    }
  }
  return AST::QualType(nullptr);
}

AST::QualType
Scope::LookupTypeLocally(const sona::strhdl_t& name) const noexcept {
  auto it = m_Types.find(name);
  if (it != m_Types.cend()) {
    return it->second;
  }
  return AST::QualType(nullptr);
}

sona::ref_ptr<const AST::VarDecl>
Scope::LookupVarDeclLocally(const sona::strhdl_t& name) const noexcept {
  auto it = m_Variables.find(name);
  if (it != m_Variables.cend()) {
    return it->second;
  }
  return nullptr;
}

sona::iterator_range<Scope::FunctionSet::const_iterator>
Scope::GetAllFuncsLocal(const sona::strhdl_t &name) const noexcept {
  auto it = m_Functions.find(name);
  if (it == m_Functions.cend()) {
    return sona::iterator_range<FunctionSet::const_iterator>(it, it);
  }

  auto it2 = it;
  ++it2;
  for (; it2 != m_Functions.cend() && it2->first == name; ++it2);
  return sona::iterator_range<FunctionSet::const_iterator>(it, it2);
}

sona::iterator_range<Scope::FunctionSet::const_iterator>
Scope::GetAllFuncs(const sona::strhdl_t &name) const noexcept {
  auto localResult = GetAllFuncsLocal(name);
  if (localResult.size() == 0 && m_ParentScope != nullptr) {
    return m_ParentScope->GetAllFuncs(name);
  }
  return localResult;
}

void Scope::ReplaceVarDecl(const sona::strhdl_t& denotingName,
                           sona::ref_ptr<const AST::VarDecl> varDecl) {
  auto it = m_Variables.find(denotingName);
  sona_assert(it != m_Variables.end());
  it->second = varDecl;
}

} // namespace Sema
} // namespace ckx
