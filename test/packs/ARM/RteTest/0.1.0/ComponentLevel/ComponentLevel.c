#ifdef LOCAL_PRE_INCLUDE
  #warning local pre-include seen in module of the component
#else
  #error local pre-include not seen in module of the component
#endif

#ifdef GLOBAL_PRE_INCLUDE
  #warning info global pre-include seen in non component module
#else
  #error global pre-include not seen by component
#endif

#ifdef FILE_COMPONENT_LEVEL_CONFIG_0
#warning Good Config component level instance 0
  #ifdef FILE_COMPONENT_LEVEL_CONFIG_1
    #warning Good Config component level instance 1
  #endif
#else
  #error Bad Config component level
#endif


int ComponentLevel(int i) {
    return 1;
}
