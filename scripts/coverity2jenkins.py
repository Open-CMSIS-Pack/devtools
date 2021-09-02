#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import json
from argparse import ArgumentParser, FileType

def main():
    parser = ArgumentParser()
    parser.add_argument("report", type=open, help="Coverity JSON report.")
    parser.add_argument("-o", "--output", default="coverity.log", type=FileType('w'),
                        help="Coverity Jenkins log.")
    args = parser.parse_args()
    report = json.load(args.report)
    for issue in report['issues']:
        main_event, = (e for e in issue['events'] if e['main'])
        data = {
            'fingerprint': issue['mergeKey'],
            'severity': issue['checkerProperties']['impact'],
            'fileName': issue['mainEventFilePathname'],
            'lineStart': issue['mainEventLineNumber'],
            'category': issue['checkerProperties']['category'],
            'type': issue['type'],
            'message': main_event['eventDescription'].replace("'", '"'),
            'description': issue['checkerProperties']['subcategoryLongDescription'].replace("'", '"')
        }
        args.output.write("{severity} '{category}' {type} {fileName}:{lineStart} '{message}' "
                          "'{description}' ({fingerprint})\n".format(**data))

if __name__ == "__main__":
    main()
