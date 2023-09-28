#ifndef RteDevice_H
#define RteDevice_H
/******************************************************************************/
/* RTE - CMSIS Run-Time Environment */
/******************************************************************************/
/** @file RteDeviceItem.h
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
#include "RteItem.h"
#include "RtePackage.h"

class RteDeviceItem;
class RteDeviceProperty;
typedef std::map<std::string, std::list<RteDeviceProperty*> > RteDevicePropertyMap;

/**
 * @brief Base class to describe device-related data: device declaration and their properties
*/
class RteDeviceElement : public RteItem
{

public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteDeviceElement(RteItem* parent) : RteItem(parent) {};

  /**
   * @brief search for RteDeviceItem in the parent chain
   * @return pointer to parent  RteDeviceItem or null if not found
  */
  RteDeviceItem* GetDeviceItemParent() const;

  /**
   * @brief search for RteDeviceElemntItem in the parent chain
   * @return pointer to parent RteDeviceElemntItem or null if not found
  */
  RteDeviceElement* GetDeviceElementParent() const;

  /**
   * @brief search for effectively defined attribute, that is defined in this item or in parent elements
   * @param name attribute name to search
   * @return attribute value or empty string if not found
  */
  virtual const std::string& GetEffectiveAttribute(const std::string& name) const;

  /**
   * @brief check if attribute effectively defined for this item, that is defined in this item or in parent elements
   * @param name attribute name to check
   * @return true if attribute is found in this item or in parent items
  */
  virtual bool HasEffectiveAttribute(const std::string& name) const;

  /**
   * @brief get device vendor string obtained from "Dvendor" attribute effectively defined in this or parent elements
   * @return value for effective "Dvendor" attribute
  */
   const std::string& GetVendorString() const override { return GetEffectiveAttribute("Dvendor"); }

  /**
   * @brief obtain for all effectively defined attributes of this device element. That is collection of all attributes defined in this element and in parent elements.
   * @param attributes reference to XmlItem to fill
  */
  virtual void GetEffectiveAttributes(XmlItem& attributes) const;

protected:
  /**
   * @brief create an RteDeviceProperty for given tag
   * @param tag XML tag
   * @return pointer to RteDeviceProperty-derived class, never null
  */
  virtual RteDeviceProperty* CreateProperty(const std::string& tag);
};

/**
 * @brief Class to describe device property read from pdsc file
*/
class RteDeviceProperty : public RteDeviceElement
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteDeviceProperty(RteItem* parent) : RteDeviceElement(parent) {};

  /**
   * @brief get device property type as string, default uses item's tag
   * @return property type as string
  */
  const std::string& GetPropetryType() const { return GetTag(); }

  /**
   * @brief check if this property is unique, i.e. can be defined only once for device/processor
   * @return true if unique (default)
  */
  virtual bool IsUnique() const { return true; }

  /**
   * @brief obtain all effective attributes of this device property combined with corresponding properties of parent device elements. Own attributes always take precedence.
   * @param attributes reference to XmlItem to fill with values
  */
   void GetEffectiveAttributes(XmlItem& attributes) const override;

  /**
   * @brief add data from supplied device property to this one, default only adds attributes
   * @param p pointer to RteDeviceProperty co collect data from
  */
  virtual void CollectEffectiveContent(RteDeviceProperty* p);

  /**
   * @brief check if child properties should be collected from this property to contribute to "effective content" :
   all properties defined in this item plus combined with those in parent device elements
   * @return true if sub-properties should be collected, default is false
   */
  virtual bool IsCollectEffectiveContent() const { return false; }

  /**
   * @brief get collected effective content:
   * list of all properties defined in this item plus combined with those in parent device elements.
   * Own properties always take precedence.
   * @return list of device properties that represent effective content of this item
  */
  virtual const std::list<RteDeviceProperty*>& GetEffectiveContent() const;

  /**
   * @brief find first effective property with given tag
   * @param tag property tag to search for
   * @return pointer to RteDeviceProperty or nullptr if no effective property is found
  */
  virtual RteDeviceProperty* GetEffectiveContentProperty(const std::string& tag) const;

  /**
   * @brief calculate values to be cached for effective properties
  */
  virtual void CalculateCachedValues() {/* default does nothing */};

protected:
  /**
   * @brief construct device property ID
   * @return device property ID string
  */
   std::string ConstructID() override;

public:
  /**
   * @brief helper function to obtain a RteDeviceProperty for given ID from supplied list
   * @param id device property ID
   * @param properties list of RteDeviceProperty* items
   * @return pointer RteDeviceProperty if found, nullptr otherwise
  */
  static RteDeviceProperty* GetPropertyFromList(const std::string& id, const std::list<RteDeviceProperty*>& properties);

  /**
   * @brief helper function to obtain a RteDeviceProperty for given ID from supplied RteDevicePropertyMap
   * @param tag device property tag string
   * @param id device property ID
   * @param properties RteDevicePropertyMap
   * @return pointer RteDeviceProperty if found, nullptr otherwise
  */
  static RteDeviceProperty* GetPropertyFromMap(const std::string& tag, const std::string& id, const RteDevicePropertyMap& properties);
};

