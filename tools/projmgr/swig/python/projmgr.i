%module projmgr
%include "std_list.i"
%include "std_set.i"
%include "std_map.i"
%include "std_string.i"
%include "argcargv.i"

%apply (int ARGC, char **ARGV) { (int argc, char **argv) }

namespace std {
    %template(map_string_string) map<string, string>;
}

%rename(lt) operator<;

%{
  #include "../include/ProjMgr.h"
  #include "../include/ProjMgrWorker.h"
  #include "../include/ProjMgrParser.h"
  #include "../include/ProjMgrGenerator.h"
%}

%include "../include/ProjMgr.h"
%include "../include/ProjMgrWorker.h"
%include "../include/ProjMgrParser.h"
%include "../include/ProjMgrGenerator.h"
