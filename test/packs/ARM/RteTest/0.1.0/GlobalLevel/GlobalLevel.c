
#ifdef LOCAL_PRE_INCLUDE
  #error local pre-include seen in module outside of the component
#else
  #warning  local pre-include not seen in module outside of the component
#endif

#ifdef GLOBAL_PRE_INCLUDE
  #warning global pre-include seen in non component module
#else
  #error global pre-include not seen by component
#endif

int  GlobalLevel(int i) {

  return 1;
}

