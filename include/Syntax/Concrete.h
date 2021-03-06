#ifndef CONCRETE_H
#define CONCRETE_H

#include <vector>
#include <string>

#include <Basic/SourceRange.h>
#include <Syntax/Operator.h>

#include <sona/range.h>
#include <sona/linq.h>
#include <sona/pointer_plus.h>
#include <sona/stringref.h>

namespace ckx {
namespace Syntax {

class Identifier {
public:
  Identifier(sona::strhdl_t const& identifier,
             SingleSourceRange const& idRange)
    : m_Identifier(identifier), m_IdRange(idRange) {}

  Identifier(std::vector<sona::strhdl_t> &&nestedNameSpecifiers,
             sona::strhdl_t identifier,
             std::vector<SingleSourceRange> &&nnsRanges,
             SingleSourceRange idRange)
    : m_NestedNameSpecifiers(std::move(nestedNameSpecifiers)),
      m_Identifier(identifier),
      m_NNSRanges(std::move(nnsRanges)),
      m_IdRange(idRange) {}

  Identifier(Identifier &&that)
    : m_NestedNameSpecifiers(std::move(that.m_NestedNameSpecifiers)),
      m_Identifier(that.m_Identifier),
      m_NNSRanges(std::move(that.m_NNSRanges)),
      m_IdRange(that.m_IdRange) {}

  Identifier(Identifier const&) = delete;
  Identifier& operator=(Identifier const&) = delete;

  Identifier ExplicitlyClone() const {
    std::vector<sona::strhdl_t> copyOfNNS = m_NestedNameSpecifiers;
    sona::strhdl_t copyOfIdentifier = m_Identifier;
    std::vector<SingleSourceRange> copyOfNNSRanges = m_NNSRanges;
    SingleSourceRange copyOfIdRange = m_IdRange;
    return Identifier(std::move(copyOfNNS), std::move(copyOfIdentifier),
                      std::move(copyOfNNSRanges), std::move(copyOfIdRange));
  }

  sona::strhdl_t const& GetIdentifier() const noexcept {
    return m_Identifier;
  }

  SingleSourceRange const& GetIdSourceRange() const noexcept {
    return m_IdRange;
  }

  std::vector<sona::strhdl_t> const&
  GetNestedNameSpecifiers() const noexcept {
    return m_NestedNameSpecifiers;
  }

  std::vector<SingleSourceRange> const& GetNNSSourceRanges() const noexcept {
    return m_NNSRanges;
  }

private:
  std::vector<sona::strhdl_t> m_NestedNameSpecifiers;
  sona::strhdl_t m_Identifier;
  std::vector<SingleSourceRange> m_NNSRanges;
  SingleSourceRange m_IdRange;
};

class Node {
public:
  enum NodeKind {
    #define CST_TRANSUNIT(name) CNK_##name,
    #define CST_MISC(name) CNK_##name,
    #define CST_TYPE(name) CNK_##name,
    #define CST_DECL(name) CNK_##name,
    #define CST_STMT(name) CNK_##name,
    #define CST_EXPR(name) CNK_##name,

    #include "Syntax/Nodes.def"
  };

  Node(NodeKind nodeKind) : m_NodeKind(nodeKind) {}
  virtual ~Node() {}

  NodeKind GetNodeKind() const noexcept {
    return m_NodeKind;
  }

private:
  NodeKind m_NodeKind;
};

class AttributeList : public Node {
public:
  class Attribute {
  public:
    Attribute(sona::strhdl_t const& attributeName,
              sona::strhdl_t const& attributeValue,
              SingleSourceRange nameRange,
              SingleSourceRange valueRange)
      : m_AttributeName(attributeName),
        m_AttributeValue(attributeValue),
        m_NameRange(nameRange),
        m_ValueRange(valueRange) {}

    Attribute(sona::strhdl_t const& attributeName,
              SingleSourceRange nameRange,
              SingleSourceRange valueRange)
      : m_AttributeName(attributeName),
        m_AttributeValue(sona::empty_optional()),
        m_NameRange(nameRange),
        m_ValueRange(valueRange) {}

    sona::strhdl_t const& GetAttributeName() const noexcept {
      return m_AttributeName;
    }

    bool HasAttributeValue() const noexcept {
      return m_AttributeValue.has_value();
    }

