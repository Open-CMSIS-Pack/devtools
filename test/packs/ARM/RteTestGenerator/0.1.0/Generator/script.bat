@echo off
echo %1 
echo %2 
echo %3
echo %4
echo %5

if "%5" == "--dry-run" (
  echo -----BEGIN GPDSC-----
  type "%3\Templates\RteTest.gpdsc.template"
  echo -----END GPDSC-----
) else (
  mkdir %1
  copy "%3\Templates\RteTest.gpdsc.template" "%1\RteTest.gpdsc"
  copy "%3\Templates\RteTest_Generated_Component.c.template" "%1\RteTest_Generated_Component.c"
)
