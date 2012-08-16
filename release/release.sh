#!/bin/bash

rm -R aush
mkdir -p aush
cp ../vc10/Release/aush.exe aush
cp ../README.org aush
cp ../elisp/aush.el aush
zip aush.zip -r aush