/**
 * @brief class to represent a group of RteDeviceProperty items
*/
class RteDevicePropertyGroup : public RteDeviceProperty
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteDevicePropertyGroup(RteItem* parent, bool bOwnChldren = true);


  /**
   * @brief virtual destructor
  */
  ~RteDevicePropertyGroup() override;

  /**
   * @brief clear internal item structure including children. The method is called from destructor
  */
  void Clear() override;

  /**
   * @brief get immediate RteDeviceProperty child with given ID
   * @param id device property ID
   * @return pointer to child RteDeviceProperty if found, nullptr otherwise
  */
  RteDeviceProperty* GetProperty(const std::string& id) const;

  /**
   * @brief get effective tag content : list of own RteDeviceProperty children and those from device items in parent chain
   * @return list of RteDeviceProperty* items
  */
   const std::list<RteDeviceProperty*>& GetEffectiveContent() const override { return m_effectiveContent; }

  /**
   * @brief collect effective content:  list of own RteDeviceProperty children and those from device items in parent chain
   * @param p RteDeviceProperty to collect content from
  */
   void CollectEffectiveContent(RteDeviceProperty* p) override;

  /**
   * @brief append a new child at the end of the list
   * @param child pointer to a new child of template type RteItem
   * @return pointer to the new child
  */
   RteItem* AddChild(RteItem* child) override;

   /**
     * @brief create a new instance of type RteItem
     * @param tag name of tag
     * @return pointer to instance of type RteItem
   */
   RteItem* CreateItem(const std::string& tag) override;

protected:
  // collected effective content
  std::list<RteDeviceProperty*> m_effectiveContent;
  bool m_bOwnChildren;
};

/**
 * @brief class to support <environment> device property
 */
class RteDeviceEnvironment : public RteDevicePropertyGroup
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteDeviceEnvironment(RteItem* parent) : RteDevicePropertyGroup(parent) {};
public:
  /**
   * @brief indicate that this property type provides effective content to collect
   * @return true
  */
   bool IsCollectEffectiveContent() const override { return true; }

};

/**
 * @brief class to support <book> device property
 */
class RteDeviceBook : public RteDeviceProperty
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteDeviceBook(RteItem* parent) : RteDeviceProperty(parent) {};
};

/**
 * @brief class to support <description> device property
 */
class RteDeviceDescription : public RteDeviceProperty
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteDeviceDescription(RteItem* parent) : RteDeviceProperty(parent) {};

  /**
   * @brief indicate the property is not unique, a device can have several <description> elements
   * @return false
  */
   bool IsUnique() const override { return false; }
};

/**
 * @brief class to support <feature> device property
 */
class RteDeviceFeature : public RteDeviceProperty
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteDeviceFeature(RteItem* parent) : RteDeviceProperty(parent) {};
protected:
  /**
   * @brief indicate the property is not unique, a device can have several <feature> elements
   * @return false
  */
   bool IsUnique() const override { return false; }
};


/**
 * @brief class to support <memory> device property
 */
class RteDeviceMemory : public RteDeviceProperty
{
public:
  /**
  * @brief constructor
  * @param parent pointer to parent RteItem
 */
  RteDeviceMemory(RteItem* parent) : RteDeviceProperty(parent) {};
public:
  /**
   * @brief get memory name
   * @return memory name string
  */
   const std::string& GetName() const override {
    if (HasAttribute("id"))
      return GetAttribute("id");
    return RteDeviceProperty::GetName();
  }
};

/**
 * @brief class to support <algorithm> device property
 */
class RteDeviceAlgorithm : public RteDeviceProperty
{
public:
  /**
  * @brief constructor
  * @param parent pointer to parent RteItem
  */
  RteDeviceAlgorithm(RteItem* parent) : RteDeviceProperty(parent) {};
};


class RteDeviceProcessor : public RteDeviceProperty
{
public:
 /**
  * @brief constructor
  * @param parent pointer to parent RteItem
 */
  RteDeviceProcessor(RteItem* parent) : RteDeviceProperty(parent) {};
public:

  /**
   * @brief get processor property name
   * @return processor name
  */
   const std::string& GetName() const override { return GetProcessorName(); }
};

/**
 * @brief class to support <control> element inside sequence
*/
class RteSequenceControlBlock : public RteDevicePropertyGroup
{
public:
  /**
  * @brief get processor property name
  * @return processor name
 */
  RteSequenceControlBlock(RteItem* parent) : RteDevicePropertyGroup(parent) {};
public:
  /**
    * @brief indicate the property is not unique, a sequence can have several <control> elements
    * @return false
   */
   bool IsUnique() const override { return false; }

protected:
  /**
   * @brief create an RteDeviceProperty for given tag
   * @param tag XML tag
   * @return pointer to RteDeviceProperty-derived class, never null
  */
   RteDeviceProperty* CreateProperty(const std::string& tag) override;
};

