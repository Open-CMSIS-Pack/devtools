#!/bin/bash

# -------------------------------------------------------
# Copyright (c) 2020-2021 Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
# -------------------------------------------------------

# This bash script generates CMSIS Build instructions
#
# Pre-requisites:
# - bash shell (for Windows: install git for Windows)
# - source "${CMSIS_BUILD_ROOT}/etc/setup"

version=

# header
header() {
  echo "($(basename "$0")): Build Invocation $version (C) 2021 ARM" >&${out}
}

# usage
usage() {
  echo "Usage:"
  echo "  cbuild.sh <ProjectFile>.cprj"
  echo "  [--toolchain=<Toolchain> --outdir=<OutDir> --intdir=<IntDir> <CMakeTarget>]"
  echo ""
  echo "  <ProjectFile>.cprj      : CMSIS Project Description input file"
  echo "  --toolchain=<Toolchain> : select the toolchain"
  echo "  --intdir=<IntDir>       : set intermediate directory"
  echo "  --outdir=<OutDir>       : set output directory"
  echo "  --quiet                 : suppress output messages except build invocations"
  echo "  --clean                 : remove intermediate and output directories"
  echo "  --update=<CprjFile>     : generate <CprjFile> for reproducing current build"
  echo "  --help                  : launch documentation and exit"
  echo "  --log=<LogFile>         : save output messages in a log file"
  echo "  --jobs=<N>              : number of job slots for parallel execution"
  echo "  --cmake[=<BuildSystem>] : select build system, default <BuildSystem>=Ninja"
  echo "  <CMakeTarget>           : optional CMake target name"
}

# silent pushd popd
pushd () {
  command pushd "$@" > /dev/null
  if [ $? -ne 0 ]; then
    echo "error: pushd $@ failed" >&${err}
    exit 1
  fi
}

popd () {
  command popd "$@" > /dev/null
  if [ $? -ne 0 ]; then
    echo "error: popd $@ failed" >&${err}
    exit 1
  fi
}

# Gets the absolute path to a given file
get_abs_filename() {
  echo "$(cd "$(dirname "$@")" && pwd)/$(basename "$@")"
}

# Expand user directory
expand_user() {
  if [ ${@:0:1} = '~' ]
    then
    eval echo "$@"
  else
    echo "$@"
  fi
}

launch_doc () {
  scriptdir="$(dirname "$0")"
  pushd "$scriptdir/../doc/html"
  OS=$(uname -s)
  case $OS in
    'Linux')
      xdg-open ./cbuild.html
      ;;
    'WindowsNT'|MINGW*|CYGWIN*)
      start cbuild.html
      ;;
    'Darwin')
      echo "Docs unavailable for Mac at present"
      ;;
    *)
      echo "Docs unavailable: unrecognised OS $OS"
      ;;
  esac
  popd
}

get_proc () {
  OS=$(uname -s)
  case $OS in
    'Linux')
      proc=$(nproc)
      ;;
    'WindowsNT'|MINGW*|CYGWIN*)
      proc=$NUMBER_OF_PROCESSORS
      ;;
    'Darwin')
      proc=$(sysctl -n hw.logicalcpu)
      ;;
    *)
      proc=1
      ;;
  esac
  if [ -z ${proc} ]
    then
    proc=1
  fi
  echo ${proc}
}

quiet=0
clean=0
out=1
err=2
# arguments
for i in "$@"
do
  case "$i" in
    [!-]*.cprj)
      projectdir=$(dirname "$i")
      projectdir=$(expand_user "${projectdir}")
      project=$(basename "$i" .cprj)
      shift
    ;;
    --toolchain=*)
      toolchain="${i#*=}"
      shift
    ;;
    --outdir=*)
      outdir="${i#*=}"
      outdir=$(expand_user "${outdir}")
      shift
    ;;
    --intdir=*)
      intdir="${i#*=}"
      intdir=$(expand_user "${intdir}")
      shift
    ;;
    --log=*)
      logfile="${i#*=}"
      logfile=$(expand_user "${logfile}")
      shift
    ;;
    --jobs=*)
      jobs="${i#*=}"
      shift
    ;;
    --quiet)
      quiet=1
      shift
    ;;
    --clean)
      clean=1
      shift
    ;;
    --cmake)
      cmake="Ninja"
      shift
    ;;
    --cmake=*)
      cmake="${i#*=}"
      shift
    ;;
    --update=*)
      update="${i#*=}"
      update=$(expand_user "${update}")
      shift
    ;;
    --help)
      header
      launch_doc
      exit 0
    ;;
    ?|-*|--*)
      header
      usage
      exit 0
    ;;
    *)
      m_target=$i
      shift
    ;;
  esac
done

# log file
if [ ! -z ${logfile} ]
then
  exec &> >(tee "${logfile}")
fi

# quiet mode
if [ $quiet -eq 1 ]
  then
  exec 3>/dev/null
  out=3
  err=3
  quiet_opt="--quiet"
  wdev="-Wno-dev"
