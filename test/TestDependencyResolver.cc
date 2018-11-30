#define TEST
#include "Meta/Dependency.h"
#include <iostream>

using namespace std;
using namespace sona;
using namespace ckx::Meta;

void test0() {
  cout << "-- running test0" << endl;

  string mod0 = "support";
  string mod1 = "chrono";
  string mod2 = "container";
  string mod3 = "filesystem";
  string mod4 = "hadoop";

  vector<string> mod0req;
  vector<string> mod1req { mod0 };
  vector<string> mod2req { mod0 };
  vector<string> mod3req { mod1, mod2 };
  vector<string> mod4req { mod1, mod2, mod3 };

  DependInfo mod0depinfo(mod0, std::move(mod0req));
  DependInfo mod1depinfo(mod1, std::move(mod1req));
  DependInfo mod2depinfo(mod2, std::move(mod2req));
  DependInfo mod3depinfo(mod3, std::move(mod3req));
  DependInfo mod4depinfo(mod4, std::move(mod4req));

  vector<ref_ptr<DependInfo>> deplist {
    &mod4depinfo, &mod1depinfo, &mod3depinfo, &mod2depinfo, &mod0depinfo
  };

  optional<vector<ref_ptr<DependInfo>>>
    resolvedDeplist = ResolveDependency(deplist);

  if (resolvedDeplist.has_value()) {
    for (ref_ptr<DependInfo> dpinfo : resolvedDeplist.value()) {
      cout << dpinfo.get().GetExportedName() << endl;
    }
  }
}

void test1() {
  cout << "-- running test1" << endl;

  string mod0 = "fork";
  string mod1 = "knife";

  vector<string> mod0req { mod1 };
  vector<string> mod1req { mod0 };

  DependInfo mod0depinfo(mod0, std::move(mod0req));
  DependInfo mod1depinfo(mod1, std::move(mod1req));

  vector<ref_ptr<DependInfo>> deplist { &mod1depinfo, &mod0depinfo };

  optional<vector<ref_ptr<DependInfo>>>
    resolvedDeplist = ResolveDependency(deplist);

  if (resolvedDeplist.has_value()) {
    for (ref_ptr<DependInfo> dpinfo : resolvedDeplist.value()) {
      cout << dpinfo.get().GetExportedName() << endl;
    }
  }
  else {
    cout << "no solution!" << endl;
  }
}

int main() {
  test0();
  test1();
  return 0;
}