/**
 * @brief class to support <block> element inside sequence
*/
class RteSequenceCommandBlock : public RteDeviceProperty
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteSequenceCommandBlock(RteItem* parent) : RteDeviceProperty(parent) {};
public:
  /**
    * @brief indicate the property is not unique, a sequence can have several <block> elements
    * @return false
   */
   bool IsUnique() const override { return false; }
};


/**
 * @brief class to support <sequence> device property
*/
class RteSequence : public RteDevicePropertyGroup
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteSequence(RteItem* parent) : RteDevicePropertyGroup(parent) {};
public:
  /**
    * @brief indicate the property is unique, a sequence with the same name can be listed only once per device
    * @return true
   */
   bool IsUnique() const override { return true; }
protected:
  /**
   * @brief create an RteDeviceProperty for given tag
   * @param tag XML tag
   * @return pointer to RteDeviceProperty-derived class, never null
  */
   RteDeviceProperty* CreateProperty(const std::string& tag) override;
};

/**
 * @brief class to support <datapatch> device property
*/
class RteDatapatch : public RteDeviceProperty
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteDatapatch(RteItem* parent) : RteDeviceProperty(parent), m_hasDP(false), m_hasAP(false), m_hasAPID(false) {};
public:
  /**
    * @brief indicate the property is not unique, a device can contain several <datapatch> elements
    * @return false
   */
   bool IsUnique() const override { return false; }

  /**
   * @brief get datapatch element name, constructed from "address" attribute
   * @return "address" attribute value
  */
   const std::string& GetName() const override { return GetAttribute("address"); }

  /**
   * @brief check if datapatch property has effective "__dp" attribute
   * @return true if attribute "__dp" is effectively set
  */
  virtual bool HasDP() const { return m_hasDP; }
  /**
   * @brief check if datapatch property has effective "__ap" attribute
   * @return true if attribute "__ap" is effectively set
  */
  virtual bool HasAP() const { return m_hasAP; }

  /**
   * @brief check if datapatch property has effective "__dp" attribute
   * @return true if attribute "__apid" is effectively set
  */
  virtual bool HasAPID() const { return m_hasAPID; }
protected:

  /**
   * @brief construct unique datapatch ID
   * @return ID string
  */
   std::string ConstructID() override;
protected:
  bool m_hasDP;
  bool m_hasAP;
  bool m_hasAPID;
};

/**
 * @brief class to support <debugconfig> device property
*/
class RteDebugConfig : public RteDeviceProperty
{
public:
/**
 * @brief constructor
 * @param parent pointer to parent RteItem
*/
  RteDebugConfig(RteItem* parent) : RteDeviceProperty(parent) {};
};

/**
 * @brief class to support <jtag> debug port property
*/
class RteDebugPortJtag : public RteDeviceProperty
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteDebugPortJtag(RteItem* parent) : RteDeviceProperty(parent) {};
};

/**
 * @brief class to support <swd> debug port property
*/
class RteDebugPortSwd : public RteDeviceProperty
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteDebugPortSwd(RteItem* parent) : RteDeviceProperty(parent) {};
};

/**
 * @brief class to support <debugport> device property
*/
class RteDebugPort : public RteDevicePropertyGroup
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteDebugPort(RteItem* parent) : RteDevicePropertyGroup(parent) {};
public:
  /**
   * @brief get debug port name
   * @return "__dp" attribute value
  */
   const std::string& GetName() const override { return GetAttribute("__dp"); }
protected:

  /**
   * @brief create an RteDeviceProperty for given tag
   * @param tag XML tag
   * @return pointer to RteDeviceProperty-derived class, never null
  */
   RteDeviceProperty* CreateProperty(const std::string& tag) override;
};


/**
 * @brief base class to support <accessport> device property
*/
class RteAccessPort : public RteDeviceProperty
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteAccessPort(RteItem* parent, unsigned char apVersion) : RteDeviceProperty(parent), m_apVersion(apVersion) {};
public:
  /**
   * @brief get access port name
   * @return "__apid" attribute value
  */
   const std::string& GetName() const override { return GetAttribute("__apid"); }
  /**
   * @brief get access port version
   * @return version string
  */
  virtual unsigned char GetApVersion() { return m_apVersion; }
protected:
  /**
    * @brief construct unique access port ID
    * @return ID string
   */
   std::string ConstructID() override;
protected:
  unsigned char m_apVersion;
};


/**
 * @brief base class to support <accessportV1> device property
*/
class RteAccessPortV1 : public RteAccessPort
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteAccessPortV1(RteItem* parent) : RteAccessPort(parent, 1) {};
};


/**
 * @brief base class to support <accessportV2> device property
*/
class RteAccessPortV2 : public RteAccessPort
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteAccessPortV2(RteItem* parent) : RteAccessPort(parent, 2) {};
};

/**
 * @brief base class to support <debug> device property
*/
class RteDeviceDebug : public RteDevicePropertyGroup
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteDeviceDebug(RteItem* parent) : RteDevicePropertyGroup(parent) {};

  /**
  * @brief check if child properties should be collected from this property to contribute to effective content
  * @return true
  */
   bool IsCollectEffectiveContent() const override { return true; }

