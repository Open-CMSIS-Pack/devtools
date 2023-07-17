#ifndef RteCondition_H
#define RteCondition_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteCondition.h
* @brief CMSIS RTE Data Model
*/
/******************************************************************************/
/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/******************************************************************************/
#include "RteItem.h"

class RteTarget;
class RteCondition;
class RteComponent;
class RteComponentAggregate;
class RtePackage;
class RteDependencyResult;
class RteConditionContext;

// constants for expression domain

#define DEVICE_EXPRESSION 'D'
#define BOARD_EXPRESSION 'B'
#define HW_EXPRESSION 'H'
#define TOOLCHAIN_EXPRESSION 'T'
#define COMPONENT_EXPRESSION 'C'
#define CONDITION_EXPRESSION 'c'
#define ERROR_EXPRESSION 'E'

#define VERBOSE_FILTER 0x02
#define VERBOSE_DEPENDENCY 0x04

/**
 * @brief Abstract base class for accept, require and deny condition expressions
*/
class RteConditionExpression : public RteItem
{
  friend class RteConditionContext; // condition context should be able to call protected members of this class

public:
  /**
   * @brief expression type corresponding its tag
  */
  enum RTEConditionExpressionType {
    ACCEPT,  // <accept> expression
    REQUIRE, // <require> expression
    DENY     // <deny> expression
  };

  /**
   * @brief constructor
   * @param parent pointer to RteCondition parent
  */
  RteConditionExpression(RteCondition* parent);

  /**
   * @brief constructor
   * @param tag expression tag
   * @param parent pointer to RteCondition parent
  */
  RteConditionExpression(const std::string& tag, RteCondition* parent = nullptr);

  /**
   * @brief virtual destructor
  */
   ~RteConditionExpression() override;

  /**
   * @brief get expression type. The base method is abstract.
   * @return RTEConditionExpressionType value
  */
  virtual RTEConditionExpressionType GetExpressionType() const = 0;

  /**
   * @brief check if this expression describes a component dependency
   * @return true if this expression contain only attributes with 'C' prefix
  */
  bool IsDependencyExpression() const;

  /**
   * @brief check if this expression contains device attributes
   * @return true if this expression contain attributes with 'D' prefix
  */
  bool IsDeviceExpression() const;

  /**
   * @brief check if this expression contains board attributes
   * @return true if this expression contain attributes with 'B' prefix
  */
  bool IsBoardExpression() const;

  /**
   * @brief check if this expression is a <deny> one
   * @return true if deny expression
  */
  bool IsDenyExpression() const;

  /**
   * @brief check if this expression is dependent on selected device
   * @return true if this is a device expression that contains non-empty "Dname" attribute
  */
   bool IsDeviceDependent() const override;

  /**
   * @brief check if this expression is dependent on selected board
   * @return true if this is a device expression that contains non-empty "Bname" attribute
  */
   bool IsBoardDependent() const override;

  /**
   * @brief get expression domain : Device, Toolchain, Component dependency or a reference to a condition
   * @return expression domain as char value: 'B', 'D', 'H', 'T', 'C', or 'c'
  */
  char GetExpressionDomain() const { return m_domain; }
  /**
   * @brief get component aggregates matching expression attributes
   * @param target pointer to RteTarget specifying condition context and available components
   * @return set of pointers to RteComponentAggregate matching this expression for specified target
  */
  const std::set<RteComponentAggregate*>& GetComponentAggregates(RteTarget* target) const;

  /**
   * @brief get pointer to a single RteComponentAggregate matching by this expression. The result is used to resolve component dependencies automatically.
   * @param target pointer to RteTarget specifying condition context and available components
   * @return pointer to one and the only one RteComponentAggregate in the collection of matching component aggregates
  */
  RteComponentAggregate* GetSingleComponentAggregate(RteTarget* target) const;

  /**
   * @brief static helper method to get a single component aggregate from the supplied collection
   * @param target pointer to RteTarget specifying condition context and available components
   * @param components set of pointers to RteComponentAggregate items to evaluate
   * @return pointer to one and the only one RteComponentAggregate in the collection of matching component aggregates
  */
  static RteComponentAggregate* GetSingleComponentAggregate(RteTarget* target, const std::set<RteComponentAggregate*>& components);

public:
  /**
   * @brief construct expression ID
   * @return expression ID constructed out of its tag and attributes
  */
   std::string ConstructID() override;

