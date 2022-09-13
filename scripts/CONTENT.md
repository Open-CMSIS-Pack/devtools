# NextMDK Development Tools and Libraries

## Scripts and Helpers

This folder contains a collection of helper scripts for recurring activities.

### groovylint.sh

This script requires network connection to a Jenkins instance. The server
and credentials used to connect are configured inside the script. The script
should be copied into some bin folder (e.g. ~/bin) which is on the search
PATH. Adopt the server URL and user credentials.

Now one can validate Jenkins groovy Pipelines (i.e. Jenkinsfile's) like this:

```sh
$ ./groovylint.sh Jenkinsfile
Jenkinsfile failed with:
Errors encountered validating Jenkinsfile:
WorkflowScript: 2: Not a valid section definition: "agent". Some extra configuration is required. @ line 2, column 3.
     agent
     ^

WorkflowScript: 1: Missing required section "agent" @ line 1, column 1.
   pipeline {
   ^
```

### pre-commit

This is a pre-commit hook script to be placed into a Git clones `.git/hooks`
folder. It is automatically executed on each `git commit` one issues (either
on command line or from a Git GUI of choice).

The hook fails and prevents the commit from being completed if touched files
don't pass the following checks

- no trailing whitespace
- empty line at the end of a file
- jenkins/\*.groovy files pass [groovylint.sh]
