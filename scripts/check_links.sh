#!/bin/bash

linkchecker -F csv --timeout 3 --check-extern $1

OFS=$IFS
IFS=$'\n'

echo "# converted output from linkchecker" > linkchecker-in.csv
echo "link;file;msg;src" >> linkchecker-in.csv
for line in $(grep -E '^[^#]' linkchecker-out.csv | tail -n +2); do
  link=$(echo $line | cut -d';' -f 1)
  file=$(echo $line | cut -d';' -f 2)
  msg=$(echo $line | cut -d';' -f 4)
  src=$(echo $file | sed -E 's/file:\/\/(.*)\/doc\/.*/\1\/doxygen\/src/')
  echo "${link};${file};${msg};${src}" >> linkchecker-in.csv
  if [ -d $src ]; then
    origin=$(grep -Ern "href=['\"]${link}['\"]" $src)
    for o in $origin; do
      ofile=$(echo $o | cut -d':' -f 1)
      oline=$(echo $o | cut -d':' -f 2)
      echo "${ofile}:${oline};${link};${msg}" >&2
    done
  fi
done

IFS=$OFS

exit 0