protected:
  /**
   * @brief create an RteDeviceProperty for given tag
   * @param tag XML tag
   * @return pointer to RteDeviceProperty-derived class, never null
  */
   RteDeviceProperty* CreateProperty(const std::string& tag) override;

  /**
  * @brief construct unique debug property ID
  * @return ID string
  */
   std::string ConstructID() override { return RteDeviceProperty::ConstructID() + std::string(":") + GetProcessorName(); }
};


/**
 * @brief base class to support <debugvars> device property
*/
class RteDeviceDebugVars : public RteDeviceProperty
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteDeviceDebugVars(RteItem* parent) : RteDeviceProperty(parent) {};

protected:
  /**
   * @brief construct unique debugvars property ID
   * @return ID string
  */
   std::string ConstructID() override { return RteDeviceProperty::ConstructID() + std::string(":") + GetProcessorName(); }
};


/**
 * @brief base class to support <serialware> device property
*/
class RteTraceSerialware : public RteDeviceProperty
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteTraceSerialware(RteItem* parent) : RteDeviceProperty(parent) {};
};

/**
 * @brief base class to support <tracebuffer> device property
*/
class RteTraceBuffer : public RteDeviceProperty
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteTraceBuffer(RteItem* parent) : RteDeviceProperty(parent) {};
protected:
   std::string ConstructID() override;
};

/**
 * @brief base class to support <traceport> device property
*/
class RteTracePort : public RteDeviceProperty
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteTracePort(RteItem* parent) : RteDeviceProperty(parent) {};

  /**
   * @brief get trace port name
   * @return "width" attribute value
  */
   const std::string& GetName() const override { return GetAttribute("width"); }
};

/**
 * @brief class to support <trace> device property
*/
class RteDeviceTrace : public RteDevicePropertyGroup
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteDeviceTrace(RteItem* parent) : RteDevicePropertyGroup(parent) {};

  /**
   * @brief check if child properties should be collected from this property
   * @return true
   */
   bool IsCollectEffectiveContent() const override { return true; }
protected:
  /**
   * @brief create an RteDeviceProperty for given tag
   * @param tag XML tag
   * @return pointer to RteDeviceProperty-derived class, never null
  */
   RteDeviceProperty* CreateProperty(const std::string& tag) override;
};

class RteFlashInfo;
/**
 * @brief class to support <block> flashinfo property
*/
class RteFlashInfoBlock : public RteDeviceProperty
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteFlashInfoBlock(RteItem* parent);

public:
  /**
   * @brief check if this item is a gap
   * @return true if tag equals to "gap"
  */
  bool IsGap() const { return GetTag() == "gap"; }
  /**
   * @brief get pointer to parent RteFlashInfo property
   * @return pointer to parent RteFlashInfo
  */
  RteFlashInfo* GetRteFlashInfo() const;

public:
  /**
   * @brief get base address
   * @return start value
  */
  unsigned long long GetStart() const { return m_start; }
  /**
   * @brief get block size
   * @return size in bytes
  */
  unsigned long long GetSize() const { return m_size; }
  /**
   * @brief get total size value (count * size)
   * @return total size in bytes
  */
  unsigned long long GetTotalSize() const { return m_totalSize; }

  /**
   * @brief get number of subsequent blocks of this flash device with identical properties
   * @return count value
  */
  unsigned long long GetCount() const { return m_count; }

  /**
   * @brief get optional argument to pass to flash operation sequence
   * @return size value
  */
  unsigned long long GetArg() const { return m_arg; }

  /**
   * @brief calculate and cache block values
   * @param previous pointer to previous RteFlashInfoBlock if any
  */
  void CalculateCachedValuesForBlock(RteFlashInfoBlock* previous);

protected:
  unsigned long long m_start;
  unsigned long long m_size;
  unsigned long long m_totalSize;
  unsigned long long m_count;
  unsigned long long m_arg;

};


/**
 * @brief class to support <flashinfo> device property
*/
class RteFlashInfo : public RteDevicePropertyGroup
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteFlashInfo(RteItem* parent);

public:
  /**
   * @brief get list of child blocks
   * @return list of pointers to child RteFlashInfoBlock elements
  */
  const std::list<RteFlashInfoBlock* >& GetBlocks() const { return m_blocks; }
  /**
   * @brief get base address of the specified flash device as mapped into target memory system
   * @return start address
  */
  unsigned long long GetStart() const { return GetAttributeAsULL("start", 0L); }
  /**
   * @brief get programming page size
   * @return page size in bytes
  */
  unsigned long long GetPageSize() const { return GetAttributeAsULL("pagesize", 0L); }
  /**
   * @brief get expected memory value for unprogrammed address ranges (64-bit value).
   * @return 64-bit value
  */
  unsigned long long GetBlankVal() const { return GetAttributeAsULL("blankval", 0xFFFFFFFFFFFFFFFFL); }
  /**
   * @brief get value that a debugger uses to fill the remainder of a programming page
   * @return 64-bit value
  */
  unsigned long long GetFiller() const { return GetAttributeAsULL("filler", 0xFFFFFFFFFFFFFFFFL); }
  /**
   * @brief get for programming a page
   * @return timeout value in milliseconds
  */
  unsigned GetProgrammingTimeout() const { return GetAttributeAsUnsigned("ptime", 100000); }
  /**
   * @brief get timeout for erasing a sector.
   * @return timeout value in milliseconds
  */
  unsigned GetErasingTimeout() const { return GetAttributeAsUnsigned("etime", 300000); }

