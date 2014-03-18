#!/bin/bash

if [[ -d "/Users" && -f "/mach_kernel" ]]; then
	./tools/premake4/premake4.osx xcode3
else
	./tools/premake4/premake4.lnx gmake
fi
