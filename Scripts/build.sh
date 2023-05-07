#!/bin/bash

# List all versions of the project
versions=("version1" "version2" "EC1")

# Build each version
for version in "${versions[@]}"
do
    # Change into the version directory
    cd "$version"
    
    # Build the executable
    g++ -Wall -std=c++20 main.cpp gl_frontEnd.cpp -framework OpenGL -framework GLUT -o travel
    
    # Return to the root directory
    cd ..
done
