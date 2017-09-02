#!/bin/sh

# List all svn ignored files. Select ignored files. Remove them. Always return true.
svn status --no-ignore 2>/dev/null | grep '^I' | awk '{print $2}' | xargs rm -rf 2>/dev/null

# Always return true
true