    sona::strhdl_t const& GetAttributeValueUnsafe() const noexcept {
      return m_AttributeValue.value();
    }

    SingleSourceRange const& GetNameRange() const noexcept {
      return m_NameRange;
    }

    SingleSourceRange const& GetValueRange() const noexcept {
      return m_ValueRange;
    }

  private:
    sona::strhdl_t m_AttributeName;
    sona::optional<sona::strhdl_t> m_AttributeValue;
    SingleSourceRange m_NameRange;
    SingleSourceRange m_ValueRange;
  };

  AttributeList(std::vector<AttributeList> &attributes)
    : Node(Node::CNK_AttributeList),
      m_Attributes(std::move(attributes)) {}

  std::vector<AttributeList> const& GetAttributes() const noexcept {
    return m_Attributes;
  }

private:
  std::vector<AttributeList> m_Attributes;
};

class Type : public Node {
public:
  Type(NodeKind nodeKind) : Node(nodeKind) {}
};

class Decl : public Node {
public:
  Decl(NodeKind nodeKind) : Node(nodeKind) {}
};

class Stmt : public Node {
public: Stmt(NodeKind nodeKind) : Node(nodeKind) {}
};

class Expr : public Node {
public: Expr(NodeKind nodeKind) : Node(nodeKind) {}
};

class BuiltinType : public Type {
public:
  enum BuiltinTypeId {
    #define BUILTIN_TYPE(name, rep, size, isint, \
                         issigned, signedver, unsignedver) \
      TK_##name,
    #include "Syntax/BuiltinTypes.def"
  };

  BuiltinType(BuiltinTypeId btid, SingleSourceRange const& range)
    : Type(NodeKind::CNK_BuiltinType),
      m_BuiltinTypeId(btid), m_Range(range) {}

  BuiltinTypeId GetBuiltinTypeId() const noexcept { return m_BuiltinTypeId; }

  SingleSourceRange const& GetSourceRange() const noexcept { return m_Range; }

private:
  BuiltinTypeId m_BuiltinTypeId;
  SingleSourceRange m_Range;
};

class UserDefinedType : public Type {
public:
  UserDefinedType(Identifier&& name, SingleSourceRange const& range)
    : Type(NodeKind::CNK_UserDefinedType),
      m_Name(std::move(name)), m_Range(range) {}

  Identifier const& GetName() const noexcept { return m_Name; }

  SingleSourceRange const& GetSourceRange() const noexcept { return m_Range; }

private:
  Identifier m_Name;
  SingleSourceRange m_Range;
};

class TemplatedType : public Type {
public:
  using TemplateArg = sona::either<sona::owner<Type>,
                                      sona::owner<Expr>>;

  TemplatedType(sona::owner<UserDefinedType> &&rootType,
                   std::vector<TemplateArg> &&templateArgs)
    : Type(NodeKind::CNK_TemplatedType),
      m_RootType(std::move(rootType)),
      m_TemplateArgs(std::move(templateArgs)) {}

  sona::ref_ptr<UserDefinedType const> GetRootType() const noexcept {
    return m_RootType.borrow();
  }

  std::vector<TemplateArg> const& GetTemplateArgs() const noexcept {
    return m_TemplateArgs;
  }

private:
  sona::owner<UserDefinedType> m_RootType;
  std::vector<TemplateArg> m_TemplateArgs;
};

class ComposedType : public Type {
public:
  enum TypeSpecifier {
    CTS_Const, CTS_Volatile, CTS_Restrict, CTS_Pointer, CTS_Ref, CTS_RvRef
  };

  ComposedType(sona::owner<Type> rootType,
               std::vector<TypeSpecifier> &&typeSpecifiers,
               std::vector<SingleSourceRange> &&typeSpecRanges)
    : Type(NodeKind::CNK_ComposedType),
      m_RootType(std::move(rootType)),
      m_TypeSpecifiers(typeSpecifiers),
      m_TypeSpecifierRanges(std::move(typeSpecRanges)) {}

  sona::ref_ptr<Type const> GetRootType() const noexcept {
    return m_RootType.borrow();
  }

  std::vector<TypeSpecifier> const& GetTypeSpecifiers() const noexcept {
    return m_TypeSpecifiers;
  }