protected:
  /**
   * @brief create an RteDeviceProperty for given tag
   * @param tag XML tag
   * @return pointer to RteDeviceProperty-derived class, never null
  */
   RteDeviceProperty* CreateProperty(const std::string& tag) override;
  /**
   * @brief iterate over RteFlashInfoBlock children to calculate and cache their values
  */
   void CalculateCachedValues() override;

protected:
  // typed collection of child blocks
  std::list<RteFlashInfoBlock* >m_blocks;
};


/**
 * @brief helper structure to store effective properties, i.e. properties collected from this RteDeviceElement and parent elements.
*/
struct RteEffectiveProperties
{
  /**
   * @brief return list of properties for given tag
   * @param tag property tag
   * @return list of RteDeviceProperty pointers
  */
  const std::list<RteDeviceProperty*>& GetProperties(const std::string& tag) const;

  /**
   * @brief full property collection: map of tag to list of RteDeviceProperty pointers
  */
  RteDevicePropertyMap m_propertyMap;
};



class RteDevice;

/**
 * @brief base class to describe device hierarchy items: family/sub-family/device/variant
*/
class RteDeviceItem : public RteDeviceElement
{
public:
  /**
   * @brief hierarchy type in device description
  */
  enum TYPE { VENDOR_LIST, VENDOR, FAMILY, SUBFAMILY, DEVICE, VARIANT, PROCESSOR };

public:

  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteDeviceItem(RteItem* parent);
  ~RteDeviceItem() override;

public:
  /**
   * @brief clear internal item structure including children. The method is called from destructor
  */
   void Clear() override;

   /**
    * @brief validate internal item structure and children recursively and set internal validity flag
    * @return validation result as boolean value
   */
   bool Validate() override;

   /**
    * @brief create a new instance of type RteItem
    * @param tag name of tag
    * @return pointer to instance of type RteItem
   */
   RteItem* CreateItem(const std::string& tag) override;

   /**
     * @brief called to construct the item with attributes and child elements
   */
   void Construct() override;

   /**
   * @brief get device item hierarchy type
   * @return RteDeviceItem::TYPE
   */
  virtual TYPE GetType() const = 0;

  /**
   * @brief returns a parent RteDeviceItem of given type
   * @param type RteDeviceItem::TYPE to search for
   * @return pointer to parent RteDeviceItem or nullptr if none is found
  */
  RteDeviceItem* GetDeviceItemParentOfType(RteDeviceItem::TYPE type) const;

  /**
   * @brief get number of processors (cores) in device
   * @return number of processors
  */
  int GetProcessorCount() const;

  /**
   * @brief get collection of processor device properties
   * @return map of processor properties : processor name to RteDeviceProperty pointer pairs
  */
  const std::map<std::string, RteDeviceProperty*>& GetProcessors() const { return m_processors; }

  /**
   * @brief get pointer to RteDeviceProcessor property for given processor name
   * @param pName processor name
   * @return pointer to RteDeviceProperty representing RteDeviceProcessor
  */
  RteDeviceProperty* GetProcessor(const std::string& pName) const;

  /**
   * @brief obtain list of pointers to RteDeviceProcessor properties collected from all levels of device description hierarchy
   * @param processors reference to list of pointers to RteDeviceProperty instances to fill
  */
  void GetEffectiveProcessors(std::list<RteDeviceProperty*>& processors) const;

  /**
   * @brief get parent RteDevice item
   * @return this for RteDevice and parent RteDevice for RteVariant, nullptr otherwise
  */
  virtual RteDevice* GetDevice() const { return nullptr; }

  /**
   * @brief obtain filtered list of child devices in the hierarchy
   * @param devices list of pointers to RteDevice to fill
   * @param searchPattern wild-card search pattern. If empty all devices will be returned
  */
  virtual void GetDevices(std::list<RteDevice*>& devices, const std::string& searchPattern = EMPTY_STRING) const;

  /**
   * @brief get tag of properties of requested tag that are described in this item
   * @param tag name of the tag (property tag)
   * @return pointer to RteDevicePropertyGroup if sound, nullptr otherwise
  */
  virtual RteDevicePropertyGroup* GetProperties(const std::string& tag) const;

  /**
   * @brief get device property specified in this item
   * @param tag property tag
   * @param id property ID
   * @return pointer to RteDeviceProperty if found, nullptr otherwise
  */
  virtual RteDeviceProperty* GetProperty(const std::string& tag, const std::string& id) const;

  /**
   * @brief get all groups of device properties described in this item
   * @return map of device properties: property tag to RteDevicePropertyGroup pointer pairs
  */
  const std::map<std::string, RteDevicePropertyGroup*>& GetProperties() const { return m_properties; }
  virtual void GetProperties(RteDevicePropertyMap& properties) const;


