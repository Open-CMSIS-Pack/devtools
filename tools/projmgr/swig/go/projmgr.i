%module projmgr
%include <typemaps.i>
%include "std_list.i"
%include "std_map.i"
%include "std_string.i"
%include "std_vector.i"

%typemap(gotype) (int argc, char **argv) "[]string"
%typemap(in) (int argc, char **argv)
%{
  {
    int i;
    _gostring_* a;

    $1 = $input.len;
    a = (_gostring_*) $input.array;
    $2 = (char **) malloc (($1 + 1) * sizeof (char *));
    for (i = 0; i < $1; i++) {
      _gostring_ *ps = &a[i];
      $2[i] = (char *) ps->p;
    }
    $2[i] = NULL;
  }
%}
%typemap(argout) (int argc, char **argv) "" /* override char ** default */
%typemap(freearg) (int argc, char **argv)
%{
  free($2);
%}

#ifdef Linux
%insert(cgo_comment_typedefs) %{
#cgo LDFLAGS: ${SRCDIR}/projmgr.so -Wl,-rpath=${SRCDIR}
%}
#endif

namespace std {
  %template(StringVector) vector<string>;
}

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