  std::vector<SingleSourceRange> const&
  GetTypeSpecRanges() const noexcept {
    return m_TypeSpecifierRanges;
  }

private:
  sona::owner<Type> m_RootType;
  std::vector<TypeSpecifier> m_TypeSpecifiers;
  std::vector<SingleSourceRange> m_TypeSpecifierRanges;
};

class Import : public Node {
public:
  Import(Identifier &&importedIdentifier,
         SingleSourceRange const& importRange)
    : Node(NodeKind::CNK_Import),
      m_ImportedIdentifier(std::move(importedIdentifier)),
      m_ImportRange(importRange),
      m_IsWeak(false),
      m_WeakRange(0, 0, 0) {}

  Import(Identifier &&importedIdentifier,
         SingleSourceRange const& importRange,
         std::true_type /* isWeakImport */,
         SingleSourceRange const& weakRange)
    : Node(NodeKind::CNK_Import),
      m_ImportedIdentifier(std::move(importedIdentifier)),
      m_ImportRange(importRange),
      m_IsWeak(true),
      m_WeakRange(weakRange) {}

  Identifier const& GetImportedIdentifier() const noexcept {
    return m_ImportedIdentifier;
  }

  SingleSourceRange const& GetImportSourceRange() const noexcept {
    return m_ImportRange;
  }

  bool IsWeak() const noexcept {
    return m_IsWeak;
  }

  SingleSourceRange const& GetWeakSourceRangeUnsafe() const noexcept {
    sona_assert(IsWeak());
    return m_WeakRange;
  }

private:
  Identifier m_ImportedIdentifier;
  SingleSourceRange m_ImportRange;
  bool m_IsWeak;
  SingleSourceRange m_WeakRange;
};

class Export : public Node {
public:
  Export(sona::owner<Decl> &&node, SingleSourceRange exportRange)
    : Node(NodeKind::CNK_Export), m_Node(std::move(node)),
      m_ExportRange(exportRange) {}

  sona::ref_ptr<Decl const> GetExportedDecl() const noexcept {
    return m_Node.borrow();
  }

  SingleSourceRange const& GetExportSourceRange() const noexcept {
    return m_ExportRange;
  }

private:
  sona::owner<Decl> m_Node;
  SingleSourceRange m_ExportRange;
};

class ForwardDecl : public Decl {
public:
  enum class ForwardDeclKind { FDK_Class, FDK_Enum, FDK_ADT };
  ForwardDecl(ForwardDeclKind fdk, sona::strhdl_t const& name,
                 SingleSourceRange const& keywordRange,
                 SingleSourceRange const& nameRange)
    : Decl(NodeKind::CNK_ForwardDecl),
      m_ForwardDeclKind(fdk), m_Name(name),
      m_KeywordRange(keywordRange),
      m_NameRange(nameRange) {}

  ForwardDeclKind GetFDK() const noexcept {
    return m_ForwardDeclKind;
  }

  sona::strhdl_t const& GetName() const noexcept {
    return m_Name;
  }

  SingleSourceRange const& GetKeywordSourceRange() const noexcept {
    return m_KeywordRange;
  }

  SingleSourceRange const& GetNameSourceRange() const noexcept {
    return m_NameRange;
  }

private:
  ForwardDeclKind m_ForwardDeclKind;
  sona::strhdl_t m_Name;
  SingleSourceRange m_KeywordRange;
  SingleSourceRange m_NameRange;
};

class TemplatedDecl : public Decl {
public:
  TemplatedDecl(
      std::vector<sona::either<sona::strhdl_t, sona::owner<Expr>>> tparams,
      sona::owner<Decl> underlyingDecl,
      SingleSourceRange templateRange)
    : Decl(NodeKind::CNK_TemplatedDecl),
      m_TParams(std::move(tparams)),
      m_UnderlyingDecl(std::move(underlyingDecl)),
      m_TemplateRange(templateRange) {}

  std::vector<sona::either<sona::strhdl_t, sona::owner<Expr>>> const&
  GetTemplateParams() const noexcept {
    return m_TParams;
  }

  sona::ref_ptr<Decl const> GetUnderlyingDecl() const noexcept {
    return m_UnderlyingDecl.borrow();
  }