  /**
   * @brief get all effective properties (inherited and overwritten) for given processor name
   * @param pName processor name
   * @return map of device properties: property tag to list of RteDeviceProperty pointers pairs
  */
  const RteDevicePropertyMap& GetEffectiveProperties(const std::string& pName);

  /**
   * @brief get list of all effective properties for given tag and processor name
   * @param tag property tag
   * @param pName processor name
   * @return list of RteDeviceProperty pointers
  */
  const std::list<RteDeviceProperty*>& GetEffectiveProperties(const std::string& tag, const std::string& pName);

  /**
   * @brief search for the first effective property for given tag and processor
   * @param tag property tag
   * @param pName processor name
   * @return pointer to RteDeviceProperty item if found, nullptr otherwise
  */
  RteDeviceProperty* GetSingleEffectiveProperty(const std::string& tag, const std::string& pName);

  /**
   * @brief get list of immediate RteDeviceItem children
   * @return list to RteDeviceItem pointers
  */
  const std::list<RteDeviceItem*>& GetDeviceItems() const { return m_deviceItems; }

  /**
   * @brief return number of RteDeviceItem children
   * @return number of device items as integer
  */
  int GetDeviceItemCount() const { return (int)m_deviceItems.size(); }

  /**
   * @brief obtain list of all RteDeviceItem children down the hierarchy
   * @param deviceItems reference to list of RteDeviceItem pointers to fill
  */
  void GetEffectiveDeviceItems(std::list<RteDeviceItem*>& deviceItems) const;

  /**
   * @brief obtain effective filter attributes  for given processor name
   * @param pName processor name
   * @param attributes reference to XmlItem to fill
  */
  void GetEffectiveFilterAttributes(const std::string& pName, XmlItem& attributes);

  /**
   * @brief create a pdsc-like XMLTreeElement structure for a single device with effective properties
   * @param pname processor name
   * @param parent XMLTreeElement parent item if any
   * @return created XMLTreeElemnt
  */
  XMLTreeElement* CreateEffectiveXmlTree(const std::string& pname = RteUtils::EMPTY_STRING, XMLTreeElement* parent = nullptr);

protected:

  /**
   * @brief convert supplied collection of properties to XMLTreeElement items
   * @param parent pointer to parent XMLTreeElement for created items
   * @param properties list of properties to convert
  */
  void CreateEffectiveXmlTreeElements(XMLTreeElement* parent, const std::list<RteDeviceProperty*>& properties);

  /**
   * @brief create a device item derived from RteDeviceItem class for given tag
   * @param tag device item tag
   * @return pointer to created RteDeviceItem
  */
  RteDeviceItem* CreateDeviceItem(const std::string& tag);

 /**
   * @brief construct item ID
   * @return device item ID
  */
   std::string ConstructID() override;

  /**
   * @brief collect effective properties for a supplied tag (e.g. "debug") and processor (e.g. "core_one")
   * @param tag property tag
   * @param properties reference to list of pointers to RteDeviceProperty to fill
   * @param pName processor name, empty to collect properties for all processors
   * @param bRecursive flag to collect properties recursively from RteDeviceItem parent chain
  */
  void CollectEffectiveProperties(const std::string& tag, std::list<RteDeviceProperty*>& properties, const std::string& pName = EMPTY_STRING, bool bRecursive = true) const;

  /**
   * @brief collect all effective properties for given processor name
   * @param properties reference to RteDevicePropertyMap to fill
   * @param pName processor name, empty to collect properties for all processors
  */
  void CollectEffectiveProperties(RteDevicePropertyMap& properties, const std::string& pName = EMPTY_STRING) const;


  /**
   * @brief fill m_effectiveProperties member by calling via CollectEffectiveProperties(RteDevicePropertyMap&, const std::string&)
   * @param pName processor name, empty to collect properties for all processors
  */
  void CollectEffectiveProperties(const std::string& pName = EMPTY_STRING);

protected:
  std::map<std::string, RteDeviceProperty*> m_processors; // processor properties
  std::map<std::string, RteDevicePropertyGroup*> m_properties; // features, algorithms, etc. grouped by tags
  std::map<std::string, RteEffectiveProperties> m_effectiveProperties; // features, algorithms, etc. grouped by tags key: processor name
  std::list<RteDeviceItem*> m_deviceItems; // sub-items: devices in subFamily, subFamilies in family, families in top container
};

/**
 * @brief class to support <device> element in device description hierarchy
*/
class RteDevice : public RteDeviceItem
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteDeviceItem
  */
  RteDevice(RteDeviceItem* parent) : RteDeviceItem(parent) {};

public:
  /**
   * @brief get device item hierarchy type
   * @return RteDeviceItem::DEVICE
  */
   TYPE GetType() const override { return RteDeviceItem::DEVICE; }

  /**
   * @brief get this RteDevice or parent of type RteDevice
   * @return this
  */
   RteDevice* GetDevice() const override;

  /**
   * @brief collect RteDevice children with names matching given wild-card pattern
   * @param devices list RteDevice pointers to fill
   * @param searchPattern wild-card pattern, if empty all RteDevic children are collected
  */
   void GetDevices(std::list<RteDevice*>& devices, const std::string& searchPattern = EMPTY_STRING) const override;

  /**
   * @brief get device name
   * @return "Dname" attribute value
  */
   const std::string& GetName() const override { return GetAttribute("Dname"); }
};


