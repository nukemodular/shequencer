#!/bin/bash

FILE="Source/BuildVersion.h"

if [ -f "$FILE" ]; then
    # Extract current build number
    CURRENT_BUILD=$(grep "#define BUILD_NUMBER" "$FILE" | awk '{print $3}')
    
    if [ -z "$CURRENT_BUILD" ]; then
        echo "Could not find BUILD_NUMBER in $FILE"
        exit 1
    fi
    
    NEW_BUILD=$((CURRENT_BUILD + 1))
    
    # Write back to file
    echo "#pragma once" > "$FILE"
    echo "#define BUILD_NUMBER $NEW_BUILD" >> "$FILE"
    
    echo "Incremented build number to $NEW_BUILD"
else
    echo "File $FILE not found!"
    exit 1
fi