  SingleSourceRange const& GetTemplateSourceRange() const noexcept {
    return m_TemplateRange;
  }

private:
  std::vector<sona::either<sona::strhdl_t, sona::owner<Expr>>> m_TParams;
  sona::owner<Decl> m_UnderlyingDecl;
  SingleSourceRange m_TemplateRange;
};

class TagDecl : public Decl {
public:
  TagDecl(NodeKind nodeKind, sona::strhdl_t const& name,
          SourceRange nameRange)
    : Decl(nodeKind), m_Name(name), m_NameRange(nameRange) {}

  sona::strhdl_t const& GetName() const noexcept {
    return m_Name;
  }

  SourceRange const& GetNameRange() const noexcept {
    return m_NameRange;
  }

private:
  sona::strhdl_t m_Name;
  SourceRange m_NameRange;
};

class ClassDecl : public TagDecl {
public:
  ClassDecl(sona::strhdl_t const& className,
            std::vector<sona::owner<Decl>> &&subDecls,
            SingleSourceRange const& classKwdRange,
            SingleSourceRange const& classNameRange)
    : TagDecl(NodeKind::CNK_ClassDecl, className, classNameRange),
      m_SubDecls(std::move(subDecls)),
      m_ClassKwdRange(classKwdRange) {}

  auto GetSubDecls() const noexcept {
    return sona::linq::from_container(m_SubDecls).
             transform([](sona::owner<Decl> const& it)
                       { return it.borrow(); });
  }

  SourceRange const& GetKeywordRange() const noexcept {
    return m_ClassKwdRange;
  }

private:
  std::vector<sona::owner<Decl>> m_SubDecls;
  SourceRange m_ClassKwdRange;
};

class EnumDecl : public TagDecl {
public:
  class Enumerator {
  public:
    Enumerator(sona::strhdl_t const& name,
               int64_t value,
               SingleSourceRange nameRange,
               SingleSourceRange eqLoc,
               SingleSourceRange valueRange)
      : m_Name(name), m_Value(value),
        m_NameRange(nameRange), m_EqLoc(eqLoc), m_ValueRange(valueRange) {}

    Enumerator(const sona::strhdl_t &name, SingleSourceRange nameRange)
      : m_Name(name), m_Value(sona::empty_optional()),
        m_NameRange(nameRange), m_EqLoc(0, 0, 0), m_ValueRange(0, 0, 0) {}

    sona::strhdl_t const& GetName() const noexcept {
      return m_Name;
    }

    SingleSourceRange const& GetNameRange() const noexcept {
      return m_NameRange;
    }

    bool HasValue() const noexcept {
      return m_Value.has_value();
    }

    int64_t GetValueUnsafe() const noexcept {
      sona_assert(HasValue());
      return m_Value.value();
    }

    SingleSourceRange const& GetEqRangeUnsafe() const noexcept {
      sona_assert(HasValue());
      return m_EqLoc;
    }

    SingleSourceRange const& GetValueRangeUnsafe() const noexcept {
      sona_assert(HasValue());
      return m_ValueRange;
    }

  private:
    sona::strhdl_t m_Name;
    sona::optional<int64_t> m_Value;
    SingleSourceRange m_NameRange;
    SingleSourceRange m_EqLoc;
    SingleSourceRange m_ValueRange;
  };

  EnumDecl(sona::strhdl_t const& name,
           std::vector<Enumerator> &&enumerators,
           SingleSourceRange const& enumRange,
           SingleSourceRange const& nameRange)
    : TagDecl(NodeKind::CNK_EnumDecl, name, nameRange),
      m_Enumerators(std::move(enumerators)), m_EnumRange(enumRange) {}

  std::vector<Enumerator> const& GetEnumerators() const noexcept {
    return m_Enumerators;
  }

  SingleSourceRange const& GetEnumRange() const noexcept {
    return m_EnumRange;
  }

private:
  std::vector<Enumerator> m_Enumerators;
  SingleSourceRange m_EnumRange;
};

class ADTDecl : public TagDecl {
public:
  class ValueConstructor {
  public:
    ValueConstructor(sona::strhdl_t const& name,
                    sona::owner<Type> &&underlyingType,
                    SingleSourceRange const& nameRange)
      : m_Name(name), m_UnderlyingType(std::move(underlyingType)),
        m_NameRange(nameRange) {}

    ValueConstructor(sona::strhdl_t const& name,
                    SingleSourceRange const& nameRange)
      : m_Name(name), m_UnderlyingType(nullptr), m_NameRange(nameRange) {}