/**
 * @brief class to support <variant> element in device description hierarchy
*/
class RteDeviceVariant : public RteDevice
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteDeviceItem
  */
  RteDeviceVariant(RteDeviceItem* parent) : RteDevice(parent) {};
public:
  /**
   * @brief get device item hierarchy type
   * @return RteDeviceItem::VARIANT
  */
   TYPE GetType() const override { return RteDeviceItem::VARIANT; }

  /**
   * @brief get pointer to RteDevice parent
   * @return pointer to RteDevice parent, never nullptr for schema-conform CMSIS packs
  */
   RteDevice* GetDevice() const override;
  /**
   * @brief get device variant name
   * @return "Dvariant" attribute value
  */
   const std::string& GetName() const override { return GetAttribute("Dvariant"); }
};


/**
 * @brief class to support <subFamily> element in device description hierarchy
*/
class RteDeviceSubFamily : public RteDeviceItem
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteDeviceItem
  */
  RteDeviceSubFamily(RteDeviceItem* parent) : RteDeviceItem(parent) {};
public:

  /**
   * @brief get device item hierarchy type
   * @return RteDeviceItem::SUBFAMILY
  */
   TYPE GetType() const override { return RteDeviceItem::SUBFAMILY; }
  /**
   * @brief get device subfamily name
   * @return "DsubFamily" attribute value
  */
   const std::string& GetName() const override { return GetAttribute("DsubFamily"); }
};


/**
 * @brief class to support <subFamily> element in device description hierarchy
*/
class RteDeviceFamily : public RteDeviceItem
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteDeviceFamily(RteItem* parent) : RteDeviceItem(parent) {};
public:
  /**
   * @brief get device item hierarchy type
   * @return RteDeviceItem::FAMILY
  */
   TYPE GetType() const override { return FAMILY; }
  /**
   * @brief get device family name
   * @return "Dfamily" attribute value
  */
   const std::string& GetName() const override { return GetAttribute("Dfamily"); }
};

/**
 * @brief class to support <devices> element in CMSIS pack description
*/
class RteDeviceFamilyContainer : public RteItem
{
public:
  /**
   * @brief constructor
   * @param parent pointer to parent RteItem
  */
  RteDeviceFamilyContainer(RteItem* parent);
public:

  /**
   * @brief create a new instance of type RteItem
   * @param tag name of tag
   * @return pointer to instance of type RteItem
  */
  RteItem* CreateItem(const std::string& tag) override;
};

/**
 * @brief map collection to store RteDeviceItem pointers ordered by ID of their packs (newer to older)
*/
typedef std::map<std::string, RteDeviceItem*, RtePackageComparator > RteDeviceItemMap;

class RteDeviceItemAggregate;

/**
 * @brief map collection of RteDeviceItemAggregate pointers ordered alpha-numerically by their names
*/
typedef std::map<std::string, RteDeviceItemAggregate*, AlnumCmp::LenLessNoCase > RteDeviceItemAggregateMap;

/**
* @brief class device to aggregate device items CMSIS packs to single vendor/family/subfamily/device/variant hierarchy
*/
class RteDeviceItemAggregate
{
public:
  /**
   * @brief constructor
   * @param name device aggregate name
   * @param type device item hierarchy type
   * @param parent pointer to parent RteDeviceItemAggregate if any
  */
  RteDeviceItemAggregate(const std::string& name, RteDeviceItem::TYPE type, RteDeviceItemAggregate* parent = nullptr);
  /**
   * @brief  virtual destructor
  */
  virtual ~RteDeviceItemAggregate();

  /**
   * @brief get pointer to parent RteDeviceItemAggregate
   * @return pointer to parent RteDeviceItemAggregate or null if this aggregate has no parent
  */
  RteDeviceItemAggregate* GetParent() const { return m_parent; }

  void Clear();

public:
  /**
   * @brief get device item hierarchy type
   * @return RteDeviceItem::TYPE
  */
  RteDeviceItem::TYPE GetType() const { return m_type; }
  /**
   * @brief get device aggregate name
   * @return name string
  */
  const std::string& GetName() const { return m_name; }

  /**
   * @brief find an immediate RteDeviceItemAggregate child with given name
   * @param name child name
   * @return pointer to RteDeviceItemAggregate or nullptr if not found
  */
  RteDeviceItemAggregate* GetDeviceAggregate(const std::string& name) const;

  /**
   * @brief find recursively a device aggregate with given device name and vendor
   * @param deviceName device name
   * @param vendor device vendor name
   * @return pointer to RteDeviceItemAggregate of type DEVICE, VARIANT or PROCESSOR, nullptr if not found
  */
  RteDeviceItemAggregate* GetDeviceAggregate(const std::string& deviceName, const std::string& vendor) const;