else
  unset quiet_opt
  wdev="-Wdev"
fi
# print header
header

# check if a project file got specified
if [ -z ${project+x} ]
  then
  echo "error: missing required argument <project>.cprj" >&${err}
  usage
  exit 1
fi

# format projectdir
if [[ "${projectdir}" = '.' ]]
  then
  unset projectdir
else
  projectdir+=/
fi

# check if specified project file exists
if [ ! -e "${projectdir}${project}.cprj" ]
  then
    echo "error: project file ${projectdir}${project}.cprj does not exist." >&${err}
    exit 1
fi

# call xmllint for schema check
type -a xmllint >/dev/null 2>&1
if [ $? -ne 0 ]
  then
  echo "xmllint was not found, proceed without xml validation" >&${err}
else
  if [ -z ${CMSIS_BUILD_ROOT+x} ]
    then
    echo "error: CMSIS_BUILD_ROOT environment variable not set" >&${err}
    echo "CPRJ.xsd was not found, proceed without xml validation" >&${err}
  else
    xmllint --schema "${CMSIS_BUILD_ROOT}/../etc/CPRJ.xsd" "${projectdir}${project}.cprj" --noout >&${out} 2>&${err}
    if [ $? -ne 0 ]
      then
      echo "xmllint schema check failed!"
      exit 1
    fi
  fi
fi

# get directory for intermediate files (parse command line and cprj file, otherwise get default value)
if [ -z "${intdir}" ]
  then
  cprj_intdir=$(echo "$(grep -o 'intdir=.*' "${projectdir}${project}.cprj" | cut -f2 -d\")")
  if [ -z "${cprj_intdir}" ]
    then
    abs_intdir="${projectdir}IntDir/"
  else
    abs_intdir="${projectdir}${cprj_intdir}"
  fi
else
  abs_intdir="${intdir}"
fi
abs_intdir="$(get_abs_filename "${abs_intdir}")"
abs_intdir+=/

# get directory for output files (parse command line and cprj file, otherwise get default value)
if [ -z "${outdir}" ]
  then
  cprj_outdir=$(echo "$(grep -o 'outdir=.*' "${projectdir}${project}.cprj" | cut -f2 -d\")")
  if [ -z "${cprj_outdir}" ]
    then
    abs_outdir="${projectdir}OutDir/"
  else
    abs_outdir="${projectdir}${cprj_outdir}"
  fi
else
  abs_outdir="${outdir}"
fi
abs_outdir="$(get_abs_filename "${abs_outdir}")"
abs_outdir+=/

# remove intermediate and output directories (if they exist)
if [ $clean -eq 1 ]
  then
  if [ -d "${abs_intdir}" ]
    then
    cbuildgen rmdir ${abs_intdir}
    if [ $? -ne 0 ]
      then
      exit 1
    fi
  fi
  if [ -d "${abs_outdir}" ]
    then
    cbuildgen rmdir ${abs_outdir}
    if [ $? -ne 0 ]
      then
      exit 1
    fi
  fi
  echo "cbuild.sh finished successfully!" >&${out}
  exit 0
fi

# call cbuildgen packlist to check for missing packs
# This assumes that the cbuildgen executable is on the path.
rm -f "${abs_intdir}${project}.cpinstall"
cbuildgen packlist "${projectdir}${project}.cprj" --toolchain="${toolchain}" --outdir="${outdir}" --intdir="${intdir}" ${quiet_opt}

if [ $? -ne 0 ]
  then
  exit 1
fi

# call pack installer
if [ -e "${abs_intdir}${project}.cpinstall" ]
  then
  dos2unix "${abs_intdir}${project}.cpinstall" >&${out} 2>&${err}
  cpackget pack add -v --agree-embedded-license --packs-list-filename "${abs_intdir}${project}.cpinstall" >&${out} 2>&${err}
  if [ $? -ne 0 ]
    then
    exit 1
  fi
fi

# number of parallel jobs
if [ -z ${jobs} ]
then
  jobs=$(get_proc)
fi

if [ -z "${cmake}" ]
  then
  cmake="Ninja"
fi

# call cbuildgen to generate CMakeLists.txt
cbuildgen cmake "${projectdir}${project}.cprj" --toolchain="${toolchain}" --update="${update}" --outdir="${outdir}" --intdir="${intdir}" ${quiet_opt}
if [ $? -ne 0 ]
  then
  exit 1
fi
if [ ! -z "${m_target}" ]
  then
  m_target="--target ${m_target}"
fi

# call cmake
cmake -G "${cmake}" -S "${abs_intdir}" -B "${abs_intdir}" ${wdev} >&${out} && cmake --build "${abs_intdir}" -j ${jobs} ${m_target}
if [ $? -ne 0 ]
  then
  echo "cmake ${abs_intdir}CMakeLists.txt failed!" >&${err}
  exit 1
else
  echo "cbuild.sh finished successfully!" >&${out}
  exit 0
fi