  /**
   * @brief validate expression after creation: attributes of different domains may not be mixed and
   * @return true if validation is passed
  */
   bool Validate() override;

public:
  /**
   * @brief get string to display expression to user
   * @return expression ID
  */
   std::string GetDisplayName() const override;

  /**
   * @brief evaluate the expression for given context
   * @param context pointer to RteConditionContext to evaluate
   * @return evaluation result as RteItem::ConditionResult value
  */
   ConditionResult Evaluate(RteConditionContext* context) override;
  /**
   * @brief get cached evaluation result for given context
   * @param context pointer to RteConditionContext to obtain cached value from
   * @return evaluation result as RteItem::ConditionResult value
  */
   ConditionResult GetConditionResult(RteConditionContext* context) const override;

  /**
   * @brief collect cached results of evaluating component dependencies by this item
   * @param results map to collect results
   * @param target RteTarget associated with condition context
   * @return overall RteItem::ConditionResult value for this item
  */
   ConditionResult GetDepsResult(std::map<const RteItem*, RteDependencyResult>& results, RteTarget* target) const override;

protected:
  /**
   * @brief method called by RteConditionContext to make actual evaluation
   * @param target pointer to RteTarget specifying condition context and available components
   * @return  evaluation result as RteItem::ConditionResult value
  */
  virtual ConditionResult EvaluateExpression(RteTarget* target);

  /**
   * @brief method is called by RteConditionContext check if component dependency has been already evaluated for this expression
   * @param results map of already evaluated results
   * @return true if supplied map contains entry with key pointing to this or to another expression with the same tag and attributes
  */
  bool HasDepsResult(std::map<const RteItem*, RteDependencyResult>& results) const;

protected:
  char m_domain; // expression domain

public:
  static const std::string ACCEPT_TAG;
  static const std::string DENY_TAG;
  static const std::string REQUIRE_TAG;
};

/**
 * @brief class to describe and evaluate <accept> expression
*/
class RteAcceptExpression : public RteConditionExpression
{
public:
  /**
    * @brief constructor
    * @param parent pointer to RteCondition parent
  */
  RteAcceptExpression(RteCondition* parent) :RteConditionExpression(ACCEPT_TAG, parent) {};

public:
  /**
   * @brief get expression type
   * @return RteConditionExpression::ACCEPT
  */
   RTEConditionExpressionType GetExpressionType() const override { return RteConditionExpression::ACCEPT; }
};

/**
 * @brief class to describe <require> expression
*/
class RteRequireExpression : public RteConditionExpression
{
public:
  /**
    * @brief constructor
    * @param parent pointer to RteCondition parent
  */
  RteRequireExpression(RteCondition* parent) :RteConditionExpression(REQUIRE_TAG, parent) {};

public:
  /**
   * @brief get expression type
   * @return RteConditionExpression::REQUIRE
  */
   RTEConditionExpressionType GetExpressionType() const override { return RteConditionExpression::REQUIRE; }
};

/**
 * @brief class to describe <deny> expression
*/
class RteDenyExpression : public RteConditionExpression
{
public:
  /**
    * @brief constructor
    * @param parent pointer to RteCondition parent
  */
  RteDenyExpression(RteCondition* parent) :RteConditionExpression(DENY_TAG, parent) {};

public:
  /**
   * @brief get expression type
   * @return RteConditionExpression::DENY
  */
   RTEConditionExpressionType GetExpressionType() const override { return RteConditionExpression::DENY; }

  /**
    * @brief evaluate the expression for given context. The method call base class implementation and negates its result
    * @param context pointer to RteConditionContext to evaluate
    * @return evaluation result as RteItem::ConditionResult value
  */
   ConditionResult Evaluate(RteConditionContext* context) override;

};

/**
 * @brief class to support CMSIS-pack conditions. Corresponds to <condition> element in pdsc file
*/
class RteCondition : public RteItem
{
public:
  /**
    * @brief constructor
    * @param parent pointer to RteCondition parent
  */
  RteCondition(RteItem* parent);

  /**
   * @brief virtual destructor
  */
   ~RteCondition() override;

  /**
   * @brief calculate device and board dependency flags by recursively checking all expressions: at least one expression in this or referenced conditions is dependent
  */
  void CalcDeviceAndBoardDependentFlags();

public:
  /**
   * @brief get parent condition, override from RteItem
   * @return return this
  */
   RteCondition* GetCondition() const override;

  /**
   * @brief get condition with given ID
   * @param id condition ID
   * @return this if id equals this ID, otherwise one found in parent container
  */
   RteCondition* GetCondition(const std::string& id) const override; // returns this if id == this->id
  /**
   * @brief return condition name
   * @return value of "id" attribute
  */
   const std::string& GetName() const override;