    sona::strhdl_t const& GetName() const noexcept {
      return m_Name;
    }

    sona::ref_ptr<Type const> GetUnderlyingType() const noexcept {
      return m_UnderlyingType.borrow();
    }

    SingleSourceRange const& GetNameRange() const noexcept {
      return m_NameRange;
    }

  private:
    sona::strhdl_t m_Name;
    sona::owner<Type> m_UnderlyingType;
    SingleSourceRange m_NameRange;
  };

  ADTDecl(sona::strhdl_t const& name,
          std::vector<ValueConstructor> &&constructors,
          SingleSourceRange const& enumRange,
          SingleSourceRange const& classRange,
          SingleSourceRange const& nameRange)
    : TagDecl(NodeKind::CNK_ADTDecl, name, nameRange),
      m_Constructors(std::move(constructors)),
      m_EnumRange(enumRange),
      m_ClassRange(classRange) {}

  std::vector<ValueConstructor> const& GetConstructors() const noexcept {
    return m_Constructors;
  }

  SingleSourceRange const& GetEnumRange() const noexcept {
    return m_EnumRange;
  }

  SingleSourceRange const& GetClassRange() const noexcept {
    return m_ClassRange;
  }

private:
  std::vector<ValueConstructor> m_Constructors;
  SingleSourceRange m_EnumRange;
  SingleSourceRange m_ClassRange;
};

class UsingDecl : public Decl {
public:
  UsingDecl(sona::strhdl_t const& name,
            sona::owner<Type> &&aliasee,
            SingleSourceRange usingRange,
            SingleSourceRange nameRange,
            SourceLocation eqLoc)
    : Decl(Node::CNK_UsingDecl),
      m_Name(name),
      m_Aliasee(std::move(aliasee)),
      m_UsingRange(usingRange),
      m_NameRange(nameRange),
      m_EqLoc(eqLoc) {}

  sona::strhdl_t const& GetName() const noexcept {
    return m_Name;
  }

  sona::ref_ptr<Type const> GetAliasee() const noexcept {
    return m_Aliasee.borrow();
  }

  SingleSourceRange const& GetUsingRange() const noexcept {
    return m_UsingRange;
  }

  SingleSourceRange const& GetNameRange() const noexcept {
    return m_NameRange;
  }

  SourceLocation const& GetEqLoc() const noexcept {
    return m_EqLoc;
  }

private:
  sona::strhdl_t m_Name;
  sona::owner<Type> m_Aliasee;
  SingleSourceRange m_UsingRange;
  SingleSourceRange m_NameRange;
  SourceLocation m_EqLoc;
};

class FuncDecl : public Decl {
public:
  FuncDecl(sona::strhdl_t const& name,
           std::vector<sona::owner<Type>> &&paramTypes,
           std::vector<sona::strhdl_t> &&paramNames,
           sona::owner<Type> &&retType,
           sona::optional<sona::owner<Stmt>> &&funcBody,
           SingleSourceRange funcRange,
           SingleSourceRange nameRange) :
    Decl(NodeKind::CNK_FuncDecl),
    m_Name(name),
    m_ParamTypes(std::move(paramTypes)),
    m_ParamNames(std::move(paramNames)),
    m_RetType(std::move(retType)),
    m_FuncBody(std::move(funcBody)),
    m_FuncRange(funcRange), m_NameRange(nameRange) {}

  sona::strhdl_t const& GetName() const noexcept { return m_Name; }

  auto GetParamTypes() const noexcept {
    return sona::linq::from_container(m_ParamTypes).transform(
          [](sona::owner<Type> const& it) { return it.borrow(); });
  }

  std::vector<sona::strhdl_t> const& GetParamNames() const noexcept {
    return m_ParamNames;
  }

  sona::ref_ptr<Type const> GetReturnType() const noexcept {
    return m_RetType.borrow();
  }

  bool IsDefinition() const noexcept {
    return m_FuncBody.has_value();
  }

  sona::ref_ptr<Stmt const> GetFuncBodyUnsafe() const noexcept {
    return m_FuncBody.value().borrow();
  }

  SingleSourceRange const& GetKeywordRange() const noexcept {
    return m_FuncRange;
  }