    /**
   * @brief find recursively a device aggregate with given device name and vendor
   * @param deviceName device name
   * @param vendor device vendor name
   * @return pointer to RteDeviceItemAggregate of any type, nullptr if not found
  */
  RteDeviceItemAggregate* GetDeviceItemAggregate(const std::string& name, const std::string& vendor) const;

  /**
   * @brief get pointer to the first RteDeviceItem stored in aggregate (comes from the newest pack)
   * @return pointer to RteDeviceItem;
  */
  RteDeviceItem* GetDeviceItem() const;
  /**
   * @brief get all stored RteDeviceItem pointers
   * @return RteDeviceMap
  */
  const RteDeviceItemMap& GetAllDeviceItems() const { return m_deviceItems; }

  /**
   * @brief recursively search for a device item with given name and vendor
   * @param deviceName device name
   * @param vendor device vendor
   * @return pointer to RteDeviceItem representing RteDevice or RteDeviceVariant, nullptr if not found
  */
  RteDeviceItem* GetDeviceItem(const std::string& deviceName, const std::string& vendor) const;

  /**
   * @brief recursively collect devices with name matching wild-card pattern. Only devices of the requested recursion depth are collected
   * @param devices list of RteDevice to fill
   * @param namePattern wild-card name pattern
   * @param vendor device vendor name
   * @param depth recursion depth, default RteDeviceItem::DEVICE
  */
  void GetDevices(std::list<RteDevice*>& devices, const std::string& namePattern, const std::string& vendor,
                  RteDeviceItem::TYPE depth = RteDeviceItem::DEVICE) const;

  /**
   * @brief add device item to aggregate
   * @param item pointer to RteDeviceItem
  */
  void AddDeviceItem(RteDeviceItem* item);
  /**
   * @brief get RteDeviceItemAggregate children
   * @return RteDeviceItemAggregateMap
  */
  const RteDeviceItemAggregateMap& GetChildren() const { return m_children; }
  /**
   * @brief get number of RteDeviceItemAggregate children
   * @return number of RteDeviceItemAggregate children as integer
  */
  size_t GetChildCount() const { return m_children.size(); }

  /**
   * @brief get number of RteDeviceItemAggregate children of requested hierarchy type
   * @param type RteDeviceItem::TYPE
   * @return  number of RteDeviceItemAggregate children as integer
  */
  size_t GetChildCount(RteDeviceItem::TYPE type) const;
  /**
   * @brief create short device item description
   * @return summary string
  */
  std::string GetSummaryString() const;
  /**
   * @brief check if this device aggregate is marked as deprecated, because it contains deprecated device items
   * @return true if deprecated
  */
  bool IsDeprecated() const { return m_bDeprecated; }

private:
  static std::string GetMemorySizeString(unsigned int size);
  static std::string GetScaledClockFrequency(const std::string& dclock);

  std::string m_name;
  RteDeviceItem::TYPE m_type;
  bool m_bDeprecated; // Mark device item aggregate as deprecated

  RteDeviceItemAggregate* m_parent;
  RteDeviceItemMap m_deviceItems; // the original device items in the aggregate
  RteDeviceItemAggregateMap m_children; // child aggregates
};

/**
 * @brief class to collect a flat list of all devices belonging to a device vendor
*/
class RteDeviceVendor
{
public:
  /**
   * @brief constructor
   * @param name device vendor name
  */
  RteDeviceVendor(const std::string& name);
  virtual ~RteDeviceVendor();

  /**
   * @brief clear internal data
  */
  void Clear();

public:
  /**
   * @brief get vendor name
   * @return name string
  */
  const std::string& GetName() const { return m_name; }

  /**
   * @brief add RteDevice children from supplied RteDeviceItem parent
   * @param item pointer to RteDeviceItem to add devices from
   * @return true if at least one RteDevice item is inserted
  */
  bool AddDeviceItem(RteDeviceItem* item);

  /**
   * @brief check if this vendor item contains device with specified name
   * @param deviceName device name to search for
   * @return true if found
  */
  bool HasDevice(const std::string& deviceName) const;
  /**
   * @brief find device with given name
   * @param deviceName device name to search for
   * @return pointer to RteDevice or RteDeviceVariant if found, nullptr otherwise
  */
  RteDevice* GetDevice(const std::string& deviceName) const;
  /**
   * @brief collect devices matching name pattern
   * @param devices list of RteDevice pointers to fill
   * @param namePattern wild-card name pattern
  */
  void GetDevices(std::list<RteDevice*>& devices, const std::string& namePattern) const;

  /**
   * @brief get collection of stored devices
   * @return map of device name to RteDevice pointer pairs
  */
  const std::map<std::string, RteDevice*>& GetDevices() const { return m_devices; }
  /**
   * @brief get number of stored devices
   * @return number of collected devices
  */
  int GetCount() const { return (int)m_devices.size(); }

protected:
  /**
   * @brief add RteDevice to collection
   * @param item pointer to RteDevice
   * @return true if added
  */
  bool AddDevice(RteDevice* item);

private:
  std::string m_name; // vendor name
  std::map<std::string, RteDevice*> m_devices; // unique map of the original devices from packs
};

#endif // RteDevice_H