  /**
   * @brief get condition name to display to user
   * @return display name combined from tag ("condition") and ID
  */
   std::string GetDisplayName() const override;

public:
  /**
   * @brief validate condition after construction
   * @return true if all child expressions are valid and no recursion is detected
  */
   bool Validate() override;
  /**
   * @brief check if condition is device depended i.e at least one expression in this or referenced conditions is device dependent
   * @return cached device dependency flag
  */
   bool IsDeviceDependent() const override;

  /**
   * @brief check if condition is board depended i.e at least one expression in this or referenced conditions is board dependent
   * @return cached board dependency flag
  */
   bool IsBoardDependent() const override;

public:
  /**
  * @brief collect cached results of evaluating component dependencies by this item
  * @param results map to collect results
  * @param target RteTarget associated with condition context
  * @return overall RteItem::ConditionResult value for this item
 */
   ConditionResult GetDepsResult(std::map<const RteItem*, RteDependencyResult>& results, RteTarget* target) const override;

  /**
    * @brief evaluate the condition for given context
    * @param context pointer to RteConditionContext to evaluate
    * @return evaluation result as RteItem::ConditionResult value
   */
   ConditionResult Evaluate(RteConditionContext* context) override;

  /**
   * @brief get cached evaluation result for given context
   * @param context pointer to RteConditionContext to obtain cached value from
   * @return evaluation result as RteItem::ConditionResult value
  */
   ConditionResult GetConditionResult(RteConditionContext* context) const override;

   /**
    * @brief get static verbosity flags
    * @return verbosity flags as unsigned integer
   */
   static unsigned GetVerboseFlags() { return s_uVerboseFlags; }

   /**
    * @brief set static verbosity flags
    * @param flags verbosity flags to set
   */
   static void SetVerboseFlags(unsigned flags ) { s_uVerboseFlags = flags; }

private:
  /**
    * @brief evaluate this condition, called from Evaluate()
    * @param context pointer to RteConditionContext
    * @return evaluation result as RteItem::ConditionResult value
   */
  ConditionResult EvaluateCondition(RteConditionContext* context);
  /**
   * @brief check if the condition is being evaluated under given context for recursion protection
   * @param context pointer to RteConditionContext
   * @return true if condition is been evaluated
  */
  bool IsEvaluating(RteConditionContext* context) const;
  /**
   * @brief check if condition has direct or indirect recursion
   * @return true if no recursion is detected
  */
  bool ValidateRecursion();

 /**
   * @brief create a new instance of type RteItem
   * @param tag name of tag
   * @return pointer to instance of type RteItem
 */
  RteItem* CreateItem(const std::string& tag) override;

protected:

 /**
   * @brief set if this condition is under evaluation for given context (recursion protection)
   * @param context pointer to RteConditionContext
   * @param evaluating: true before valuating, false after evaluating
  */
  void SetEvaluating(RteConditionContext* context, bool evaluating);

private:
  int m_bDeviceDependent; // cached device dependency flag
  int m_bBoardDependent; // cached board dependency flag
  bool m_bInCheck; // recursion protection flag for CalcDeviceAndBoardDependentFlags() and  ValidateRecursion()
  std::set<RteConditionContext*> m_evaluating; // recursion protection for Evaluate(RteConditionContext*)
  static unsigned s_uVerboseFlags;
};

/**
 * @brief class to process <conditions> element in *.pdsc files
*/
class RteConditionContainer : public RteItem
{
public:

  /**
    * @brief constructor
    * @param parent pointer to RteCondition parent
   */
  RteConditionContainer(RteItem* parent);


  /**
   * @brief create a new instance of type RteItem
   * @param tag name of tag
   * @return pointer to instance of type RteItem
  */
  RteItem* CreateItem(const std::string& tag) override;
};

/**
 * @brief helper class to keep and present result of evaluating component dependencies
*/
class RteDependencyResult
{
public:

  /**
   * @brief default constructor
   * @param item pointer to RteItem this result refers to (an originating component, API, condition or condition expression), nullptr for top-level result
   * @param result RteItem::ConditionResult to set initially, default is RteItem::UNDEFINED
  */
  RteDependencyResult(const RteItem* item = nullptr, RteItem::ConditionResult result = RteItem::UNDEFINED);
  /**
   * @brief virtual destructor
  */
  virtual ~RteDependencyResult();

  /**
   * @brief clear stored results
  */
  void Clear();