  SingleSourceRange const& GetNameRange() const noexcept {
    return m_NameRange;
  }

private:
  sona::strhdl_t m_Name;
  std::vector<sona::owner<Type>> m_ParamTypes;
  std::vector<sona::strhdl_t> m_ParamNames;
  sona::owner<Type> m_RetType;
  sona::optional<sona::owner<Stmt>> m_FuncBody;
  SingleSourceRange m_FuncRange, m_NameRange;
};

class VarDecl : public Decl {
public:
  VarDecl(sona::strhdl_t const& name, sona::owner<Type> type,
          SingleSourceRange const& defRange,
          SingleSourceRange const& nameRange)
    : Decl(NodeKind::CNK_VarDecl),
      m_Name(name), m_Type(std::move(type)),
      m_DefRange(defRange), m_NameRange(nameRange) {}

  sona::strhdl_t const& GetName() const noexcept {
    return m_Name;
  }

  sona::ref_ptr<Type const> GetType() const noexcept {
    return m_Type.borrow();
  }

  SingleSourceRange const& GetKeywordRange() const noexcept {
    return m_DefRange;
  }

  SingleSourceRange const& GetNameRange() const noexcept {
    return m_NameRange;
  }

private:
  sona::strhdl_t m_Name;
  sona::owner<Type> m_Type;
  SingleSourceRange m_DefRange, m_NameRange;
};

class LiteralExpr : public Expr {
public:
  LiteralExpr(NodeKind nodeKind, SourceRange const& range)
    : Expr(nodeKind), m_Range(range) {}

  SourceRange GetRange() const noexcept { return m_Range; }

private:
  SourceRange m_Range;
};

class IntLiteralExpr : public LiteralExpr {
public:
  IntLiteralExpr(std::int64_t iValue, SourceRange const& range)
    : LiteralExpr(NodeKind::CNK_IntLiteralExpr, range), m_IntValue(iValue) {}

  std::int64_t GetValue() const noexcept { return m_IntValue; }

private:
  std::int64_t m_IntValue;
};

class UIntLiteralExpr : public LiteralExpr {
public:
  UIntLiteralExpr(std::uint64_t uValue, SourceRange const& range)
    : LiteralExpr(NodeKind::CNK_UIntLiteralExpr, range), m_UIntValue(uValue) {}

  std::uint64_t GetValue() const noexcept { return m_UIntValue; }

private:
  std::uint64_t m_UIntValue;
};

class CharLiteralExpr : public LiteralExpr {
public:
  CharLiteralExpr(char cValue, SourceRange const& range)
    : LiteralExpr(NodeKind::CNK_CharLiteralExpr, range), m_CharValue(cValue) {}

  char GetValue() const noexcept { return m_CharValue; }

private:
  /// @todo use char32_t instead
  /// since a unicode char may be enclosed by single quote
  char m_CharValue;
};

class StringLiteralExpr : public LiteralExpr {
public:
  StringLiteralExpr(sona::strhdl_t const& strValue, SourceRange const& range)
    : LiteralExpr(NodeKind::CNK_StringLiteralExpr, range),
      m_StrValue(strValue) {}

  sona::strhdl_t const& GetValue() const noexcept { return m_StrValue; }

private:
  sona::strhdl_t m_StrValue;
};

class BoolLiteralExpr : public LiteralExpr {
public:
  BoolLiteralExpr(bool bValue, SourceRange const& range)
    : LiteralExpr(NodeKind::CNK_BoolLiteralExpr, range), m_BoolValue(bValue) {}

  bool GetValue() const noexcept { return m_BoolValue; }

private:
  bool m_BoolValue;
};

class FloatLiteralExpr : public LiteralExpr {
public:
  FloatLiteralExpr(double fValue, SourceRange const& range)
    : LiteralExpr(NodeKind::CNK_FloatLiteralExpr, range),
      m_FloatValue(fValue) {}

  double GetValue() const noexcept { return m_FloatValue; }

private:
  double m_FloatValue;
};

class NullLiteralExpr : public LiteralExpr {
public:
  NullLiteralExpr(SourceRange const& range)
    : LiteralExpr(NodeKind::CNK_NullLiteralExpr, range) {}
};

class IdRefExpr : public Expr {
public:
  IdRefExpr(Identifier &&id)
    : Expr(NodeKind::CNK_IdRefExpr), m_Id(std::move(id)) {}

