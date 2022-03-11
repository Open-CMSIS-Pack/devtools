@echo off
echo %1 
echo %2 
echo %3

mkdir %1
copy "%3\Templates\RteTest.gpdsc.template" "%1\RteTest.gpdsc"