  /**
   * @brief get referenced item
   * @return pointer to RteItem
  */
  const RteItem* GetItem() const { return m_item; }
  /**
   * @brief get result of condition evaluation
   * @return RteItem::ConditionResult
  */
  RteItem::ConditionResult GetResult() const { return m_result; }

  /**
   * @brief get collected component aggregates as possible candidates to resolve a dependency
   * @return set of RteComponentAggregate pointers
  */
  const std::set<RteComponentAggregate*>& GetComponentAggregates() const { return m_aggregates; }
  /**
   * @brief get dependency results of sub-items
   * @return map of RteItem pointer to RteDependencyResult pairs
  */
  const std::map<const RteItem*, RteDependencyResult>& GetResults() const { return m_results; }

  /**
   * @brief return display name to present to user in validation window
   * @return display name string depending on level
  */
  std::string GetDisplayName() const;
  /**
   * @brief get message text to display in validation widow and in a console
   * @return message text depending on level and evaluation result
  */
  std::string GetMessageText() const;
  /**
   * @brief get severity of the evaluated result
   * @return severity as string: "error",  "warning" or empty string
  */
  std::string GetSeverity() const;
  /**
   * @brief get internal error number depending on stored evaluation result
   * @return error number as string corresponding stored RteItem::ConditionResult value
  */
  std::string GetErrorNum() const;

  /**
   * @brief get formatted output message to present to a console
   * @return formatted message text depending on evaluation result
  */
  std::string GetOutputMessage() const;

  /**
   * @brief check if this result contains multiple selection options due to parallel accept expressions
   * @return true if multiple
  */
  bool IsMultiple() const {return m_bMultiple;}

  /**
   * @brief set this result contains multiple selection options due to parallel accept expressions
   * @return set/reset flag that the result contains multiple selection options due to parallel accept expressions
  */
  void SetMultiple(bool bMuiltiple) { m_bMultiple = bMuiltiple; }


public:
  /**
   * @brief add RteComponentAggregate to list of potential candidates to resolve a dependency
   * @param a
  */
  void AddComponentAggregate(RteComponentAggregate* a);
  /**
   * @brief set condition evaluation result
   * @param res RteItem::ConditionResult value to set
  */
  void SetResult(RteItem::ConditionResult res) { m_result = res; }

  /**
   * @brief get mutable collection of child results
   * @return reference to internal mutable map of RteItem pointer to RteDependencyResult pairs
  */
  std::map<const RteItem*, RteDependencyResult>& Results() { return m_results; }

  /**
   * @brief helper static function to obtain evaluation result for given item from supplied collection of results
   * @param item pointer to RteItem to search for
   * @param results  map of RteItem pointer to RteDependencyResult pairs
   * @return RteItem::ConditionResult value, RteItem::UNDEFINED if not found
  */
  static RteItem::ConditionResult GetResult(const RteItem* item, const std::map<const RteItem*, RteDependencyResult>& results);

private:
  bool m_bMultiple; // flag indication that parallel accept expressions also contain components to select
  const RteItem* m_item; // an originating component, API, condition or condition expression
  RteItem::ConditionResult m_result; // overall condition result of this item and sub-items
  std::set<RteComponentAggregate*> m_aggregates; // multiple components in API, available or incompatible in expressions
  std::map<const RteItem*, RteDependencyResult> m_results; // sub-items (condition expression)
};

/**
 * @brief base class to provide context for condition evaluation: filtering (this base) or resolving component dependencies (derived class)
*/
class RteConditionContext
{
public:
  /**
   * @brief constructor
   * @param target pointer to RteTarget that owns this context and provides filtering attributes as well as methods to search for components
  */
  RteConditionContext(RteTarget* target);
  /**
   * @brief virtual destructor
  */
  virtual ~RteConditionContext();

  /**
   * @brief check if this context calculates/solves dependencies
   * @return the base returns false
  */
  virtual bool IsDependencyContext() const { return false; }

  /**
   * @brief get RteTarget that owns this context and provides filtering attributes as well as methods to search for components
   * @return pointer to RteTarget
  */
  RteTarget* GetTarget() const { return m_target; }

  /**
   * @brief get overall evaluation result
   * @return RteItem::ConditionResult value
  */
  RteItem::ConditionResult GetConditionResult() const { return m_result; }

  /**
   * @brief get condition result for specified item
   * @param item pointer to RteItem to search for
   * @return cached RteItem::ConditionResult, RteItem::UNDEFINED if not found
  */
  RteItem::ConditionResult GetConditionResult(RteItem* item) const; // returns cached result

