/*
 * Copyright (c) 2025 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef RPCINTERFACE_H
#define RPCINTERFACE_H

#include <jsonrpccxx/server.hpp>
#include <optional>
#include <string>
#include <vector>

using namespace std;
using namespace jsonrpccxx;

namespace Args {

 struct PackElement {
    string id;
    optional<string> description;
    optional<string> doc;
  };

  // GetPacksInfo
  struct Pack : public PackElement {
    optional<string> overview;
    optional<bool> used;
    optional<vector<string>> references;
  };
  struct PacksInfo {
    vector<Pack> packs;
  };

  // GetCoponentTree
  struct Component : public PackElement {
    string fromPack;
    optional<string> implements;
    optional<int> maxInstances;
  };

  using Api = PackElement;
  using Taxonomy = PackElement;
  using Bundle = PackElement;

  // component Tree
  struct CtItem {
    string name;
  };

  struct ComponentInstance {
    string id;
    int selectedCount; // number of selected instances
    optional<Component> resolvedComponent;
    optional<string> layer;
  };

  struct CtVariant : public CtItem {
    vector<Component> components; // version-sorted components
  };

  struct CtAggregate : public CtItem {
    string id;
    optional<string> activeVariant;
    optional<string> activeVersion;
    optional<int> selectedCount; // selection flag, represents selected number of instances
    vector<CtVariant> variants;
    optional<string> layer;
  };

  struct CtGroup;
  struct CtAggregate;
  struct CtTreeItem : public CtItem {
    optional<vector<CtGroup>> groups;
    optional<vector<CtAggregate>> aggregates;
  };

  struct CtGroup : public CtTreeItem {
    optional<Api> api;
    optional<Taxonomy> taxonomy;
  };

  struct CtBundle : public CtTreeItem {
    optional<Bundle> bundle;
  };

  struct CtClass : public CtItem{
    optional<Taxonomy> taxonomy;
    optional<string> activeBundle;
    vector<CtBundle> bundles;
  };

  struct CtRoot {
    vector<CtClass> classes;
  };

  // ValidateComponents
  struct Condition {
    string expression;
    optional<vector<string>> aggregates;
  };
  struct Result {
    string result;
    string id;
    optional<vector<string>> aggregates;
    optional<vector<Condition>> conditions;
  };
  struct Results {
    optional<vector<Result>> validation;
  };

  struct UsedItems {
    vector<ComponentInstance> components;
    vector<Pack> packs;
  };

  // GetLogMessages
  struct LogMessages {
    optional<vector<string>> info;
    optional<vector<string>> errors;
    optional<vector<string>> warnings;
  };

  // to_json converters
  template<class T> void to_json(nlohmann::json& j, const string& key, const T& value) {
    j[key] = value;
  }
  template<class T> void to_json(nlohmann::json& j, const string& key, const optional<T>& opt) {
    if (opt.has_value()) {
      j[key] = opt.value();
    }
  }
  inline void to_json(nlohmann::json& j, const Pack& p) {
    to_json(j, "id", p.id);
    to_json(j, "description", p.description);
    to_json(j, "overview", p.overview);
    to_json(j, "used", p.used);
    to_json(j, "references", p.references);
  }
  inline void to_json(nlohmann::json& j, const PacksInfo& info) {
    to_json(j, "packs", info.packs);
  }

  inline void to_json(nlohmann::json& j, const Component& c) {
    to_json(j, "id", c.id);
    to_json(j, "description", c.description);
    to_json(j, "doc", c.doc);
    to_json(j, "from-pack", c.fromPack);
    to_json(j, "implements", c.implements);
    to_json(j, "maxInstances", c.maxInstances);
  }
  inline void to_json(nlohmann::json& j, const PackElement& e) {
    to_json(j, "id", e.id);
    to_json(j, "description", e.description);
    to_json(j, "doc", e.doc);
  }

  inline void to_json(nlohmann::json& j, const ComponentInstance& ci) {
    to_json(j, "id", ci.id);
    to_json(j, "selectedCount", ci.selectedCount);
    to_json(j, "layer", ci.layer);
    to_json(j, "resolvedComponent", ci.resolvedComponent);
  }

  inline void to_json(nlohmann::json& j, const Condition& c) {
    to_json(j, "expression", c.expression);
    to_json(j, "aggregates", c.aggregates);
  }
  inline void to_json(nlohmann::json& j, const Result& r) {
    to_json(j, "result", r.result);
    to_json(j, "id", r.id);
    to_json(j, "aggregates", r.aggregates);
    to_json(j, "conditions", r.conditions);
  }
  inline void to_json(nlohmann::json& j, const Results& r) {
    to_json(j, "validation", r.validation);
  }

  inline void to_json(nlohmann::json& j, const LogMessages& m) {
    to_json(j, "info", m.info);
    to_json(j, "errors", m.errors);
    to_json(j, "warnings", m.warnings);
  }

  inline void to_json(nlohmann::json& j, const CtVariant& v) {
    to_json(j, "name", v.name);
    to_json(j, "components", v.components);
  }

  inline void to_json(nlohmann::json& j, const CtAggregate& a) {
    to_json(j, "name", a.name);
    to_json(j, "id", a.id);
    to_json(j, "selectedCount", a.selectedCount);
    to_json(j, "activeVariant", a.activeVariant);
    to_json(j, "activeVersion", a.activeVersion);
    to_json(j, "layer", a.layer);
    to_json(j, "variants", a.variants);
  }

  inline void to_json(nlohmann::json& j, const CtGroup& g) {
    to_json(j, "name", g.name);
    to_json(j, "api", g.api);
    to_json(j, "taxonomy", g.taxonomy);
    to_json(j, "groups", g.groups);
    to_json(j, "aggregates", g.aggregates);
  }

  inline void to_json(nlohmann::json& j, const CtBundle& b) {
    to_json(j, "name", b.name);
    to_json(j, "bundle", b.bundle);
    to_json(j, "groups", b.groups);
    to_json(j, "aggregates", b.aggregates);
  }

  inline void to_json(nlohmann::json& j, const CtClass& c) {
    to_json(j, "name", c.name);
    to_json(j, "activeBundle", c.activeBundle);
    to_json(j, "bundles", c.bundles);
    to_json(j, "taxonomy", c.taxonomy);
  }

  inline void to_json(nlohmann::json& j, const UsedItems& u) {
    to_json(j, "components", u.components);
    to_json(j, "packs", u.packs);
  }

  inline void to_json(nlohmann::json& j, const CtRoot& r) {
    to_json(j, "classes", r.classes);
  }
}

class RpcMethods {
public:
  RpcMethods(JsonRpc2Server& jsonServer) {
    jsonServer.Add("GetVersion", GetHandle(&RpcMethods::GetVersion, *this));
    jsonServer.Add("Shutdown", GetHandle(&RpcMethods::Shutdown, *this));
    jsonServer.Add("Apply", GetHandle(&RpcMethods::Apply, *this), { "context" });
    jsonServer.Add("LoadPacks", GetHandle(&RpcMethods::LoadPacks, *this));
    jsonServer.Add("LoadSolution", GetHandle(&RpcMethods::LoadSolution, *this), { "solution" });
    jsonServer.Add("GetPacksInfo", GetHandle(&RpcMethods::GetPacksInfo, *this), { "context" });
    jsonServer.Add("GetUsedItems", GetHandle(&RpcMethods::GetUsedItems, *this), { "context" });
    jsonServer.Add("GetComponentsTree", GetHandle(&RpcMethods::GetComponentsTree, *this), { "context", "all" });
    jsonServer.Add("SelectComponent", GetHandle(&RpcMethods::SelectComponent, *this), { "context", "id", "count" });
    jsonServer.Add("SelectVariant", GetHandle(&RpcMethods::SelectVariant, *this), { "context", "id", "variant" });
    jsonServer.Add("SelectVersion", GetHandle(&RpcMethods::SelectVersion, *this), { "context", "id", "version" });
    jsonServer.Add("SelectBundle", GetHandle(&RpcMethods::SelectBundle, *this), { "context", "class", "bundle" });
    jsonServer.Add("ValidateComponents", GetHandle(&RpcMethods::ValidateComponents, *this), { "context" });
    jsonServer.Add("GetLogMessages", GetHandle(&RpcMethods::GetLogMessages, *this));
  }
  virtual const string GetVersion(void) { return string(); }
  virtual const bool Shutdown(void) { return bool(); }
  virtual const bool Apply(const string& context) { return bool(); }
  virtual const bool LoadPacks(void) { return bool(); }
  virtual const bool LoadSolution(const string& solution) { return bool(); }
  virtual const Args::UsedItems GetUsedItems(const string& context) { return Args::UsedItems(); }
  virtual const Args::PacksInfo GetPacksInfo(const string& context) { return Args::PacksInfo(); }
  virtual const Args::CtRoot GetComponentsTree(const string& context, bool all) { return Args::CtRoot(); }
  virtual const bool SelectComponent(const string& context, const string& aggregateId, int count) { return bool(); }
  virtual const bool SelectVariant(const string& context, const string& aggregateId, const string& variantName) { return bool(); }
  virtual const bool SelectVersion(const string& context, const string& aggregateId, const string& version) { return bool(); }
  virtual const bool SelectBundle(const string& context, const string& className, const string& bundleName) { return bool(); }
  virtual const Args::Results ValidateComponents(const string& context) { return Args::Results(); }
  virtual const Args::LogMessages GetLogMessages(void) { return Args::LogMessages(); }
};

#endif // RPCINTERFACE_H
