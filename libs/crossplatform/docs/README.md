# What is this?

This repo contains all the platform-specific code that allows the CMSIS-Build
project to be constructed and run on multiple platforms. The intent is to
provide alternative implementations of the Windows-specific features that the
main code uses, and to eliminate the need to include platform-specific
switching inside the main codebase.