  /**
   * @brief clear internal data and caches
  */
  virtual void Clear();

  /**
   * @brief check if this context is verbose
  */
  virtual bool IsVerbose() const;

  /**
   * @brief evaluate item if not yet done
   * @param item pointer RteItem to evaluate (RteComponent, RteFile, RteCondition, RteConditionExpression)
   * @return result of item evaluation as RteItem::ConditionResult value
  */
  virtual RteItem::ConditionResult Evaluate(RteItem* item);

  /**
   * @brief evaluate supplied condition, called from supplied condition
   * @param condition pointer to RteCondition to evaluate
   * @return evaluation result as RteItem::ConditionResult value
  */
  virtual RteItem::ConditionResult EvaluateCondition(RteCondition* condition);
  /**
   * @brief evaluate expression condition, called from supplied condition expression
   * @param condition pointer to RteConditionExpression to evaluate
   * @return evaluation result as RteItem::ConditionResult value
  */
  virtual RteItem::ConditionResult EvaluateExpression(RteConditionExpression* expr);

protected:
  void virtual VerboseIn(RteItem* item);
  void virtual VerboseOut(RteItem* item, RteItem::ConditionResult res);

protected:
  RteTarget* m_target; // owning target
  RteItem::ConditionResult m_result; // overall result
  std::map<RteItem*, RteItem::ConditionResult> m_cachedResults; // collection of cached results
  unsigned m_verboseIndent;
};


/**
 * @brief class to provide context for resolving component dependencies
*/
class RteDependencySolver : public RteConditionContext
{
public:

  /**
   * @brief constructor
   * @param target pointer to RteTarget that owns this context and provides filtering attributes as well as methods to search for components
  */
  RteDependencySolver(RteTarget* target);
  /**
   * @brief virtual destructor
  */
   ~RteDependencySolver() override;

   /**
    * @brief check if this context calculates/solves dependencies
    * @return true
   */
   bool IsDependencyContext() const override { return true; }


  /**
    * @brief clear internal data and caches
   */
   void Clear() override;

  /**
    * @brief check if this context is verbose
   */
   bool IsVerbose() const override;

  /**
  * @brief evaluate supplied condition, called from supplied condition
  * @param condition pointer to RteCondition to evaluate
  * @return evaluation result as RteItem::ConditionResult value
  */
   RteItem::ConditionResult EvaluateCondition(RteCondition* condition) override;
  /**
   * @brief evaluate expression condition, called from supplied condition expression
   * @param condition pointer to RteConditionExpression to evaluate
   * @return evaluation result as RteItem::ConditionResult value
  */
   RteItem::ConditionResult EvaluateExpression(RteConditionExpression* expr) override;

  /**
   * @brief get cached component aggregates for supplied expression
   * @param target pointer to RteConditionExpression to get aggregates for
   * @return set of pointers to RteComponentAggregate matching supplied expression
  */
  const std::set<RteComponentAggregate*>& GetComponentAggregates(RteConditionExpression* expr) const;

  /**
   * @brief evaluate component dependencies.
   * @return evaluation result as RteItem::ConditionResult value
  */
  RteItem::ConditionResult EvaluateDependencies();

  /**
   * @brief try to resolve component dependencies by selecting n collected during evaluation.
   * Resolves only on-ambiguous component aggregates for condition expressions with RteItem::SELECTABLE result.
   * Calls EvaluateDependencies() after each iteration.
   * Stops when no expression with RteItem::SELECTABLE result is left
   * @return evaluation result as RteItem::ConditionResult value
  */
  RteItem::ConditionResult ResolveDependencies();

protected:
  /**
   * @brief evaluate component dependencies for given expression and sores potential component aggregates
   * @param expr pointer to RteConditionExpression to evaluate
   * @return evaluation result as RteItem::ConditionResult value
  */
  RteItem::ConditionResult CalculateDependencies(RteConditionExpression* expr);

  /**
   * @brief perform single resolve iteration:
   * iterates over cached RteItem::SELECTABLE dependency results
   * selects component that triggers call EvaluatDependencies()
   * @return true if at least one component got selected
  */
  bool ResolveIteration();

  /**
   * @brief try to resolve single dependency
   * @param depsRes RteDependencyResult to resolve
   * @return true if resolve
  */
  bool ResolveDependency(const RteDependencyResult& depsRes);


protected:
  std::map<RteConditionExpression*, std::set<RteComponentAggregate*> > m_componentAggregates; // cached component aggregates per expression
};

#endif // RteCondition_H
