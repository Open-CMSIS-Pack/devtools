#!/bin/bash
echo $1 
echo $2 
echo $3
echo $4

mkdir $1
cp "$3/Templates/RteTest.gpdsc.template" "$1/RteTest.gpdsc"