  Identifier const& GetId() const noexcept {
    return m_Id;
  }

private:
  Identifier m_Id;
};

class SizeOfExpr : public Expr {
public:
  SizeOfExpr(sona::owner<Syntax::Expr> &&containedExpr,
             SourceRange const& sizeOfRange)
    : Expr(NodeKind::CNK_SizeOfExpr),
      m_ContainedExpr(std::move(containedExpr)),
      m_SizeOfRange(sizeOfRange) {}

  sona::ref_ptr<Syntax::Expr const> GetContainedExpr() const noexcept {
    return m_ContainedExpr.borrow();
  }

  SourceRange const& GetSizeOfRange() const noexcept {
    return m_SizeOfRange;
  }

private:
  sona::owner<Syntax::Expr> m_ContainedExpr;
  SourceRange m_SizeOfRange;
};

class AlignOfExpr : public Expr {
public:
  AlignOfExpr(sona::owner<Syntax::Expr> &&containedExpr,
              SourceRange const& alignOfRange)
    : Expr(NodeKind::CNK_SizeOfExpr),
      m_ContainedExpr(std::move(containedExpr)),
      m_AlignOfRange(alignOfRange) {}

  sona::ref_ptr<Syntax::Expr const> GetContainedExpr() const noexcept {
    return m_ContainedExpr.borrow();
  }

  SourceRange const& GetAlignOfLocation() const noexcept {
    return m_AlignOfRange;
  }

private:
  sona::owner<Syntax::Expr> m_ContainedExpr;
  SourceRange m_AlignOfRange;
};

class FuncCallExpr : public Expr {
public:
  FuncCallExpr(sona::owner<Expr> &&callee,
               std::vector<sona::owner<Expr>> &&args)
    : Expr(NodeKind::CNK_FuncCallExpr),
      m_Callee(std::move(callee)), m_Args(std::move(args)) {}

  sona::ref_ptr<Expr const> GetCallee() const noexcept {
    return m_Callee.borrow();
  }

  auto GetArgs() const noexcept {
    return sona::linq::from_container(m_Args).
        transform([](sona::owner<Expr> const& e) { return e.borrow(); });
  }

private:
  sona::owner<Expr> m_Callee;
  std::vector<sona::owner<Expr>> m_Args;
};

class ArraySubscriptExpr : public Expr {
public:
  ArraySubscriptExpr(sona::owner<Expr> &&array, sona::owner<Expr> &&index)
    : Expr(NodeKind::CNK_ArraySubscriptExpr),
      m_Array(std::move(array)), m_Index(std::move(index)) {}

  sona::ref_ptr<Expr const> GetArrayPart() const noexcept {
    return m_Array.borrow();
  }

  sona::ref_ptr<Expr const> GetIndexPart() const noexcept {
    return m_Index.borrow();
  }

private:
  sona::owner<Expr> m_Array;
  sona::owner<Expr> m_Index;
};

class MemberAccessExpr : public Expr {
public:
  MemberAccessExpr(sona::owner<Syntax::Expr> &&baseExpr,
                   Syntax::Identifier &&member)
    : Expr(Node::CNK_MemberAccessExpr),
      m_BaseExpr(std::move(baseExpr)), m_Member(std::move(member)) {}

  sona::ref_ptr<Syntax::Expr const> GetBaseExpr() const noexcept {
    return m_BaseExpr.borrow();
  }

  Syntax::Identifier const& GetMember() const noexcept {
    return m_Member;
  }

private:
  sona::owner<Syntax::Expr> m_BaseExpr;
  Syntax::Identifier m_Member;
};

class UnaryAlgebraicExpr : public Expr {
public:
  UnaryAlgebraicExpr(UnaryOperator op, sona::owner<Syntax::Expr> &&baseExpr,
                     SourceRange opRange)
    : Expr(Node::CNK_UnaryAlgebraicExpr),
      m_Operator(op), m_BaseExpr(std::move(baseExpr)),
      m_OpRange(opRange) {}

  UnaryOperator GetOperator() const noexcept { return m_Operator; }

  sona::ref_ptr<Syntax::Expr const> GetBaseExpr() const noexcept {
    return m_BaseExpr.borrow();
  }

