#ifndef UNRESOLVEDDECL_HPP
#define UNRESOLVEDDECL_HPP

#include "Sema/Dependency.h"
#include "Syntax/CSTFwd.h"
#include "AST/Decl.hpp"

#include <iosfwd>

namespace ckx {
namespace Sema {

class Scope;

class IncompleteDecl {
public:
  IncompleteDecl(std::vector<Dependency> &&dependencies,
                 std::shared_ptr<Scope> const& inScope) :
    m_Dependencies(std::move(dependencies)),
    m_InScope(inScope) {}

  IncompleteDecl(IncompleteDecl const&) = delete;

  IncompleteDecl(IncompleteDecl &&that)
    : m_Dependencies(std::move(that.m_Dependencies)),
      m_InScope(that.m_InScope) {}

  std::vector<Dependency>& GetDependencies() noexcept {
    return m_Dependencies;
  }

  std::vector<Dependency> const& GetDependencies() const noexcept {
    return m_Dependencies;
  }

  void AddNameDepend(Syntax::Identifier &&id, bool isStrong);
  void AddValueDepend(sona::ref_ptr<AST::Decl> decl, bool isStrong);

  std::shared_ptr<Scope> & GetEnclosingScope() noexcept {
    return m_InScope;
  }

  virtual std::string ToString() const = 0;

  virtual ~IncompleteDecl() {}

private:
  std::vector<Dependency> m_Dependencies;
  std::shared_ptr<Scope> m_InScope;
};

class IncompleteVarDecl : public IncompleteDecl {
public:
  IncompleteVarDecl(sona::ref_ptr<AST::VarDecl> incomplete,
                    sona::ref_ptr<Syntax::VarDecl const> concrete,
                    sona::ref_ptr<AST::DeclContext> inContext,
                    std::vector<Dependency> &&dependencies,
                    std::shared_ptr<Scope> const& inScope)
    : IncompleteDecl(std::move(dependencies), inScope),
      m_Incomplete(incomplete), m_Concrete(concrete), m_InContext(inContext) {}

  sona::ref_ptr<AST::VarDecl> GetIncomplete() noexcept {
    return m_Incomplete;
  }

  sona::ref_ptr<Syntax::VarDecl const> GetConcrete() const noexcept {
    return m_Concrete;
  }

  sona::ref_ptr<AST::DeclContext> GetDeclContext() noexcept {
    return m_InContext;
  }

  std::string ToString() const override;

private:
  sona::ref_ptr<AST::VarDecl> m_Incomplete;
  sona::ref_ptr<Syntax::VarDecl const> m_Concrete;
  sona::ref_ptr<AST::DeclContext> m_InContext;
};

class IncompleteTagDecl : public IncompleteDecl {
public:
  IncompleteTagDecl(sona::ref_ptr<AST::Decl> halfway,
                    std::vector<Dependency> &&dependencies,
                    std::shared_ptr<Scope> const& inScope)
    : IncompleteDecl(std::move(dependencies), inScope), m_Halfway(halfway) {}

  sona::ref_ptr<AST::Decl const> GetHalfway() const noexcept {
    return m_Halfway;
  }

  std::string ToString() const override;

private:
  sona::ref_ptr<AST::Decl> m_Halfway;
};

class IncompleteEnumClassInternDecl : public IncompleteDecl {
public:
  IncompleteEnumClassInternDecl(sona::ref_ptr<AST::Decl> halfway,
                                std::vector<Dependency> &&dependencies,
                                std::shared_ptr<Scope> const& inScope)
    : IncompleteDecl(std::move(dependencies), inScope), m_Halfway(halfway) {}

  sona::ref_ptr<AST::Decl const> GetHalfway() const noexcept {
    return m_Halfway;
  }

  std::string ToString() const override;

private:
  sona::ref_ptr<AST::Decl> m_Halfway;
};

class IncompleteUsingDecl : public IncompleteDecl {
public:
  IncompleteUsingDecl(sona::ref_ptr<AST::UsingDecl> halfway,
                      std::vector<Dependency> &&dependencies,
                      std::shared_ptr<Scope> const& inScope)
    : IncompleteDecl(std::move(dependencies), inScope), m_Halfway(halfway) {}

  sona::ref_ptr<AST::UsingDecl const> GetHalfway() const noexcept {
    return m_Halfway;
  }

  std::string ToString() const override;

private:
  sona::ref_ptr<AST::UsingDecl> m_Halfway;
};

class IncompleteFuncDecl : public IncompleteDecl {
public:
  IncompleteFuncDecl(sona::ref_ptr<Syntax::FuncDecl const> funcDecl,
                     std::shared_ptr<Scope> const& inScope,
                     sona::ref_ptr<AST::DeclContext> inContext)
    : IncompleteDecl(std::vector<Dependency>(), inScope),
      m_FuncDecl(funcDecl), m_InContext(inContext) {}

  std::string ToString() const override;

private:
  sona::ref_ptr<Syntax::FuncDecl const> m_FuncDecl;
  sona::ref_ptr<AST::DeclContext> m_InContext;
};

} // namespace Sema
} // namespace ckx

#endif // UNRESOLVEDDECL_HPP