  SourceRange const& GetOpRange() const noexcept { return m_OpRange; }

private:
  UnaryOperator m_Operator;
  sona::owner<Syntax::Expr> m_BaseExpr;
  SourceRange m_OpRange;
};

class BinaryExpr : public Expr {
public:
  BinaryExpr(BinaryOperator op, sona::owner<Syntax::Expr> &&lhs,
             sona::owner<Syntax::Expr> &&rhs, SourceRange const& opRange)
    : Expr(Node::CNK_BinaryExpr),
      m_Operator(op), m_LeftHandSide(std::move(lhs)),
      m_RightHandSide(std::move(rhs)), m_OpRange(opRange) {}

  BinaryOperator GetOperator() const noexcept { return m_Operator; }

  sona::ref_ptr<Syntax::Expr const> GetLeftHandSide() const noexcept {
    return m_LeftHandSide.borrow();
  }

  sona::ref_ptr<Syntax::Expr const> GetRightHandSide() const noexcept {
    return m_RightHandSide.borrow();
  }

  SourceRange const& GetOpRange() const noexcept {
    return m_OpRange;
  }

private:
  BinaryOperator m_Operator;
  sona::owner<Syntax::Expr> m_LeftHandSide, m_RightHandSide;
  SourceRange m_OpRange;
};

class AssignExpr : public Expr {
public:
  AssignExpr(AssignOperator op, sona::owner<Syntax::Expr> &&lhs,
             sona::owner<Syntax::Expr> &&rhs, SourceRange const& opRange)
    : Expr(Node::CNK_AssignExpr),
      m_Operator(op), m_LeftHandSide(std::move(lhs)),
      m_RightHandSide(std::move(rhs)), m_OpRange(opRange) {}

  AssignOperator GetOperator() const noexcept { return m_Operator; }

  sona::ref_ptr<Syntax::Expr const> GetLeftHandSide() const noexcept {
    return m_LeftHandSide.borrow();
  }

  sona::ref_ptr<Syntax::Expr const> GetRightHandSide() const noexcept {
    return m_RightHandSide.borrow();
  }

  SourceRange const& GetOpRange() const noexcept {
    return m_OpRange;
  }

private:
  AssignOperator m_Operator;
  sona::owner<Syntax::Expr> m_LeftHandSide, m_RightHandSide;
  SourceRange m_OpRange;
};

class CastExpr : public Expr {
public:
  CastExpr(CastOperator castop, sona::owner<Syntax::Expr> &&castedExpr,
           sona::owner<Syntax::Type> &&destType,
           SourceRange const& castOpRange)
    : Expr(Node::CNK_CastExpr),
      m_CastOp(castop), m_CastedExpr(std::move(castedExpr)),
      m_DestType(std::move(destType)), m_CastOpRange(castOpRange) {}

  CastOperator GetOperator() const noexcept { return m_CastOp; }

  sona::ref_ptr<Syntax::Expr const> GetCastedExpr() const noexcept {
    return m_CastedExpr.borrow();
  }

  sona::ref_ptr<Syntax::Type const> GetDestType() const noexcept {
    return m_DestType.borrow();
  }

  SourceRange const& GetCastOpRange() const noexcept {
    return m_CastOpRange;
  }

private:
  CastOperator m_CastOp;
  sona::owner<Syntax::Expr> m_CastedExpr;
  sona::owner<Syntax::Type> m_DestType;
  SourceRange m_CastOpRange;
};

class MixFixExpr : public Expr {};

class TransUnit : public Node {
public:
  TransUnit() : Node(NodeKind::CNK_TransUnit) {}

  void Declare(sona::owner<Decl> &&decl) {
    m_Decls.push_back(std::move(decl));
  }

  void DoImport(sona::owner<Import> &&import) {
    m_Imports.push_back(std::move(import));
  }

  auto GetDecls() const noexcept {
    return sona::linq::from_container(m_Decls).
        transform([](sona::owner<Decl> const& decl)
          { return decl.borrow(); } );
  }

  auto GetImports() const noexcept {
    return sona::linq::from_container(m_Imports).
        transform([](sona::owner<Import> const& decl)
          { return decl.borrow(); } );
  }

private:
  std::vector<sona::owner<Decl>> m_Decls;
  std::vector<sona::owner<Import>> m_Imports;
};

} // namespace Syntax;
} // namespace ckx

#endif // CONCRETE_H
